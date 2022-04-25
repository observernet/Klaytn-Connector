/*****************************************************
 * 프로그램ID	: ExecTransact.c
 * 프로그램명	: Que에서 데이타를 읽어 Transaction을 처리한다
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <pthread.h>
#include <dirent.h>

#include <profile.h>
#include <ft_common.h>

#include <KASConn_interface.h>
#include "ExecTransact.h"

/*************************************************************************************
 * 구현 시작..
 *************************************************************************************/
 
int main(int argc, char** argv)
{
    int *arg, i;
	char account_group[32];

    DIR* dir_info;
    struct dirent* dir_entry;
    char dirpath[256], strdate[16];
    char is_new;

	SetProgramName(argv[0]);

	/* 서버를 초기화 한다. */
	InitServer();
    sprintf(dirpath, "%s/%s/req/", mdb->program_home, DATA_PATH);
    
	while ( 1 )
    {
        /* 디렉토리를 연다 */
    	if ( (dir_info = opendir(dirpath)) == NULL )
        {
            Log("main: 디렉토리 정보를 가져올수 없습니다 dirpath[%s] errno[%d]\n", dirpath, errno);
            break;
        }

        /* 새로운 파일이 생성되었는지 체크한다 */
        is_new = 0;
        while ( (dir_entry = readdir(dir_info)) )
        {
            sprintf(strdate, "%08d", mdb->system_date);
            if ( strncmp(dir_entry->d_name, strdate, 8) == 0 )
            {
                memset(account_group, 0x00, 32);
                strncpy(account_group, dir_entry->d_name+9, strlen(dir_entry->d_name)-13);
                
                for ( i = 0 ; i < thread_count ; i++ )
                {
                    if ( strcasecmp(thread[i].account_group, account_group) == 0 )
                        break;
                }
                if ( i >= thread_count )
                {
                    is_new = 1;
                    break;
                }
            }
        }
        closedir(dir_info);

        // 큐파일을 감시하고 처리할 쓰레드를 생성한다
        if ( is_new )
        {
            /* 메모리할당 */
            thread = (TH_INFO*)realloc(thread, sizeof(TH_INFO)*(thread_count+1));
            thread[thread_count].date = mdb->system_date;
            strcpy(thread[thread_count].account_group, account_group);
            
            /* 쓰레드생성 */
            arg = (int*)malloc( sizeof(int) ); *arg = thread_count;
            if ( pthread_create(&thread[thread_count].th, NULL, ThreadProcess, (void*)arg) != 0 )
            {
                Log("main: 쓰레드 생성에 실패하였습니다. errno[%d]\n", errno);
                break;
            }
            thread_count++;
        }

        sleep(10);
	}
	
	interrupt(0);
	
	exit(EXIT_SUCCESS);
}

/*************************************************************************************
 * Request Thread Processing
 *************************************************************************************/

void* ThreadProcess(void* arg)
{
	int thread_num;

    FILE* fp;
    char filename[256], readbuf[MAX_PACKET];
    int len, linenum, finish_line;
    int thread_wait_count = 0;

    QDATA qdata;
	
	thread_num = *((int*)arg);
	free(arg);
	
	Log("Thread Start........   date[%d] account_group[%s]\n", thread[thread_num].date, thread[thread_num].account_group);

    /* 이전에 처리한 라인넘버를 가져온다 */
    finish_line = GetReadLineNum(thread[thread_num].date, "req", thread[thread_num].account_group);
	
    /* 파일을 연다 (파일을 체크한 후 생성된거이기 때문에 파일은 무조건 존재) */
    sprintf(filename, "%s/%s/req/%08d.%s.que", mdb->program_home, DATA_PATH, thread[thread_num].date, thread[thread_num].account_group);
    if ( (fp = fopen(filename, "r")) == NULL )
    {
        Log("ThreadProcess[%s]: 파일이 존재하지 않습니다 filename[%s] errno[%d]\n", thread[thread_num].account_group, filename, errno);
        pthread_exit(0);
	    return (NULL);
    }

    linenum = 0;
	while ( !thread_terminate )
	{
		memset(readbuf, 0x00, MAX_PACKET);
        if ( fgets(readbuf, MAX_PACKET, fp) )
        {
            linenum++;
            if ( linenum <= finish_line) continue;

            len = strlen(readbuf);
            if ( readbuf[len-1] == '\n' ) readbuf[len-1] = 0;

            /* 읽어온 데이타를 파싱한다 */
            memset(&qdata, 0x00, sizeof(QDATA));
            sscanf(readbuf, "%ld\t'%[^\']'\t'%[^\']'", &qdata.datakey, (char*)&qdata.header, qdata.data);

            /* 로그기록 */
            Log("ThreadProcess[%s]: [%ld][%s][%s]\n", thread[thread_num].account_group, qdata.datakey, (char*)&qdata.header, qdata.data);

            /* 계좌구분에 따라 */
            switch ( qdata.header.account_type )
            {
                case KI_ACCOUNTYPE_KLAYTN:
                    SendTransactToKlaytn(thread_num, &qdata);
                    break;
                
                case KI_ACCOUNTYPE_KAS:
                    SendTransactToKAS(thread_num, &qdata);
                    break;

                default:
                    Log("ThreadProcess[%s]: 정의되지 않은 계좌구분입니다 linenum[%d] account_type[%c]\n", thread[thread_num].account_group, linenum, qdata.header.account_type);
                    SendErrorMessage(thread[thread_num].date, thread[thread_num].account_group, &qdata, "정의되지 않은 계좌구분");
            }

            /* 로그기록 */
            Log("ThreadProcess[%s]: [%ld][%s] Process Complete\n", thread[thread_num].account_group, qdata.datakey, (char*)&qdata.header);

            /* 라인넘버를 기록한다 */
            WriteReadLineNum(linenum, thread[thread_num].date, "req", thread[thread_num].account_group);
            usleep(THREAD_PER_SLEEP);
        }
        else
        {
            if ( thread[thread_num].date != mdb->system_date )
            {
                thread_wait_count++;
                if ( thread_wait_count > THREAD_WAIT_COUNT ) break;
            }
            usleep(THREAD_PER_SLEEP);
        }
	}
    fclose(fp);
	
	Log("Thread Terminate........   date[%d] account_group[%s]\n", thread[thread_num].date, thread[thread_num].account_group);
	
	pthread_exit(0);
	return (NULL);
}

/**
 * 클레이튼에 트랜잭션을 요청한다
 */
int SendTransactToKlaytn(int thread_num, QDATA* qdata)
{
    FILE* fp;
    char pipename[MAX_BUFFER], *rcvbuf = NULL;

    char tmpbuf[MAX_BUFFER];
    int ptr = 0, len;

    /* NODE에 트랜잭션을 전송을 요청한다 */
    sprintf(pipename, "%s %s/%s %c '%s'", NODE_BASE, mdb->program_home, NODE_INTERFACE_KLAYTN, qdata->header.trid, qdata->data);
    if ( (fp = popen(pipename, "r")) == NULL )
    {
        Log("SendTransactToKlaytn[%s]: pipe open error [%s]\n", thread[thread_num].account_group, pipename);
        SendErrorMessage(thread[thread_num].date, thread[thread_num].account_group, qdata, "Node 실행 에러!!");
        return (-1);
    }

    while ( 1 )
    {
        memset(tmpbuf, 0x00, MAX_BUFFER);
        if ( fgets(tmpbuf, MAX_BUFFER, fp) == NULL ) break;

        len = strlen(tmpbuf);
        rcvbuf = realloc(rcvbuf, ptr + len);
        memcpy(rcvbuf+ptr, tmpbuf, len);
        ptr = ptr + len;
    }
    pclose(fp);

    /* 수신 데이타가 존재한다면 */
    if ( rcvbuf )
    {
		rcvbuf = realloc(rcvbuf, ptr+1);
	    rcvbuf[ptr] = 0;

        len = strlen(rcvbuf);
        if ( rcvbuf[len-1] == '\n' ) rcvbuf[len-1] = 0;

        /* 큐에 응답데이타를 넣는다 */
        InsertResponseQue(thread[thread_num].date, thread[thread_num].account_group, qdata, rcvbuf);
        free(rcvbuf);
    }

    return (0);
}

/**
 * KAS 트랜잭션을 요청한다
 */
int SendTransactToKAS(int thread_num, QDATA* qdata)
{
    FILE* fp;
    char pipename[MAX_BUFFER], *rcvbuf = NULL;

    char tmpbuf[MAX_BUFFER];
    int ptr = 0, len;

    /* NODE에 트랜잭션을 전송을 요청한다 */
    sprintf(pipename, "%s %s/%s %c '%s'", NODE_BASE, mdb->program_home, NODE_INTERFACE_KAS, qdata->header.trid, qdata->data);
    if ( (fp = popen(pipename, "r")) == NULL )
    {
        Log("SendTransactToKAS[%s]: pipe open error [%s]\n", thread[thread_num].account_group, pipename);
        SendErrorMessage(thread[thread_num].date, thread[thread_num].account_group, qdata, "Node 실행 에러!!");
        return (-1);
    }

    while ( 1 )
    {
        memset(tmpbuf, 0x00, MAX_BUFFER);
        if ( fgets(tmpbuf, MAX_BUFFER, fp) == NULL ) break;

        len = strlen(tmpbuf);
        rcvbuf = realloc(rcvbuf, ptr + len);
        memcpy(rcvbuf+ptr, tmpbuf, len);
        ptr = ptr + len;
    }
    pclose(fp);

    /* 수신 데이타가 존재한다면 */
    if ( rcvbuf )
    {
		rcvbuf = realloc(rcvbuf, ptr+1);
	    rcvbuf[ptr] = 0;

        len = strlen(rcvbuf);
        if ( rcvbuf[len-1] == '\n' ) rcvbuf[len-1] = 0;

        /* 큐에 응답데이타를 넣는다 */
        InsertResponseQue(thread[thread_num].date, thread[thread_num].account_group, qdata, rcvbuf);
        free(rcvbuf);
    }

    return (0);
}

/**
 * 응답데이타를 큐에 삽입한다
 */
int InsertResponseQue(int date, char* account_group, QDATA* req, char* res)
{
    FILE* fp;
    char filename[256];

    /* 큐에 데이타를 넣는다 */
    sprintf(filename, "%s/%s/res/%08d.%s.que", mdb->program_home, DATA_PATH, date, account_group);
    if ( (fp = fopen(filename, "a+")) == NULL ) return (-1);
    fprintf(fp, "%ld\t'%-.*s'\t'%s'\n", req->datakey, (int)sizeof(KI_REQRES_HEADER) - KI_BODY_LENGTH, (char*)&req->header, res);
    fclose(fp);

    return (0);
}

/**
 * 에러메세지를 전송한다
 */
int SendErrorMessage(int date, char* account_group, QDATA* req, char* errmsg)
{
    char buff[MAX_BUFFER];

    /* 에레메세지를 응답큐에 넣는다 */
    sprintf(buff, "{\"success\":false,\"msg\":\"%s\"}", errmsg);
    return ( InsertResponseQue(date, account_group, req, buff) );
}

/*************************************************************************************
 * 초기화함수
 *************************************************************************************/

/**
 * 서버를 초기화한다.
 */ 
void InitServer()
{
	/* 공유메모리를 초기화 한다. */
	InitCommonMemory();
	
    /* 메모리초기화 */
    thread_terminate = false;
    thread_count = 0; thread = NULL;

    /* Process를 등록한다. */
	if ( (process_id = RegistProcess(_PROC_EXEC_TRANSACT_)) == -1 )
	{
		Log("InitServer: 프로세스 실행에 실패하였습니다.\n");
		exit(EXIT_FAILURE);
	}
	
	/* 시스널 핸들러 설정 */
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, interrupt);
    signal(SIGTERM, interrupt);
    signal(SIGQUIT, interrupt);
    
	Log("InitServer: 서버 초기화! Process Start [%d]..................................\n", process_id);
}

/**
 * 서버를 종료한다.
 */
void interrupt(int sig)
{
    int i;
	void* result;
	
	/* 쓰레드 종료를 기다린다. */
	thread_terminate = true;
	
	if ( thread_count > 0 )
	{
		for ( i = 0 ; i < thread_count ; i++ )
		{
			/* 쓰레드종료 */
			if ( thread[i].th )
            {
                pthread_join(thread[i].th, &result);
            }
		}
        free(thread);
	}

	/* 프로세스 등록 해제 */
	RemoveProcess(_PROC_EXEC_TRANSACT_);
	DeAttachShm((void*)mdb);
	
    exit(EXIT_SUCCESS);
}
