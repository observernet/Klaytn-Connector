/*****************************************************
 * 프로그램ID	: common_memory.h
 * 프로그램명	: 공유 메모리를 정의한다.
 *****************************************************/
 
#ifndef _COMMON_MEMORY_H
#define _COMMON_MEMORY_H

#include <network.h>
#include <server_define.h>

/******************************************************************************
 * 서버설정정보
 ******************************************************************************/

typedef struct 
{
	int						is_debug;									/* DEBUG 여부 (0/1) */
	char					db_user[64];								/* DB 접속정보 */
} SERVER_CONFIG;


/******************************************************************************
 * 전체 Process 정보
 ******************************************************************************/

/**
 * Process 정보를 담고 있는 Struct
 */
typedef struct
{
	pid_t					pid;										/* Process 고유번호 */
	char					program_name[64];							/* Process 이름 */
} PROCESS_INFO;

/**                                                     			
 * Process Shared Memory                                			
 */                                                     			
typedef struct
{
	int						process_count;								/* 현재 등록된 Process 갯수 */
	int						process_end_ptr;							/* Process Memory의 제일 마지막 데이타 포인터 */
	PROCESS_INFO			process_info[MAX_PROCESS];					/* 등록된 Process 정보 */
} PROCESS_SHM;

/******************************************************************************
 * 서버 접속 정보
 ******************************************************************************/

typedef struct
{
	SOCKET          		sockfd;
    char            		user_ip[16];
	time_t 					connect_time;
	time_t 					polling_time;
	char					service_name[32];
	char					account_group[32];
	char					user_key[64];
	long					datakey;
} SERVER_USER;

typedef struct
{
	int						inquiry_user_count;
	SERVER_USER				inquiry_user[MAX_INQUIRY_SERVER_USER];

	int						request_user_count;
	SERVER_USER				request_user[MAX_REQUEST_SERVER_USER];

	int						response_user_count;
	SERVER_USER				response_user[MAX_SEND_SERVER_USER];

	int						send_user_count;
	SERVER_USER				send_user[MAX_SEND_SERVER_USER];
} SVR_USER_SHM;

/******************************************************************************
 * Common Shared Memory 정보
 ******************************************************************************/

typedef struct
{
	char					program_home[64];							/* 프로그램 Home */
	int						system_date;								/* 프로그램 실행일자 */
	int						system_status;								/* 프로그램 상태 (0: 접수정지, 1: 정상) */
	
	SERVER_CONFIG			config;										/* 서버 설정 정보 */
	PROCESS_SHM				process;									/* 전체 Process 정보 */
	SVR_USER_SHM			user;										/* 서버 접속 정보 */

} COMMON_SHM;


#endif
