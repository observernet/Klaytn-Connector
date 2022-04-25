/*****************************************************
 * 프로그램ID	: RequestTransact.c
 * 프로그램명	: 클레이튼 Transaction 받아 Que에 넣는다
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

#include <network.h>
#include <profile.h>
#include <ft_common.h>

#include <KASConn_interface.h>
#include "RequestTransact.h"

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
    	n = epoll_wait(epoll_fd, events, MAX_REQUEST_SERVER_USER, 10);
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
 * 요청 데이타 처리 함수
 *************************************************************************************/

/**
 * 요청을 받아들인다
 */
int ReceiveRequest(SOCKET sockfd)
{
    int res, rcv, length;
    char rcvbuf[MAX_PACKET], tmpbuf[16];
    KI_REQRES_HEADER* reqHeader;
    long datakey;

    int user_offset = GetUserOffset(sockfd);
    if ( user_offset == -1 )
    {
        Log("ReceiveRequest: 사용자 정보를 찾을수 없습니다. sockfd[%d]\n", sockfd);
        del_epoll(sockfd); CloseSocket(sockfd);
        return (-1);
    }

	/* 사이즈를 읽어온다 읽어온다. */
    memset(rcvbuf, 0x00, MAX_PACKET);
    if ( (res = ReceiveTCP(sockfd, rcvbuf, sizeof(KI_REQRES_HEADER), &timeover)) <= 0 )
    {
        Log("ReceiveRequest: 헤더 수신에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.request_user[user_offset].service_name, sockfd, errno);
        RemoveUser(sockfd);
        return (-1);
    }
    rcv = res;
    reqHeader = (KI_REQRES_HEADER*)rcvbuf;

    /* 나머지 데이타를 읽어온다 */
    memcpy(tmpbuf, reqHeader->body_length, KI_BODY_LENGTH); tmpbuf[KI_BODY_LENGTH] = 0;
    if ( (length = atoi(tmpbuf)) > 0 )
    {
        if ( (res = ReceiveTCP(sockfd, rcvbuf + sizeof(KI_REQRES_HEADER), length, &timeover)) <= 0 )
        {
            Log("ReceiveRequest: 데이타 수신에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.request_user[user_offset].service_name, sockfd, errno);
            RemoveUser(sockfd);
            return (-1);
        }
        rcv += res;
    }

    /* 로그를 기록한다 */
    Log("ReceiveRequest: service[%s] rcvbuf[%d:%s]\n", mdb->user.request_user[user_offset].service_name, strlen(rcvbuf), rcvbuf);
    time(&mdb->user.request_user[user_offset].polling_time);

    /* 트랜잭션 요청만 처리한다 */
    if ( reqHeader->req_type != KI_REQTYPE_TRANSACT )
    {
        Log("ReceiveRequest: 트랜잭션 요청만 허용합니다. service[%s] req_type[%c]\n", mdb->user.request_user[user_offset].service_name, reqHeader->req_type);
        SendResponse(user_offset, reqHeader, 'N', "허용되지 않은 요청종류입니다 (트랜잭션만 허용)", 0);
        return (-1);
    }

    /* 시스템 상태를 체크한다 */
    if ( mdb->system_status != SYSTEM_STATUS_OPEN )
    {
        Log("ReceiveRequest: 접수 대기 상태입니다. service[%s] req_type[%c]\n", mdb->user.request_user[user_offset].service_name, reqHeader->req_type);
        SendResponse(user_offset, reqHeader, 'N', "현재 접수 대기중 상태입니다", 0);
        return (-1);
    }

    /* 큐에 넣는다 */
    datakey = InsertRequestQue(reqHeader, rcvbuf + sizeof(KI_REQRES_HEADER));

    /* 응답데이타를 전송한다 */
    SendResponse(user_offset, reqHeader, 'Y', "", datakey);

    return (0);
}

/**
 * 사용자에게 응답을 전송한다
 */
int SendResponse(int user_offset, KI_REQRES_HEADER* reqHeader, char success, char* errmsg, long datakey)
{
    char sndbuf[MAX_BUFFER], tmpbuf[16];
    KI_REQRES_HEADER* resHeader;
    char* resBody;
    
    /* 보낼 헤더를 세팅한다 */
    memset(sndbuf, 0x00, MAX_BUFFER);
    memcpy(sndbuf, reqHeader, sizeof(KI_REQRES_HEADER));
    resHeader = (KI_REQRES_HEADER*)sndbuf;

    /* 보낼 데이타를 세팅한다 */
    resBody = (char*)(sndbuf + sizeof(KI_REQRES_HEADER));
    if ( success == 'Y' )
    {
        sprintf(resBody, "{\"success\":true,\"msg\":%ld}", datakey);
    }
    else
    {
        sprintf(resBody, "{\"success\":false,\"msg\":\"%s\"}", errmsg);
    }

    /* Body Size를 세팅한다 */
    sprintf(tmpbuf, "%0*ld", KI_BODY_LENGTH, strlen(resBody));
    memcpy(resHeader->body_length, tmpbuf, KI_BODY_LENGTH);

    /* 응답데이타를 전송한다 */
    if ( SendTCP(mdb->user.request_user[user_offset].sockfd, sndbuf, strlen(sndbuf), &timeover) == -1 )
    {
        Log("SendResponse: 응답데이타 전송에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.request_user[user_offset].service_name, mdb->user.request_user[user_offset].sockfd, errno);
        return (-1);
    }

    return (0);
}

/**
 * 요청데이타를 큐에 삽입한다
 */
long InsertRequestQue(KI_REQRES_HEADER* reqHeader, char* buff)
{
    FILE* fp;
    char filename[256], sndbuf[MAX_PACKET];

    char account_group[40];
    long datakey;

    if ( (int)(unique_datakey / 100000000) != mdb->system_date ) unique_datakey = (long)mdb->system_date * 100000000;
    datakey = ++unique_datakey;

    /* 큐에 보낼 데이타를 세팅한다 */
    PARSE_STRING(account_group, reqHeader->account_group, 16);
    sprintf(sndbuf, "%ld\t'%-.*s'\t'%s'", datakey, (int)sizeof(KI_REQRES_HEADER) - KI_BODY_LENGTH, (char*)reqHeader, buff);

    /* 큐에 데이타를 넣는다 */
    sprintf(filename, "%s/%s/req/%08d.%s.que", mdb->program_home, DATA_PATH, mdb->system_date, account_group);
    if ( (fp = fopen(filename, "a+")) == NULL ) return (-1);
    fprintf(fp, "%s\n", sndbuf);
    fclose(fp);

    /* datakey를 파일에 기록한다 */
    WriteDataKeyToFile(datakey);

    return (datakey);
}

long ReadDataKeyFromFile()
{
    FILE* fp;
	char filename[256], buff[64];
	long datakey = 0;

	/* 라인파일을 연다 */
	sprintf(filename, "%s/%s/line/%s.%08d.datakey", mdb->program_home, DATA_PATH, program_name, mdb->system_date);
	if ( (fp = fopen(filename, "r")) == NULL ) return ((long)mdb->system_date * 100000000);

	/* 라인번호를 가져온다 */
	memset(buff, 0x00, 64);
	if ( fgets(buff, 64, fp) != NULL )
	{
		datakey = atol(buff);
	}
	fclose(fp);

	return (datakey);
}

void WriteDataKeyToFile(long datakey)
{
    FILE* fp;
	char filename[256];

	/* 라인파일을 열고 기록한다 */
	sprintf(filename, "%s/%s/line/%s.%08d.datakey", mdb->program_home, DATA_PATH, program_name, mdb->system_date);
	fp = fopen(filename, "w");
	fprintf(fp, "%ld", datakey);
	fclose(fp);
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
	
	int offset;
	char ip[15];

	SERVER_USER user;
    KI_REQRES_HEADER header;
	
	/* 사용자소켓을 받아들인다. */
	if ( (fd = GetClientSocket(sockfd, ip)) == -1 )
	{
		Log("AcceptUser: 사용자를 받아들일 수 없습니다. [%d]\n", errno);
		return (-1);
	}

    /* 로그인 헤더를 읽어 온다 */
    memset(&header, 0x00, sizeof(KI_REQRES_HEADER));
    if ( ReceiveTCP(fd, (char*)&header, sizeof(KI_REQRES_HEADER), &timeover) <= 0 )
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
	
	/* 빈공간을 찾아서 할당한다. */
	for ( offset = 0 ; offset < MAX_REQUEST_SERVER_USER ; offset++ )
	{
		if ( mdb->user.request_user[offset].sockfd == 0 )
			break;
	}
	if ( offset >= MAX_REQUEST_SERVER_USER )
	{	
		Log("AcceptUser: 허용된 최대 사용자가 초과되었습니다.\n");
		CloseSocket(fd);
		return (-1);
	}
	
	/* 사용자 접속 최종 허용 */
	memcpy( &mdb->user.request_user[offset], &user, sizeof(SERVER_USER) );
    mdb->user.request_user_count++;

	add_epoll(mdb->user.request_user[offset].sockfd);
	Log("사용자 접속: offset[%d] sockfd[%d] ip[%s] service[%s] account_group[%s]\n", offset, mdb->user.request_user[offset].sockfd, mdb->user.request_user[offset].user_ip, mdb->user.request_user[offset].service_name, mdb->user.request_user[offset].account_group);
	
	return (offset);
}

/**
 * 사용자 연결을 종료한다.
 */
int RemoveUser(SOCKET sockfd)
{
    int offset = GetUserOffset(sockfd);
    if ( offset == -1 )
    {
        del_epoll(sockfd); CloseSocket(sockfd);
        return (0);
    }

	usleep(10000);
	del_epoll(mdb->user.request_user[offset].sockfd);
	CloseSocket(mdb->user.request_user[offset].sockfd);
	
	Log("사용자 종료: offset[%d] sockfd[%d] ip[%s] service[%s] account_group[%s]\n", offset, mdb->user.request_user[offset].sockfd, mdb->user.request_user[offset].user_ip, mdb->user.request_user[offset].service_name, mdb->user.request_user[offset].account_group);
	memset( &mdb->user.request_user[offset], 0x00, sizeof(SERVER_USER) );
    mdb->user.request_user_count--;
	
	return (0);
}

/**
 * User Offset을 가져온다
 */
int GetUserOffset(SOCKET sockfd)
{
    int offset;

    for ( offset = 0 ; offset < MAX_REQUEST_SERVER_USER ; offset++ )
    {
        if ( mdb->user.request_user[offset].sockfd == sockfd )
            return (offset);
    }

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
	server_port = GetProfileInt("SERVER_INFO", "REQUEST_TRANSACT_PORT", 0);
	if ( server_port == 0 )
	{
		Log("InitServer: 서버포트 정보가 없습니다.\n");
		exit(EXIT_FAILURE);
	}
	
	/* 설정파일을 닫는다. */
	CloseProfile();

	/* epoll을 생성한다. */
    if ( (epoll_fd = epoll_create(MAX_REQUEST_SERVER_USER)) < 0 ) 
    { 
        Log("InitServer: epoll 생성 도중 에러가 발생하였습니다. errno[%d]\n", errno);
		exit(EXIT_FAILURE);
    }
    
	/* 서버 소켓을 생성한다. */
	server_sockfd = GetServerSocket(server_port, MAX_REQUEST_SERVER_USER);
	if ( server_sockfd == -1 )
	{
		Log("InitServer: 서버 소켓 생성에 실패 하였습니다. port[%d] errno[%d]\n", server_port, errno);
		exit(EXIT_FAILURE);
	}
	add_epoll(server_sockfd);
	
	/* 변수를 초기화한다. */
	memset(mdb->user.request_user, 0x00, sizeof(SERVER_USER)*MAX_REQUEST_SERVER_USER);
    mdb->user.request_user_count = 0;
	timeover.tv_sec = 5; timeover.tv_usec = 0;
    unique_datakey = ReadDataKeyFromFile();

    /* Process를 등록한다. */
	if ( (process_id = RegistProcess(_PROC_REQUEST_TRANSACT_)) == -1 )
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
    Log("InitServer: Load Unique Datakey [%ld]\n", unique_datakey);
}

/**
 * 서버를 종료한다.
 */
void interrupt(int sig)
{
    int i;

	/* 연결된 사용자 모두 종료 */
	for ( i = 0 ; i < MAX_REQUEST_SERVER_USER ; i++ )
	{
		if ( mdb->user.request_user[i].sockfd )
			RemoveUser(mdb->user.request_user[i].sockfd);
	}
	CloseSocket(server_sockfd);
	
	/* epoll 제거 */
	close(epoll_fd);
	
	/* 프로세스 등록 해제 */
	RemoveProcess(_PROC_REQUEST_TRANSACT_);
	DeAttachShm((void*)mdb);
	
    exit(EXIT_SUCCESS);
}
