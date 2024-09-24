/*****************************************************
 * 프로그램ID	: SendNodeEvent.c
 * 프로그램명	: 노드 이벤트를 수신하여 사용자에게 전달한다
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include <pthread.h>
#include <dirent.h>

#include <network.h>
#include <profile.h>
#include <ft_common.h>

#include <KASConn_interface.h>
#include "SendNodeEvent.h"

/*************************************************************************************
 * 구현 시작..
 *************************************************************************************/
 
int main(int argc, char** argv)
{
	int i, n, nread;
	
	SetProgramName(argv[0]);
	
	/* 서버를 초기화 한다. */
	InitServer();
    
	while ( 1 )
    {
    	/* 소켓이벤트를 체크한다. (0.01초) */
    	n = epoll_wait(epoll_fd, events, MAX_SEND_SERVER_USER, 10);
		if ( n < 0 )
		{
			if ( errno != EINTR ) Log("main: epoll_wait Error [%d]\n", errno);
			usleep(3000);
		}
		
		for ( i = 0 ; i < n ; i++ )
		{
			if ( events[i].data.fd == server_sockfd )
			{
				/* 클라이언트를 받아들인다. */
				if ( AcceptUser(server_sockfd) == -1 )
					Log("main: User를 받아 들이는데 실패하였습니다.\n");
			}
			else
			{
				ioctl(events[i].data.fd, FIONREAD, &nread);
		
				/* 클라이언트 제거 */
				if ( nread == 0 )
					RemoveUser(events[i].data.fd);
		
				/* 클라이언트 요청을 받아들인다. */
				else
					ReceiveRequest(events[i].data.fd);
			}
		}
	}
	
	interrupt(0);
	
	exit(EXIT_SUCCESS);
}

/*
 * fd를 등록한다.
 */
int add_epoll(SOCKET fd)
{
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = fd;
    if ( epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1 )
    {
        Log("add_epoll: epoll_ctl EPOLL_CTL_ADD Failed!! errno[%d]\n", errno);
        return (-1);
    }

    return (0);
}

/*
 * fd를 해제한다.
 */
int del_epoll(SOCKET fd)
{
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = fd;
    if ( epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) == -1 )
    {
        Log("del_epoll: epoll_ctl EPOLL_CTL_DEL Failed!! errno[%d]\n", errno);
        return (-1);
    }

    return (0);
}

/*************************************************************************************
 * 쓰레드 처리 함수
 *************************************************************************************/

void* ThreadProcess(void* arg)
{
    int thread_num;
    int user_offset;

    struct timeval tv, cur_time;
	fd_set readfds;
	
	int i, result, len;
	char readbuf[MAX_PACKET], sndbuf[MAX_PACKET];
    KI_SEND_HEADER* resHeader;
    long datakey;

    thread_num = *((int*)arg); free(arg);
    user_offset = thread[thread_num].user_offset;

    Log("ThreadProcess: Thread Start thread_num[%d] user_offset[%d] service_name[%s] account_group[%s] datakey[%ld]\n", thread_num, user_offset, mdb->user.send_user[user_offset].service_name, mdb->user.send_user[user_offset].account_group, mdb->user.send_user[user_offset].datakey);

    while ( !thread[thread_num].thread_terminate )
    {
        /* 열리지 않은 응답큐을 연다 */
        OpenEventFile(thread_num, user_offset);

        /* 현재시간을 가져온다. */
        gettimeofday(&cur_time, NULL);
        
        /* FD SET 초기화 */
        FD_ZERO(&readfds);
        for ( i = 0 ; i < thread[thread_num].event_file_count ; i++ )
        {
            if ( thread[thread_num].event_file[i].filefd > 0 )
            {
                /* 날짜가 변경되었다면 */
                if ( thread[thread_num].event_file[i].date != mdb->system_date )
                {
                    /* 대기 타임아웃이 지났다면, 파일을 닫는다 */
                    if ( cur_time.tv_sec - thread[thread_num].event_file[i].last_read_time.tv_sec > THREAD_WAIT_TIMEOUT )
                    {
                        fclose(thread[thread_num].event_file[i].fp); thread[thread_num].event_file[i].fp = NULL; thread[thread_num].event_file[i].filefd = 0;
                        Log("ThreadProcess(%d): Event File Close [%s]\n", thread_num, thread[thread_num].event_file[i].file_name);
                        continue;
                    }
                }

                FD_SET( thread[thread_num].event_file[i].filefd, &readfds );
            }
        }
            
        /* time out 세팅 (의미없음) */
        tv.tv_sec = 0; tv.tv_usec = 1000;
            
        /* 읽을 데이타가 있는지 체크한다. */
        result = select(FD_SETSIZE, &readfds, (fd_set*)0, (fd_set*)0, &tv);
        if ( result == -1 )
        {
            Log("ThreadProcess(%d): FILE Read Select Failed!! errno[%d]\n", thread_num, errno);
            sleep(1);
            continue;
        }
            
        if ( result > 0 )
        {
            for ( i = 0 ; i < thread[thread_num].event_file_count ; i++ )
            {
                if ( thread[thread_num].event_file[i].filefd > 0 && FD_ISSET(thread[thread_num].event_file[i].filefd, &readfds) )
                {
                    memset(readbuf, 0x00, MAX_PACKET);
                    if ( fgets(readbuf, MAX_PACKET, thread[thread_num].event_file[i].fp) != NULL )
                    {
                        len = strlen(readbuf);
                        if ( readbuf[len-1] == '\n' ) readbuf[len-1] = 0;

                        /* 데이타키를 할당한다 */
                        thread[thread_num].event_file[i].linenum++;
                        datakey = (long)thread[thread_num].event_file[i].date * 100000000 + (long)thread[thread_num].event_file[i].linenum;
                        
                        //printf("[%ld][%ld]\n", mdb->user.send_user[user_offset].datakey, datakey);

                        /* 요청한 데이타키보다 큰경우만 전송한다 */
                        if ( mdb->user.send_user[user_offset].datakey < datakey )
                        {
                            /* 보낼데이타를 생성한다 */
                            memset(sndbuf, 0x00, MAX_PACKET);
                            resHeader = (KI_SEND_HEADER*)sndbuf;
                            resHeader->trid = KI_TRID_NODE_EVENT;
                            resHeader->req_type = KI_REQTYPE_EVENT;
                            sprintf(resHeader->service_name, "%-16s", " ");
                            resHeader->account_type = ' ';
                            sprintf(resHeader->account_group, "%-16s", " ");
                            sprintf(resHeader->user_key, "%-32s", " ");
                            sprintf(resHeader->data_key, "%08d%08d", thread[thread_num].event_file[i].date, thread[thread_num].event_file[i].linenum);
                            sprintf(resHeader->body_length, "%0*ld", KI_BODY_LENGTH, strlen(readbuf));
                            sprintf(sndbuf + sizeof(KI_SEND_HEADER), "%s", readbuf);

                            /* 데이타를 전송한다 */
                            if ( SendTCP(mdb->user.send_user[user_offset].sockfd, sndbuf, strlen(sndbuf), &timeover) == -1 )
                            {
                                Log("ThreadProcess(%d): 데이타 전송에 실패하였습니다. errno[%d]\n", thread_num, errno);
                                RemoveUser(mdb->user.send_user[user_offset].sockfd);
                                continue;
                            }
                            Log("ThreadProcess(%d): SendData [%ld:%s]\n", thread_num, strlen(sndbuf), sndbuf);
                        }

                        gettimeofday(&thread[thread_num].event_file[i].last_read_time, NULL);
                    }
                }
            }
        }

        usleep(10000);
    }

    /* 열려 있는 파일이 있다면 닫는다 */
    for ( i = 0 ; i < thread[thread_num].event_file_count ; i++ )
    {
        if ( thread[thread_num].event_file[i].fp )
            fclose(thread[thread_num].event_file[i].fp);
    }

    Log("ThreadProcess: Thread End thread_num[%d] user_offset[%d] service_name[%s] account_group[%s] datakey[%ld]\n", thread_num, user_offset, mdb->user.send_user[user_offset].service_name, mdb->user.send_user[user_offset].account_group, mdb->user.send_user[user_offset].datakey);
    pthread_exit(0);
	return (NULL);
}

/**
 * 응답큐파일을 연다
 */
int OpenEventFile(int thread_num, int user_offset)
{
    int i;

    DIR* dir_info;
    struct dirent* dir_entry;
    char dirpath[256], tmpbuf[16];

    int filedate, keydate;

    /* 디렉토리를 연다 */
    sprintf(dirpath, "%s/%s/event/", mdb->program_home, DATA_PATH);
    if ( (dir_info = opendir(dirpath)) == NULL )
    {
        Log("OpenResponseQue(%d): 디렉토리 정보를 가져올수 없습니다 dirpath[%s] errno[%d]\n", thread_num, dirpath, errno);
        return (-1);
    }
    keydate = (int)(mdb->user.send_user[user_offset].datakey / 100000000);

    /* 새로운 파일이 생성되었는지 체크하고 파일을 연다 */
    while ( (dir_entry = readdir(dir_info)) )
    {
        /* 확장자가 .evt인 파일만 체크한다 */
        if ( !strstr(dir_entry->d_name, ".evt") ) continue;

        memcpy(tmpbuf, dir_entry->d_name, 8); tmpbuf[8] = 0; filedate = atoi(tmpbuf);
        if ( filedate >= keydate )
        {
            /* 최초 1회 실행 이후에는 오늘 날짜랑 같은 파일만 처리한다 */
            if ( thread[thread_num].initialized && filedate != mdb->system_date )
                continue;
            
            /* 파일이름을 체크한다 */
            if ( strncmp(dir_entry->d_name+9, NODE_EVENT_FILE, strlen(dir_entry->d_name)-13) != 0 )
                continue;
            
            /* 이미 열린 파일인지 체크한다ㅣ */
            for ( i = 0 ; i < thread[thread_num].event_file_count ; i++ )
            {
                if ( thread[thread_num].event_file[i].date == filedate )
                    break;
            }
            if ( i >= thread[thread_num].event_file_count )
            {
                thread[thread_num].event_file = (EVENT_FILE*)realloc(thread[thread_num].event_file, sizeof(EVENT_FILE)*(thread[thread_num].event_file_count+1));
                thread[thread_num].event_file[thread[thread_num].event_file_count].date = filedate;
                sprintf(thread[thread_num].event_file[thread[thread_num].event_file_count].file_name, "%s/%s/event/%08d.%s.evt", mdb->program_home, DATA_PATH, filedate, NODE_EVENT_FILE);
                thread[thread_num].event_file[thread[thread_num].event_file_count].linenum = 0;
                if ( (thread[thread_num].event_file[thread[thread_num].event_file_count].fp = fopen(thread[thread_num].event_file[thread[thread_num].event_file_count].file_name, "r")) == NULL )
                {
                    Log("OpenEventFile(%d): 파일이 존재하지 않습니다 filename[%s] errno[%d]\n", thread_num, thread[thread_num].event_file[thread[thread_num].event_file_count].file_name, errno);
                    thread[thread_num].event_file[thread[thread_num].event_file_count].fp = NULL;
                    thread[thread_num].event_file[thread[thread_num].event_file_count].filefd = 0;
                }
                else
                {
                    thread[thread_num].event_file[thread[thread_num].event_file_count].filefd = fileno(thread[thread_num].event_file[thread[thread_num].event_file_count].fp);
                    gettimeofday(&thread[thread_num].event_file[thread[thread_num].event_file_count].last_read_time, NULL);
                    Log("OpenEventFile(%d): Event File Open [%s]\n", thread_num, thread[thread_num].event_file[thread[thread_num].event_file_count].file_name);
                }
                thread[thread_num].event_file_count++;
            }
        }
    }
    closedir(dir_info);
    thread[thread_num].initialized = true;

    return (thread[thread_num].event_file_count);
}

/*************************************************************************************
 * 요청 데이타 처리 함수
 *************************************************************************************/

/**
 * 요청을 받아들인다
 */
int ReceiveRequest(SOCKET sockfd)
{
    int res, rcv, length;
    char rcvbuf[MAX_PACKET], tmpbuf[16];
    KI_SEND_HEADER* reqHeader;

    int user_offset = GetUserOffset(sockfd);
    if ( user_offset == -1 )
    {
        Log("ReceiveRequest: 사용자 정보를 찾을수 없습니다. sockfd[%d]\n", sockfd);
        del_epoll(sockfd); CloseSocket(sockfd);
        return (-1);
    }

	/* 사이즈를 읽어온다 읽어온다. */
    memset(rcvbuf, 0x00, MAX_PACKET);
    if ( (res = ReceiveTCP(sockfd, rcvbuf, sizeof(KI_SEND_HEADER), &timeover)) <= 0 )
    {
        Log("ReceiveRequest: 헤더 수신에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.send_user[user_offset].service_name, sockfd, errno);
        RemoveUser(sockfd);
        return (-1);
    }
    rcv = res;
    reqHeader = (KI_SEND_HEADER*)rcvbuf;

    /* 나머지 데이타를 읽어온다 */
    memcpy(tmpbuf, reqHeader->body_length, KI_BODY_LENGTH); tmpbuf[KI_BODY_LENGTH] = 0;
    if ( (length = atoi(tmpbuf)) > 0 )
    {
        if ( (res = ReceiveTCP(sockfd, rcvbuf + sizeof(KI_SEND_HEADER), length, &timeover)) <= 0 )
        {
            Log("ReceiveRequest: 데이타 수신에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.send_user[user_offset].service_name, sockfd, errno);
            RemoveUser(sockfd);
            return (-1);
        }
        rcv += res;
    }

    /* 로그를 기록한다 */
    Log("ReceiveRequest: service[%s] rcvbuf[%d:%s]\n", mdb->user.send_user[user_offset].service_name, strlen(rcvbuf), rcvbuf);

    return (0);
}

/*************************************************************************************
 * 사용자 소켓 처리사항
 *************************************************************************************/

/**
 * 사용자를 받아들인다.
 */
int AcceptUser(SOCKET sockfd)
{
	SOCKET fd;
	
	int offset, *arg;
	char ip[15], tmpbuf[32];

	SERVER_USER user;
    KI_SEND_HEADER header;
	
	/* 사용자소켓을 받아들인다. */
	if ( (fd = GetClientSocket(sockfd, ip)) == -1 )
	{
		Log("AcceptUser: 사용자를 받아들일 수 없습니다. [%d]\n", errno);
		return (-1);
	}

    /* 로그인 헤더를 읽어 온다 */
    memset(&header, 0x00, sizeof(KI_SEND_HEADER));
    if ( ReceiveTCP(fd, (char*)&header, sizeof(KI_SEND_HEADER), &timeover) <= 0 )
    {
        Log("AcceptUser: 로그인 수신에 실패하였습니다. sockfd[%d] errno[%d]\n", fd, errno);
        CloseSocket(fd);
		return (-1);
    }
    if ( header.trid != KI_TRID_LOGIN || header.req_type != KI_REQTYPE_LOGIN )
    {
        Log("AcceptUser: 수신된 TR이 로그인이 아닙니다. sockfd[%d] trid[%c] req_type[%c]\n", header.trid, header.req_type);
        CloseSocket(fd);
		return (-1);
    }
	
	/* 사용자 데이타 설정 */
	memset( &user, 0x00, sizeof(SERVER_USER) );
	user.sockfd = fd;
    strcpy(user.user_ip, ip);
    time(&user.connect_time);
    time(&user.polling_time);
    PARSE_STRING(user.service_name, header.service_name, 16);
    PARSE_STRING(user.account_group, header.account_group, 16);
    PARSE_STRING(user.user_key, header.user_key, 32);
    PARSE_LONG(user.datakey, header.data_key, 16, tmpbuf);
	
    pthread_mutex_lock(&user_mutex);

	/* 빈공간을 찾아서 할당한다. */
	for ( offset = 0 ; offset < MAX_SEND_SERVER_USER ; offset++ )
	{
		if ( mdb->user.send_user[offset].sockfd == 0 )
			break;
	}
	if ( offset >= MAX_SEND_SERVER_USER )
	{
        pthread_mutex_unlock(&user_mutex);

		Log("AcceptUser: 허용된 최대 사용자가 초과되었습니다.\n");
		CloseSocket(fd);
		return (-1);
	}
	
	/* 사용자 접속 최종 허용 */
	memcpy( &mdb->user.send_user[offset], &user, sizeof(SERVER_USER) );
    mdb->user.send_user_count++;
    add_epoll(mdb->user.send_user[offset].sockfd);

    pthread_mutex_unlock(&user_mutex);

	Log("사용자 접속: offset[%d] sockfd[%d] ip[%s] service[%s] account_group[%s]\n", offset, mdb->user.send_user[offset].sockfd, mdb->user.send_user[offset].user_ip, mdb->user.send_user[offset].service_name, mdb->user.send_user[offset].account_group);

    /* 쓰레드 정보를 기록한다 */
    thread = (TH_INFO*)realloc(thread, sizeof(TH_INFO)*(thread_count+1));
    thread[thread_count].user_offset = offset;
    thread[thread_count].event_file_count = 0;
    thread[thread_count].event_file = NULL;
    thread[thread_count].thread_terminate = false;
    thread[thread_count].initialized = false;

    /* 사용자 처리 쓰레드를 생성한다 */
	arg = (int*)malloc( sizeof(int) ); *arg = thread_count;
    if ( pthread_create(&thread[thread_count].th, NULL, ThreadProcess, (void*)arg) != 0 )
    {
        Log("AcceptUser: 쓰레드 생성에 실패하였습니다. errno[%d]\n", errno);
        RemoveUser(fd);
        return (-1);
    }
    thread_count++;

	return (offset);
}

/**
 * 사용자 연결을 종료한다.
 */
int RemoveUser(SOCKET sockfd)
{
    int i, offset = GetUserOffset(sockfd);
    if ( offset == -1 )
    {
        del_epoll(sockfd); CloseSocket(sockfd);
        return (0);
    }

    // 쓰레드를 종료한다
    void* result;
    if ( thread )
    {
        for ( i = 0 ; i < thread_count ; i++ )
        {
            if ( thread[i].user_offset == offset )
            {
                thread[i].thread_terminate = true;
                pthread_join(thread[i].th, &result);

                free(thread[i].event_file);
                memset(&thread[i], 0x00, sizeof(TH_INFO));
                thread[i].user_offset = -1;
            }
        }
    }

    // 소켓을 닫는다
	usleep(10000);
	del_epoll(mdb->user.send_user[offset].sockfd);
	CloseSocket(mdb->user.send_user[offset].sockfd);
	
	Log("사용자 종료: offset[%d] sockfd[%d] ip[%s] service[%s] account_group[%s]\n", offset, mdb->user.send_user[offset].sockfd, mdb->user.send_user[offset].user_ip, mdb->user.send_user[offset].service_name, mdb->user.send_user[offset].account_group);

    pthread_mutex_lock(&user_mutex);
	memset( &mdb->user.send_user[offset], 0x00, sizeof(SERVER_USER) );
    mdb->user.send_user_count--;
    pthread_mutex_unlock(&user_mutex);
	
	return (0);
}

/**
 * User Offset을 가져온다
 */
int GetUserOffset(SOCKET sockfd)
{
    int offset;

    pthread_mutex_lock(&user_mutex);
    for ( offset = 0 ; offset < MAX_SEND_SERVER_USER ; offset++ )
    {
        if ( mdb->user.send_user[offset].sockfd == sockfd )
        {
            pthread_mutex_unlock(&user_mutex);
            return (offset);
        }
    }
    pthread_mutex_unlock(&user_mutex);

    return (-1);
}

/*************************************************************************************
 * 초기화함수
 *************************************************************************************/

/**
 * 서버를 초기화한다.
 */ 
void InitServer()
{
    char conf_file_name[256];
	
	/* 공유메모리를 초기화 한다. */
	InitCommonMemory();
	
	/* 설정파일을 연다. */
	sprintf(conf_file_name, "%s/%s/%s", mdb->program_home, CONF_PATH, SERVER_CONFIG_FILE);
	if ( !OpenProfile(conf_file_name) )
	{
		Log("InitServer: 설정파일을 여는 도중 에러가 발생하였습니다. file[%s] errno[%d]\n", conf_file_name, errno);
		exit(EXIT_FAILURE);
	}
	
	/* 설정파일의 값을 가져온다. */
	server_port = GetProfileInt("SERVER_INFO", "SEND_NODE_EVENT_PORT", 0);
	if ( server_port == 0 )
	{
		Log("InitServer: 서버포트 정보가 없습니다.\n");
		exit(EXIT_FAILURE);
	}
	
	/* 설정파일을 닫는다. */
	CloseProfile();

	/* epoll을 생성한다. */
    if ( (epoll_fd = epoll_create(MAX_SEND_SERVER_USER)) < 0 ) 
    { 
        Log("InitServer: epoll 생성 도중 에러가 발생하였습니다. errno[%d]\n", errno);
		exit(EXIT_FAILURE);
    }
    
	/* 서버 소켓을 생성한다. */
	server_sockfd = GetServerSocket(server_port, MAX_SEND_SERVER_USER);
	if ( server_sockfd == -1 )
	{
		Log("InitServer: 서버 소켓 생성에 실패 하였습니다. port[%d] errno[%d]\n", server_port, errno);
		exit(EXIT_FAILURE);
	}
	add_epoll(server_sockfd);
	
	/* 변수를 초기화한다. */
	memset(mdb->user.send_user, 0x00, sizeof(SERVER_USER)*MAX_SEND_SERVER_USER);
    mdb->user.send_user_count = 0;
	timeover.tv_sec = 5; timeover.tv_usec = 0;

    /* 뮤텍스를 생성한다. */
    pthread_mutex_init(&user_mutex, NULL);

    /* Process를 등록한다. */
	if ( (process_id = RegistProcess(_PROC_SEND_NODE_EVENT_)) == -1 )
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
    Log("InitServer: TCP Server Port [%d]\n", server_port);
}

/**
 * 서버를 종료한다.
 */
void interrupt(int sig)
{
    int i;
	void* result;
	
	/* 쓰레드 종료를 기다린다. */	
	if ( thread_count > 0 )
	{
		for ( i = 0 ; i < thread_count ; i++ )
		{
			if ( thread[i].th )
            {
                thread[i].thread_terminate = true;
                pthread_join(thread[i].th, &result);
            }
            free(thread[i].event_file);
		}
        free(thread); thread = NULL;
	}

	/* 연결된 사용자 모두 종료 */
	for ( i = 0 ; i < MAX_SEND_SERVER_USER ; i++ )
	{
		if ( mdb->user.send_user[i].sockfd )
			RemoveUser(mdb->user.send_user[i].sockfd);
	}
	CloseSocket(server_sockfd);
	
	/* epoll 제거 */
	close(epoll_fd);
    pthread_mutex_destroy(&user_mutex);
	
	/* 프로세스 등록 해제 */
	RemoveProcess(_PROC_SEND_NODE_EVENT_);
	DeAttachShm((void*)mdb);
	
    exit(EXIT_SUCCESS);
}
