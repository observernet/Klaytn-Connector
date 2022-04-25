/*****************************************************
 * 프로그램ID	: ProcessManager.c
 * 프로그램명	: 시세 프로세스를 관리한다.
 * 작성일		: 2007/11/10
 * 작성자		: 김성철
 *****************************************************/

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <server_define.h>
#include <common_memory.h>
#include <ft_common.h>
#include <vector.h>

/*************************************************************************************
 * Defines....
 *************************************************************************************/
 
#define PROCESS_POLLING_TIME		10

typedef struct _process_list_st
{
	int		process_no;
	char	process_path[128];
	char	process_name[64];
	char	is_manage;
	
	int		excute_count;
} PROC_LIST;

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/
 
char program_name[64];

VECTOR* g_process_vector = NULL;

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/

void ProcessCheck();
int  RunProcess(PROC_LIST*);
void interrupt(int);

void SettingProcess();

/*************************************************************************************
 * 구현 시작..
 *************************************************************************************/

int main(int argc, char** argv)
{
	SetProgramName(argv[0]);
	
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, interrupt);
    signal(SIGTERM, interrupt);
    signal(SIGQUIT, interrupt);
    
    // 공유메모리를 초기화 한다.
    InitCommonMemory();
    SettingProcess();
    
	while (1)
	{
		ProcessCheck();
		sleep(PROCESS_POLLING_TIME);
	}
	
	DeAttachShm(mdb);
	
	exit(EXIT_SUCCESS);
}

/**
 * Process를 체크한다.
 */
void ProcessCheck()
{
	int i, vector_length;
	PROC_LIST* proc;
	void** temp;
	
	vector_length = GetLengthVector(g_process_vector);
	for ( i = 0 ; i < vector_length ; i++ )
	{
		temp = (void**)GetAtVector(i, g_process_vector);
		proc = (PROC_LIST*)(*temp);
		
		if ( proc->is_manage != 'Y' )
			continue;
		
		if ( mdb->process.process_info[proc->process_no].pid == 0 )
			RunProcess(proc);
		else
		{
			if ( getsid(mdb->process.process_info[proc->process_no].pid) < 0 )
			{
				// Remove Process
				RemoveProcess(proc->process_no);
				RunProcess(proc);
			}
			else
				proc->excute_count = 0;
		}
	}
}

/**
 * 프로세스를 실행시킨다.
 */
int RunProcess(PROC_LIST* proc)
{
	char buffer[512];
	
	sprintf(buffer, "cd %s;%s/%s &", proc->process_path,
									 proc->process_path,
									 proc->process_name);
	
	system(buffer);
	proc->excute_count++;
	
	Log("RunProcess: 실행 [%s]\n", buffer);
	
	return (0);
}

void interrupt(int sig)
{
	int i, vector_length;
	PROC_LIST* proc;
	void** temp;
	
	vector_length = GetLengthVector(g_process_vector);
	for ( i = 0 ; i < vector_length ; i++ )
	{
		temp = (void**)GetAtVector(i, g_process_vector);
		proc = (PROC_LIST*)(*temp);
		free(proc);
	}
	FreeVector(g_process_vector);
	
	DeAttachShm((void*)mdb);
    exit(EXIT_SUCCESS);
}

/**
 * 프로세스 이름을 세팅한다.
 */
void SettingProcess()
{
	FILE* fp;
	char token[8], file_name[128], buffer[MAX_BUFFER];
	
	int ptr;
	PROC_LIST* proc_list;
	
	/* VECTOR 생성 */
	g_process_vector = CreateVector(VECTOR_DEFAULT_SIZE, sizeof(PROC_LIST*));
	
	/* 파일을 연다. */
	sprintf(file_name, "%s/%s/%s", mdb->program_home, CONF_PATH, PROCESS_LIST_FILE);
	if ( (fp = fopen(file_name, "r")) == NULL )
	{
		Log("GetProcessList: 파일을 열 수 없습니다. [%s] errno[%d]\n", file_name, errno);
		interrupt(0);
	}
	
	while ( 1 )
	{
		memset(buffer, 0x00, MAX_BUFFER);
		if ( fgets(buffer, MAX_BUFFER, fp) == NULL )
			break;
		
		if ( buffer[0] == '#' || strlen(buffer) < 10 )
			continue;
		
		/* 데이타 파싱 */
		proc_list = (PROC_LIST*)calloc(1, sizeof(PROC_LIST));
		ptr = get_next_token(buffer, 0, token, ';'); proc_list->process_no = atoi(token);
		ptr = get_next_token(buffer, ptr, proc_list->process_path, ';'); str_trim(proc_list->process_path, TRIM_ALL);
		ptr = get_next_token(buffer, ptr, proc_list->process_name, ';'); str_trim(proc_list->process_name, TRIM_ALL);
		ptr = get_next_token(buffer, ptr, token, ';'); proc_list->is_manage = token[0];
		
		AddVector((char*)&proc_list, g_process_vector);
	}
	fclose(fp);
}
