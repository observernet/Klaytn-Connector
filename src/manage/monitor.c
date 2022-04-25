/*****************************************************
 * 프로그램ID	: monitor.c
 * 프로그램명	: uExpert 메모리 정보를 관찰한다.
 * 작성일		: 2005/11/01
 * 작성자		: 김성철
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <ft_common.h>

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/

char program_name[64];

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/
int  CheckParam(int, char**);

void ViewConf();
void ViewSystemInfo();
void ViewProcess();

void PrintUsage();

/*************************************************************************************
 * 구현 시작..
 *************************************************************************************/

int main(int argc, char** argv)
{
	SetProgramName(argv[0]);
	
	/* 파리미터 갯수를 체크한다. */
	if ( argc < 2 )
	{
		PrintUsage();
		exit(EXIT_FAILURE);
	}
	
	/* 공유메로리를 초기화한다. */
	InitCommonMemory();
	
	/* 화면을 지운다. */
	system("clear");
	
	/* 파라미터 정보를 확인한다. */
	if ( CheckParam(argc, argv) == -1 )
		PrintUsage();
	
	
	DeAttachShm(mdb);
	exit(EXIT_SUCCESS);
}

/**
 * 파라미터 정보를 체크한다.
 */
int CheckParam(int pcount, char** param)
{
	int res = 0;
	
	if ( strcmp(param[1], "conf") == 0 )
		ViewConf();
	else if ( strcmp(param[1], "system_info") == 0 )
		ViewSystemInfo();
	else if ( strcmp(param[1], "process") == 0 )
		ViewProcess();
	else
		res = -1;
	
	return (res);
}

/**
 * 서버 환경정보를 출력한다.
 */
void ViewConf()
{
	printf("\n");
	printf("************************** SERVER PROCESS INFOMATION ***************************\n");
	printf(" PROGRAM_HOME                                [%s]\n", mdb->program_home);
	printf(" is_debug                                    [%d]\n", mdb->config.is_debug);
	printf(" db_user                                     [%s]\n", mdb->config.db_user);
	printf("*********************************************************************************\n");
}

/**
 * 시스템 정보를 출력한다.
 */
void ViewSystemInfo()
{
	printf("\n");
	printf("************************** SYSTEM INFO INFOMATION ***************************\n");
	printf(" SYSTEM_DATE                                [%d]\n", mdb->system_date);
	printf(" SYSTEM_STATUS                              [%d]\n", mdb->system_status);
	printf("*****************************************************************************\n");
}

/**
 * Process 정보를 출력한다.
 */
void ViewProcess()
{
	FILE* fp;
	char token[16], file_name[128], buffer[MAX_BUFFER];
	
	int ptr, procid;
	char process_path[128], process_name[64];
	
	/* 파일을 연다. */
	sprintf(file_name, "%s/%s/%s", mdb->program_home, CONF_PATH, PROCESS_LIST_FILE);
	if ( (fp = fopen(file_name, "r")) == NULL )
	{
		printf("ViewProcess: 파일을 열 수 없습니다. [%s] errno[%d]\n", file_name, errno);
		return;
	}
	
	printf("\n");
	printf("**************************** PROCESS INFOMATION ******************************\n");
	
	while ( 1 )
	{
		memset(buffer, 0x00, MAX_BUFFER);
		if ( fgets(buffer, MAX_BUFFER, fp) == NULL )
			break;
		
		if ( buffer[0] == '#' || strlen(buffer) < 10 )
			continue;
		
		/* 데이타 파싱 */
		ptr = get_next_token(buffer, 0, token, ';'); procid = atoi(token);
		ptr = get_next_token(buffer, ptr, process_path, ';'); str_trim(process_path, TRIM_ALL);
		ptr = get_next_token(buffer, ptr, process_name, ';'); str_trim(process_name, TRIM_ALL);
		
		if ( mdb->process.process_info[procid].pid == 0 || getsid(mdb->process.process_info[procid].pid) < 0 )
		{
			printf("Index %2d: No Process, Please Check... %s/%s \n", procid, process_path, process_name);
		}
		else
		{
			printf("Index %2d: Normal Processing... %s/%s\n", procid, process_path, process_name);
		}
	}
	fclose(fp);
	
	printf("*******************************************************************************\n");
}

/**
 * 사용방법을 Print한다.
 */
void PrintUsage()
{
	printf("usage: %s excute_type [option]\n", program_name);
	printf("  excute_type: \n");
	printf("    conf - Server Configure View\n");
	printf("    system_info - System Info View\n");
	printf("    process - Regist Process View\n");
}
