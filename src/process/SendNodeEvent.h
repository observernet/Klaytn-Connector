#ifndef _SEND_NODE_EVENT_H
#define _SEND_NODE_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************
 * Defines.....
 *************************************************************************************/

/* Node Event File Name */
#define NODE_EVENT_FILE             "NodeAPI"

/* 날짜변경 후 쓰레드 유지시간 - 초 */
#define THREAD_WAIT_TIMEOUT          3600

#define PARSE_STRING(V, T, S)   memcpy(V, T, S); V[S] = 0; str_trim(V, TRIM_ALL)
#define PARSE_LONG(V, T, S, TM) memcpy(TM, T, S); TM[S] = 0; V = atol(TM)

/* Response Que File Info */
typedef struct
{
	char		file_name[128];
	int         date;
    int         linenum;

	FILE*		fp;
	int			filefd;
	struct timeval last_read_time;
} EVENT_FILE;

/* 쓰레드정보 */
typedef struct
{
	pthread_t		    th;
    bool			    thread_terminate;
    bool                initialized;

    int                 user_offset;
    int                 event_file_count;
    EVENT_FILE*         event_file;
} TH_INFO;

/* 응답정보 */
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

int					epoll_fd;
struct epoll_event	events[MAX_SEND_SERVER_USER];
struct timeval		timeover;

int				    server_port;
SOCKET			    server_sockfd;

pthread_mutex_t     user_mutex;
int				    thread_count;
TH_INFO			    *thread;

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/

int   add_epoll(SOCKET fd);
int   del_epoll(SOCKET fd);

void* ThreadProcess(void* arg);
int   OpenEventFile(int thread_num, int user_offset);


int   ReceiveRequest(SOCKET sockfd);

int   AcceptUser(SOCKET sockfd);
int   RemoveUser(SOCKET sockfd);
int   GetUserOffset(SOCKET sockfd);

void  InitServer();
void  interrupt(int);

#ifdef __cplusplus
}
#endif

#endif
