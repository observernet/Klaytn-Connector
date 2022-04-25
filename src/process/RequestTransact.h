#ifndef _REQUEST_TRANSACT_H
#define _REQUEST_TRANSACT_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************
 * Defines.....
 *************************************************************************************/

#define PARSE_STRING(V, T, S)   memcpy(V, T, S); V[S] = 0; str_trim(V, TRIM_ALL)

/*************************************************************************************
 * Global 변수 정의
 *************************************************************************************/
 
char				program_name[64];
int 				process_id;

int					epoll_fd;
struct epoll_event	events[MAX_REQUEST_SERVER_USER];
struct timeval		timeover;

int				    server_port;
SOCKET			    server_sockfd;

long                unique_datakey;

/*************************************************************************************
 * 함수 정의
 *************************************************************************************/

int   add_epoll(SOCKET fd);
int   del_epoll(SOCKET fd);

int   ReceiveRequest(SOCKET sockfd);
int   SendResponse(int user_offset, KI_REQRES_HEADER* reqHeader, char success, char* errmsg, long datakey);

long  InsertRequestQue(KI_REQRES_HEADER* reqHeader, char* buff);
long  ReadDataKeyFromFile();
void  WriteDataKeyToFile(long datakey);

int   AcceptUser(SOCKET sockfd);
int   RemoveUser(SOCKET sockfd);
int   GetUserOffset(SOCKET sockfd);

void  InitServer();
void  interrupt(int);

#ifdef __cplusplus
}
#endif

#endif
