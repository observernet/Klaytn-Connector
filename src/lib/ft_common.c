#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/ipc.h>

#include <packet.h>
#include <profile.h>

#include <server_define.h>
#include "ft_common.h"

extern char		program_name[64];

/**
 * Log를 기록한다.
 */
void Log(char* fmt, ...)
{
    va_list args;
    char buf[MAX_PACKET];
	
	/* 로그 정보를 가져온다. */
	memset(buf, 0x00, MAX_PACKET);
	va_start(args, fmt);
	vsprintf( buf, fmt, args );
	va_end(args);
		
	/* 공유메모리가 초기화가 안되었다면 */
	if ( mdb == NULL )
	{
		printf("%s: [%d %d] %s", program_name, intDate(), intTime(), buf);
	}
	else
	{
		FILE *fp;
		char file_name[256];
		
	    sprintf(file_name, "%s/%s/%s.%d.log", mdb->program_home, LOG_PATH, program_name, intDate());
	
		fp = fopen(file_name, "a+");
		fprintf(fp, "[%d %d] %s", intDate(), intTime(), buf);
		fclose(fp);
		
		if ( mdb->config.is_debug )
		printf("%s: [%d %d] %s", program_name, intDate(), intTime(), buf);
	}
}

/**
 * 올바른 프로그램 이름을 세팅한다.
 */
void SetProgramName(char* param)
{
	int i, length;
	
	length = strlen(param);
	
	for ( i = length - 1 ; i >= 0 ; i-- )
	{
		if ( param[i] == '/' )
			break;
	}
	
	strncpy(program_name, param + (i+1), length - (i+1));
}

/**
 * 공유메모리를 초기화 한다..
 */
int InitCommonMemory()
{
	int shmid;
	
	/* 이미 Attach된 메모리인지 체크한다. */
	if ( mdb != NULL )
	{
		Log("InitCommonMemory: 이미 등록된 Common Memory입니다.\n");
		return (-1);
	}
	
	/* 공유메모리에 Attach한다 */
	shmid = GetShm(COMMON_SHM_KEY, sizeof(COMMON_SHM));
	mdb = (COMMON_SHM*)AttachShm(shmid);
	if ( mdb == NULL )
		return (-1);
	
	return (0);
}

/**
 * 프로세스를 등록한다.
 */
int RegistProcess(int process_type)
{
	if ( mdb == 0 )
	{
		Log("RegistProcess: Shared Memory Not Initial!! Regist Failed!!\n");
		exit(EXIT_FAILURE);
	}
	
	if ( process_type < 0 || process_type >= MAX_PROCESS )
	{
		Log("RegistProcess: offset range over!! 0 <= offset < %d\n", MAX_PROCESS);
		exit(EXIT_FAILURE);
	}
	
	if ( mdb->process.process_info[process_type].pid != 0 )
	{
		Log("RegistProcess: 이미 등록된 Process 입니다. [%s][%s]\n", mdb->process.process_info[process_type].program_name, program_name);
		exit(EXIT_FAILURE);
	}
	
	/* Process 등록 */
	strcpy(mdb->process.process_info[process_type].program_name, program_name);
	mdb->process.process_info[process_type].pid = getpid();
	
	Log("RegistProcess: Regist Process PID:%d, offset:%d\n", mdb->process.process_info[process_type].pid, process_type);
	
	return (mdb->process.process_info[process_type].pid);
}

/**
 * 프로세스를 제거한다.
 */
void RemoveProcess(int process_type)
{
	if ( mdb == 0 )
	{
		Log("RemoveProcess: Shared Memory Not Initial!! Regist Failed!!\n");
		exit(EXIT_FAILURE);
	}
	
	if ( process_type < 0 || process_type >= MAX_PROCESS )
	{
		Log("RemoveProcess: offset range over!! 0 <= offset < %d\n", MAX_PROCESS);
		exit(EXIT_FAILURE);
	}
	
	/* Process 제거 */
	Log("RemoveProcess: Remove Process PID:%d, index:%d\n", mdb->process.process_info[process_type].pid, process_type);
	memset( &mdb->process.process_info[process_type], 0x00, sizeof(PROCESS_INFO) );
}

/**
 * Data Read Number 를 구해온다.
 */
int GetReadLineNum(int date, char* file_type, char* address)
{
	FILE* fp;
	char filename[256], buff[64];
	int linenum = 0;

	/* 라인파일을 연다 */
	if ( address ) sprintf(filename, "%s/%s/line/%s.%08d.%s.%s.line", mdb->program_home, DATA_PATH, program_name, date, file_type, address);
	else		   sprintf(filename, "%s/%s/line/%s.%08d.%s.line", mdb->program_home, DATA_PATH, program_name, date, file_type);
	if ( (fp = fopen(filename, "r")) == NULL ) return (0);

	/* 라인번호를 가져온다 */
	memset(buff, 0x00, 64);
	if ( fgets(buff, 64, fp) != NULL )
	{
		linenum = atoi(buff);
	}
	fclose(fp);

	return (linenum);
}

/**
 * Data Read Number을 기론한다.
 */
void WriteReadLineNum(int readnum, int date, char* file_type, char* address)
{
	FILE* fp;
	char filename[256];

	/* 라인파일을 열고 기록한다 */
	if ( address ) sprintf(filename, "%s/%s/line/%s.%08d.%s.%s.line", mdb->program_home, DATA_PATH, program_name, date, file_type, address);
	else		   sprintf(filename, "%s/%s/line/%s.%08d.%s.line", mdb->program_home, DATA_PATH, program_name, date, file_type);
	fp = fopen(filename, "w");
	fprintf(fp, "%d", readnum);
	fclose(fp);
}
