/*****************************************************
 * 프로그램ID	: InitMemory.c
 * 프로그램명	: 공유메모리를 초기화한다.
 * 작성일		: 2005/05/20
 * 작성자		: 김성철
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <profile.h>
#include <ft_common.h>

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/
 
char program_name[64];

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/
 
int  CheckParam(int, char**);
int  InitConf();
int  InitSystemDate();
int  InitSystemStatus();
int  InitProcess(int, char**);
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
	
	/* 공유메로리를 초기화 한다. */
	InitCommonMemory();
	
	/* 파라미터 정보를 확인한다. */
	if ( CheckParam(argc, argv) == -1 )
	{
		PrintUsage();
		DeAttachShm(mdb);
		exit(EXIT_FAILURE);
	}
	
	DeAttachShm(mdb);
	
	exit(EXIT_SUCCESS);
}

/**
 * 파라미터 정보를 체크한다.
 */
int CheckParam(int pcount, char** param)
{
	int res = 0;
	
	if ( strcmp(param[1], "all") == 0 )
	{
		memset(mdb, 0x00, sizeof(COMMON_SHM));
		InitConf();
		InitSystemDate();
	}
	else if ( strcmp(param[1], "conf") == 0 )
	{
		InitConf();
	}
	else if ( strcmp(param[1], "system_date") == 0 )
	{
		InitSystemDate();
	}
	else if ( strcmp(param[1], "system_status") == 0 )
	{
		InitSystemStatus(pcount, param);
	}
	else if ( strcmp(param[1], "conf") == 0 )
	{
		InitConf();
	}
	else if ( strcmp(param[1], "process") == 0 )
	{
		res = InitProcess(pcount, param);
	}
	else
		res = -1;
	
	return (res);
}

/**
 * 서버환경정보를 초기화한다.
 */
int InitConf()
{
	char conf_file_name[128], path[64];
	
	/* 설정파일이 존재하는지 체크한다. */
	sprintf(conf_file_name, "../%s/%s", CONF_PATH, SERVER_CONFIG_FILE);
	if ( access(conf_file_name, F_OK) != 0 )
	{
		/* 홈디렉토리를 가져온다. */
		char buff[MAX_BUFFER];
		GetHomeDirectory(buff);
		
		/* 설정파일을 다시한번 체크 */
		sprintf(conf_file_name, "%s/%s/%s/%s", buff, PROGRAM_NAME, CONF_PATH, SERVER_CONFIG_FILE);
		if ( access(conf_file_name, F_OK) != 0 )
		{
			Log("InitConf: 설정파일을 찾을수 없습니다. file[%s] errno[%d]\n", conf_file_name, errno);
			return (-1);
		}
	}
	
	/* 설정파일을 연다. */
	if ( !OpenProfile(conf_file_name) )
	{
		Log("InitConf: 설정파일을 여는 도중 에러가 발생하였습니다. file[%s] errno[%d]\n", conf_file_name, errno);
		return (-1);
	}
	
	/* 프로그램 Home을 가져온다 */
	GetHomeDirectory(path);
	sprintf(mdb->program_home, "%s/%s", path, PROGRAM_NAME);
	
	/* 설정파일 정보를 가져온다. */
	mdb->config.is_debug = GetProfileInt("MAIN", "IS_DEBUG", 0);
	GetProfileString("MAIN", "DB_USER", mdb->config.db_user);

	/* 설정파일을 닫는다. */
	CloseProfile();
	
	Log("InitConf: 서버 환경 정보 초기화에 성공 program_home[%s]\n", mdb->program_home);
	Log("InitConf: 서버 환경 정보 초기화에 성공 IS_DEBUG[%d]\n", mdb->config.is_debug);
	Log("InitConf: 서버 환경 정보 초기화에 성공 DB_USER[%s]\n", mdb->config.db_user);
	
	return (0);
}

int InitSystemDate()
{
	mdb->system_date = intDate();
	return (0);
}

int InitSystemStatus(int pcount, char** param)
{
	if ( pcount != 3 )
	{
		printf("InitSystemStatus: Must type Status!! [ 0 | 1 ]\n");
		return (-1);
	}

	int status = atoi(param[2]);
	if ( status != 0 && status != 1 )
	{
		printf("InitSystemStatus: Must type Status!! [ 0 | 1 ]\n");
		return (-1);
	}

	mdb->system_status = status;
	return (0);
}

/**
 * Process를 초기화한다.
 */
int InitProcess(int pcount, char** param)
{
	int i;
	
	switch (pcount)
	{
		case 2:
			memset(&mdb->process.process_info, 0x00, sizeof(PROCESS_INFO) * MAX_PROCESS);
			break;
		
		case 3:
			i = atoi(param[2]);
			memset(&mdb->process.process_info[i], 0x00, sizeof(PROCESS_INFO));
			break;
			
		default:
			return (-1);
	}
	
	return (0);
}

/**
 * 사용방법을 Print한다.
 */
void PrintUsage()
{
	printf("usage: %s excute_type [option]\n", program_name);
	printf("  excute_type: \n");
	printf("    all - All Initial\n");
	printf("    conf - Server Configure Initial\n");
	printf("    system_date - System Date Initial\n");
	printf("    system_status [0|1] - Set System Status\n");
	printf("    process [index] - Process Clear\n");
}
