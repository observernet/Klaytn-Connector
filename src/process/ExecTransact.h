#ifndef _EXEC_TRANSACT_H
#define _EXEC_TRANSACT_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************
 * Defines.....
 *************************************************************************************/

/**
 * THREAD_PER_SLEEP: 건당 처리후 대기 시간 
 * THREAD_WAIT_COUNT: 날짜 변경 후 쓰레드 유지시간 ( 실제시간(초) = THREAD_WAIT_COUNT * THREAD_PER_SLEEP / 1000000 )
 */
#define THREAD_PER_SLEEP            50000
#define THREAD_WAIT_COUNT           72000

/* 쓰레드정보 */
typedef struct
{
	pthread_t		    th;
    int                 date;
    char                account_group[32];
} TH_INFO;

/* 요청정보 */
typedef struct
{
    long                datakey;
    KI_REQRES_HEADER    header;
    char                data[MAX_PACKET];
} QDATA;

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/
 
char				program_name[64];
int 				process_id;

int				    thread_count;
TH_INFO			    *thread;

bool			    thread_terminate;

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/

void* ThreadProcess(void*);
int   SendTransactToKlaytn(int thread_num, QDATA* qdata);
int   SendTransactToKAS(int thread_num, QDATA* qdata);

int   InsertResponseQue(int date, char* account_group, QDATA* req, char* res);
int   SendErrorMessage(int date, char* account_group, QDATA* req, char* errmsg);

void  InitServer();
void  interrupt(int);

#ifdef __cplusplus
}
#endif

#endif
