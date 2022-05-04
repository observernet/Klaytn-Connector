/*****************************************************
 * 프로그램ID	: InquiryCall.c
 * 프로그램명	: 조회용 요청을 받아 응답해준다
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

#include <pthread.h>

#include <network.h>
#include <profile.h>
#include <ft_common.h>

#include <KASConn_interface.h>
#include "InquiryCall.h"

/*************************************************************************************
 * 구현 시작..
 *************************************************************************************/
 
int main(int argc, char** argv)
{
	int i, n, nread;
	
	SetProgramName(argv[0]);
	
	/* 서버를 초기화 한다. */
	InitServer();
    InitThread();
    
	while ( 1 )
    {
    	/* 소켓이벤트를 체크한다. (0.01초) */
    	n = epoll_wait(epoll_fd, events, MAX_INQUIRY_SERVER_USER, 10);
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
    char *rcvbuf, tmpbuf[16];
    KI_REQRES_HEADER* reqHeader;

    int user_offset = GetUserOffset(sockfd);
    if ( user_offset == -1 )
    {
        Log("ReceiveRequest: 사용자 정보를 찾을수 없습니다. sockfd[%d]\n", sockfd);
        del_epoll(sockfd); CloseSocket(sockfd);
        return (-1);
    }

	/* 사이즈를 읽어온다 읽어온다. */
    rcvbuf = (char*)calloc(MAX_PACKET, sizeof(char));
    if ( (res = ReceiveTCP(sockfd, rcvbuf, sizeof(KI_REQRES_HEADER), &timeover)) <= 0 )
    {
        Log("ReceiveRequest: 헤더 수신에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.inquiry_user[user_offset].service_name, sockfd, errno);
        RemoveUser(sockfd);
        free(rcvbuf);
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
            Log("ReceiveRequest: 데이타 수신에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.inquiry_user[user_offset].service_name, sockfd, errno);
            RemoveUser(sockfd);
            free(rcvbuf);
            return (-1);
        }
        rcv += res;
    }

    /* 로그를 기록한다 */
    Log("ReceiveRequest: service[%s] rcvbuf[%d:%s]\n", mdb->user.inquiry_user[user_offset].service_name, strlen(rcvbuf), rcvbuf);
    time(&mdb->user.inquiry_user[user_offset].polling_time);

    /* 콜 요청만 처리한다 */
    if ( reqHeader->req_type != KI_REQTYPE_CALL )
    {
        Log("ReceiveRequest: 트랜잭션 요청만 허용합니다. service[%s] req_type[%c]\n", mdb->user.inquiry_user[user_offset].service_name, reqHeader->req_type);
        SendErrorMessage(user_offset, reqHeader, 'N', "허용되지 않은 요청종류입니다 (콜만 허용)");
        free(rcvbuf);
        return (-1);
    }

    /* 시스템 상태를 체크한다 */
    if ( mdb->system_status != SYSTEM_STATUS_OPEN )
    {
        Log("ReceiveRequest: 접수 대기 상태입니다. service[%s] req_type[%c]\n", mdb->user.inquiry_user[user_offset].service_name, reqHeader->req_type);
        SendErrorMessage(user_offset, reqHeader, 'N', "현재 접수 대기중 상태입니다");
        free(rcvbuf);
        return (-1);
    }

    /* 쓰레드에 처리를 요청한다 */
    if ( AllocateThread(user_offset, reqHeader, rcvbuf) == -1 )
    {
        Log("ReceiveRequest: 쓰레드 할당에 실패하였습니다. service[%s] req_type[%c] errno[%d]\n", mdb->user.inquiry_user[user_offset].service_name, reqHeader->req_type, errno);
        SendErrorMessage(user_offset, reqHeader, 'N', "시스템 에러!");
        free(rcvbuf);
        return (-1);
    }

    return (0);
}

/**
 * 처리할 쓰레드를 할당한다
 */
int AllocateThread(int user_offset, KI_REQRES_HEADER* reqHeader, char* rcvbuf)
{
    int i, j, q_idx;
	int alloc_thnum;

	/* 먼저 해당쓰레드에 이미 처리중인 계좌그룹이 있는지 체크한다. */
	alloc_thnum = -1;
	for ( i = 0 ; i < thread_count ; i++ )
	{
		for ( j = thread[i].read_ptr ; j < thread[i].write_ptr ; j++ )
		{
			q_idx = j % MAX_THREAD_QUE_SIZE;
			if ( strncmp(reqHeader->account_group, ((KI_REQRES_HEADER*)(thread[i].q[q_idx].rcvbuf))->account_group, 16) == 0 )
			{
				alloc_thnum = i;
				break;
			}
		}
		
		if ( alloc_thnum >= 0 ) break;
	}
	
	/* 처리중인 계좌그룹이 없다면, 제일 한가한 쓰레드에 분배한다. */
	if ( alloc_thnum < 0 )
	{
		int q_idx_1, q_idx_2;
		
		alloc_thnum = 0;
		for ( i = 1 ; i < thread_count ; i++ )
		{
			q_idx_1 = thread[i].read_ptr % MAX_THREAD_QUE_SIZE;
			q_idx_2 = thread[alloc_thnum].read_ptr % MAX_THREAD_QUE_SIZE;
			
			/* 대기시간 체크 */
			if ( thread[i].q[q_idx_1].request_time < thread[alloc_thnum].q[q_idx_2].request_time )
				alloc_thnum = i;
				
			/* 단순수량비교 */
			//if ( (thread[i].write_ptr - thread[i].read_ptr) < (thread[alloc_thnum].write_ptr - thread[alloc_thnum].read_ptr) )
			//	alloc_thnum = i;
		}
	}
	
	/* 큐크기를 벗어나는지 체크한다. */
	if ( thread[alloc_thnum].write_ptr - thread[alloc_thnum].read_ptr > MAX_THREAD_QUE_SIZE )
	{
		Log("AllocateThread: 쓰레드큐 초과!! thread_num[%d]\n", alloc_thnum);
		return (-1);
	}
	
	/* 할당된 쓰레드에 데이타를 할당한다. */
	q_idx = thread[alloc_thnum].write_ptr % MAX_THREAD_QUE_SIZE;
	thread[alloc_thnum].q[q_idx].user_offset = user_offset;
    thread[alloc_thnum].q[q_idx].rcvbuf = rcvbuf;
	time(&thread[alloc_thnum].q[q_idx].request_time);
	thread[alloc_thnum].write_ptr++;

	return (alloc_thnum);
}

/**
 * 사용자에게 응답을 전송한다
 */
int SendErrorMessage(int user_offset, KI_REQRES_HEADER* reqHeader, char success, char* errmsg)
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
    if ( success == 'Y' ) sprintf(resBody, "{\"success\":true,\"msg\":\"%s\"}", errmsg);
    else                  sprintf(resBody, "{\"success\":false,\"msg\":\"%s\"}", errmsg);

    /* Body Size를 세팅한다 */
    sprintf(tmpbuf, "%0*ld", KI_BODY_LENGTH, strlen(resBody));
    memcpy(resHeader->body_length, tmpbuf, KI_BODY_LENGTH);
    
    /* 응답데이타를 전송한다 */
    if ( SendTCP(mdb->user.inquiry_user[user_offset].sockfd, sndbuf, strlen(sndbuf), &timeover) == -1 )
    {
        Log("SendErrorMessage: 응답데이타 전송에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", mdb->user.inquiry_user[user_offset].service_name, mdb->user.inquiry_user[user_offset].sockfd, errno);
        return (-1);
    }
    Log("SendErrorMessage: service[%s] sndbuf[%ld:%s]\n", mdb->user.inquiry_user[user_offset].service_name, strlen(sndbuf), sndbuf);

    return (0);
}

/**
 * 콜조회를 실행한다.
 */
void* InquiryCall(void* arg)
{
    int thread_num, q_idx;
    TH_QUE* qdata;
    KI_REQRES_HEADER* reqHeader;
	
	thread_num = *((int*)arg);
	free(arg);
	
	Log("InquiryCall: Thread Start thread_num [%d].................\n", thread_num);

    while ( !thread_terminate )
	{
		/* Thread Que에 데이타가 쌓였는지 체크한다. */
		if ( thread[thread_num].read_ptr >= thread[thread_num].write_ptr )
		{
			usleep(100000);
			continue;
		}
		
		/* 데이타 포인터를 세팅한다. */
		q_idx = thread[thread_num].read_ptr % MAX_THREAD_QUE_SIZE;
        qdata = &thread[thread_num].q[q_idx];
        reqHeader = (KI_REQRES_HEADER*)qdata->rcvbuf;

        /* 계좌구분에 따라 */
        switch ( reqHeader->account_type )
        {
            case KI_ACCOUNTYPE_KLAYTN:
                InquiryCallToKlaytn(thread_num, qdata->user_offset, reqHeader, qdata->rcvbuf + sizeof(KI_REQRES_HEADER));
                break;
            
            case KI_ACCOUNTYPE_KAS:
                InquiryCallToKAS(thread_num, qdata->user_offset, reqHeader, qdata->rcvbuf + sizeof(KI_REQRES_HEADER));
                break;

            default:
                Log("InquiryCall[%d]: 정의되지 않은 계좌구분입니다 account_type[%c]\n", thread_num, reqHeader->account_type);
                SendErrorMessage(qdata->user_offset, reqHeader, 'N', "정의되지 않은 계좌구분");
        }

        /* 큐정보 갱신 */
        free(qdata->rcvbuf); qdata->rcvbuf = NULL;
		qdata->user_offset = 0; qdata->request_time = 0;
		thread[thread_num].read_ptr++;
	}

    Log("InquiryCall: Thread Terminate thread_num [%d].............\n", thread_num);
	
	pthread_exit(0);
	return (NULL);
}

/**
 * 클레이튼에 조회를 요청한다
 */
int InquiryCallToKlaytn(int thread_num, int user_offset, KI_REQRES_HEADER* reqHeader, char* reqBody)
{
    FILE* fp;
    char pipename[MAX_BUFFER], *rcvbuf = NULL;

    char tmpbuf[MAX_BUFFER];
    int ptr = 0, len;

    /* NODE에 트랜잭션을 전송을 요청한다 */
    sprintf(pipename, "%s %s/%s %c '%s'", NODE_BASE, mdb->program_home, NODE_INTERFACE_KLAYTN, reqHeader->trid, reqBody);
    if ( (fp = popen(pipename, "r")) == NULL )
    {
        Log("InquiryCallToKlaytn[%d]: pipe open error [%s]\n", thread_num, pipename);
        SendErrorMessage(user_offset, reqHeader, 'N', "Node 실행 에러!!");
        return (-1);
    }

    while ( 1 )
    {
        memset(tmpbuf, 0x00, MAX_BUFFER);
        if ( fgets(tmpbuf, MAX_BUFFER, fp) == NULL ) break;

        len = strlen(tmpbuf);
        rcvbuf = realloc(rcvbuf, ptr + len);
        memcpy(rcvbuf+ptr, tmpbuf, len);
        ptr = ptr + len;
    }
    pclose(fp);

    /* 수신 데이타가 존재한다면 */
    if ( rcvbuf )
    {
		rcvbuf = realloc(rcvbuf, ptr+1);
	    rcvbuf[ptr] = 0;

        len = strlen(rcvbuf);
        if ( rcvbuf[len-1] == '\n' ) rcvbuf[len-1] = 0;

        /* 사용자에게 응답을 전송한다 */
        SendResponse(thread_num, user_offset, reqHeader, rcvbuf);
        free(rcvbuf);
    }

    return (0);
}

/**
 * KAS에 조회를 요청한다
 */
int InquiryCallToKAS(int thread_num, int user_offset, KI_REQRES_HEADER* reqHeader, char* reqBody)
{
    FILE* fp;
    char pipename[MAX_BUFFER], *rcvbuf = NULL;

    char tmpbuf[MAX_BUFFER];
    int ptr = 0, len;

    /* NODE에 트랜잭션을 전송을 요청한다 */
    sprintf(pipename, "%s %s/%s %c '%s'", NODE_BASE, mdb->program_home, NODE_INTERFACE_KAS, reqHeader->trid, reqBody);
    if ( (fp = popen(pipename, "r")) == NULL )
    {
        Log("InquiryCallToKAS[%d]: pipe open error [%s]\n", thread_num, pipename);
        SendErrorMessage(user_offset, reqHeader, 'N', "Node 실행 에러!!");
        return (-1);
    }

    while ( 1 )
    {
        memset(tmpbuf, 0x00, MAX_BUFFER);
        if ( fgets(tmpbuf, MAX_BUFFER, fp) == NULL ) break;

        len = strlen(tmpbuf);
        rcvbuf = realloc(rcvbuf, ptr + len);
        memcpy(rcvbuf+ptr, tmpbuf, len);
        ptr = ptr + len;
    }
    pclose(fp);

    /* 수신 데이타가 존재한다면 */
    if ( rcvbuf )
    {
		rcvbuf = realloc(rcvbuf, ptr+1);
	    rcvbuf[ptr] = 0;

        len = strlen(rcvbuf);
        if ( rcvbuf[len-1] == '\n' ) rcvbuf[len-1] = 0;

        /* 사용자에게 응답을 전송한다 */
        SendResponse(thread_num, user_offset, reqHeader, rcvbuf);
        free(rcvbuf);
    }

    return (0);
}

/**
 * 사용자에게 응답을 전송한다
 */
int SendResponse(int thread_num, int user_offset, KI_REQRES_HEADER* reqHeader, char* resBody)
{
    char sndbuf[MAX_BUFFER];
    KI_REQRES_HEADER* resHeader;
    
    /* 보낼 헤더를 세팅한다 */
    memset(sndbuf, 0x00, MAX_BUFFER);
    memcpy(sndbuf, reqHeader, sizeof(KI_REQRES_HEADER));
    resHeader = (KI_REQRES_HEADER*)sndbuf;
    sprintf(resHeader->body_length, "%0*ld", KI_BODY_LENGTH, strlen(resBody));
    
    /* 보낼 데이타를 세팅한다 */
    sprintf(sndbuf + sizeof(KI_REQRES_HEADER), "%s", resBody);
    
    /* 응답데이타를 전송한다 */
    if ( SendTCP(mdb->user.inquiry_user[user_offset].sockfd, sndbuf, strlen(sndbuf), &timeover) == -1 )
    {
        Log("SendResponse[%d]: 응답데이타 전송에 실패하였습니다. service[%s] sockfd[%d] errno[%d]\n", thread_num, mdb->user.inquiry_user[user_offset].service_name, mdb->user.inquiry_user[user_offset].sockfd, errno);
        return (-1);
    }
    Log("SendResponse(%d): service[%s] sndbuf[%ld:%s]\n", thread_num, mdb->user.inquiry_user[user_offset].service_name, strlen(sndbuf), sndbuf);

    return (0);
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
	for ( offset = 0 ; offset < MAX_INQUIRY_SERVER_USER ; offset++ )
	{
		if ( mdb->user.inquiry_user[offset].sockfd == 0 )
			break;
	}
	if ( offset >= MAX_INQUIRY_SERVER_USER )
	{	
		Log("AcceptUser: 허용된 최대 사용자가 초과되었습니다.\n");
		CloseSocket(fd);
		return (-1);
	}
	
	/* 사용자 접속 최종 허용 */
	memcpy( &mdb->user.inquiry_user[offset], &user, sizeof(SERVER_USER) );
    mdb->user.inquiry_user_count++;

	add_epoll(mdb->user.inquiry_user[offset].sockfd);
	Log("사용자 접속: offset[%d] sockfd[%d] ip[%s] service[%s] account_group[%s]\n", offset, mdb->user.inquiry_user[offset].sockfd, mdb->user.inquiry_user[offset].user_ip, mdb->user.inquiry_user[offset].service_name, mdb->user.inquiry_user[offset].account_group);
	
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
	del_epoll(mdb->user.inquiry_user[offset].sockfd);
	CloseSocket(mdb->user.inquiry_user[offset].sockfd);
	
	Log("사용자 종료: offset[%d] sockfd[%d] ip[%s] service[%s] account_group[%s]\n", offset, mdb->user.inquiry_user[offset].sockfd, mdb->user.inquiry_user[offset].user_ip, mdb->user.inquiry_user[offset].service_name, mdb->user.inquiry_user[offset].account_group);
	memset( &mdb->user.inquiry_user[offset], 0x00, sizeof(SERVER_USER) );
    mdb->user.inquiry_user_count--;
	
	return (0);
}

/**
 * User Offset을 가져온다
 */
int GetUserOffset(SOCKET sockfd)
{
    int offset;

    for ( offset = 0 ; offset < MAX_INQUIRY_SERVER_USER ; offset++ )
    {
        if ( mdb->user.inquiry_user[offset].sockfd == sockfd )
            return (offset);
    }

    return (-1);
}

/*************************************************************************************
 * 초기화함수
 *************************************************************************************/

/**
 * 뮤텍스, 쓰레드를 초기화한다.
 */
void InitThread()
{
	int i, *arg;	
	
	thread_terminate = false;
	
	/* 쓰레드를 생성한다. */
	if ( thread_count > 0 )
	{
		thread = (TH_INFO*)calloc(thread_count, sizeof(TH_INFO));
		for ( i = 0 ; i < thread_count ; i++ )
		{
			/* 쓰레드생성 */
			arg = (int*)malloc( sizeof(int) ); *arg = i;
			if ( pthread_create(&thread[i].th, NULL, InquiryCall, (void*)arg) != 0 )
			{
				Log("InitThread: 쓰레드 생성에 실패하였습니다. errno[%d]\n", errno);
				exit(EXIT_FAILURE);
			}
		}
	}
}

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
	server_port = GetProfileInt("SERVER_INFO", "INQUIRY_CALL_PORT", 0);
	if ( server_port == 0 )
	{
		Log("InitServer: 서버포트 정보가 없습니다.\n");
		exit(EXIT_FAILURE);
	}
    thread_count = GetProfileInt("INQUIRY_CALL", "THREAD_COUNT", 1);
	
	/* 설정파일을 닫는다. */
	CloseProfile();

	/* epoll을 생성한다. */
    if ( (epoll_fd = epoll_create(MAX_INQUIRY_SERVER_USER)) < 0 ) 
    { 
        Log("InitServer: epoll 생성 도중 에러가 발생하였습니다. errno[%d]\n", errno);
		exit(EXIT_FAILURE);
    }
    
	/* 서버 소켓을 생성한다. */
	server_sockfd = GetServerSocket(server_port, MAX_INQUIRY_SERVER_USER);
	if ( server_sockfd == -1 )
	{
		Log("InitServer: 서버 소켓 생성에 실패 하였습니다. port[%d] errno[%d]\n", server_port, errno);
		exit(EXIT_FAILURE);
	}
	add_epoll(server_sockfd);
	
	/* 변수를 초기화한다. */
	memset(mdb->user.inquiry_user, 0x00, sizeof(SERVER_USER)*MAX_INQUIRY_SERVER_USER);
    mdb->user.inquiry_user_count = 0;
	timeover.tv_sec = 5; timeover.tv_usec = 0;

    /* Process를 등록한다. */
	if ( (process_id = RegistProcess(_PROC_INQUIRY_CALL_)) == -1 )
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
    
	Log("InitServer: 서버 초기화! Process Start [%d] ................................\n", process_id);
    Log("InitServer: TCP Server Port [%d]\n", server_port);
}

/**
 * 서버를 종료한다.
 */
void interrupt(int sig)
{
    int i;

	/* 연결된 사용자 모두 종료 */
	for ( i = 0 ; i < MAX_INQUIRY_SERVER_USER ; i++ )
	{
		if ( mdb->user.inquiry_user[i].sockfd )
			RemoveUser(mdb->user.inquiry_user[i].sockfd);
	}
	CloseSocket(server_sockfd);
	
	/* epoll 제거 */
	close(epoll_fd);
	
	/* 프로세스 등록 해제 */
	RemoveProcess(_PROC_INQUIRY_CALL_);
	DeAttachShm((void*)mdb);
	
    exit(EXIT_SUCCESS);
}
