#ifndef _INQUIRY_CALL_H
#define _INQUIRY_CALL_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************
 * Defines.....
 *************************************************************************************/

#define PARSE_STRING(V, T, S)   memcpy(V, T, S); V[S] = 0; str_trim(V, TRIM_ALL)

#define MAX_THREAD_QUE_SIZE		1024

/* 쓰레드 큐정보 */
typedef struct
{
    int             user_offset;
    char*           rcvbuf;
    time_t	        request_time;
} TH_QUE;

/* 쓰레드정보 */
typedef struct
{
    TH_QUE q[MAX_THREAD_QUE_SIZE];
	
	int 			read_ptr;
	int 			write_ptr;

	pthread_t		th;
} TH_INFO;

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/
 
char				program_name[64];
int 				process_id;

int					epoll_fd;
struct epoll_event	events[MAX_INQUIRY_SERVER_USER];
struct timeval		timeover;

int				    server_port;
SOCKET			    server_sockfd;

int				    thread_count;
TH_INFO			    *thread;

bool			    thread_terminate;

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/

int   add_epoll(SOCKET fd);
int   del_epoll(SOCKET fd);

int   ReceiveRequest(SOCKET sockfd);
int   AllocateThread(int user_offset, KI_REQRES_HEADER* reqHeader, char* rcvbuf);
int   SendErrorMessage(int user_offset, KI_REQRES_HEADER* reqHeader, char success, char* errmsg);

void* InquiryCall(void*);
int   InquiryCallToKlaytn(int thread_num, int user_offset, KI_REQRES_HEADER* reqHeader, char* reqBody);
int   InquiryCallToKAS(int thread_num, int user_offset, KI_REQRES_HEADER* reqHeader, char* reqBody);
int   SendResponse(int thread_num, int user_offset, KI_REQRES_HEADER* reqHeader, char* resBody);

int   AcceptUser(SOCKET sockfd);
int   RemoveUser(SOCKET sockfd);
int   GetUserOffset(SOCKET sockfd);

void  InitThread();
void  InitServer();
void  interrupt(int);

#ifdef __cplusplus
}
#endif

#endif
