/*****************************************************
 * 프로그램ID	: KASConn_interface.h
 * 프로그램명	: 통신 스펙을 정의한다.
 *****************************************************/
 
#ifndef _KASCONN_INTERFACE_H
#define _KASCONN_INTERFACE_H


/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define KI_BODY_LENGTH					5

#define KI_REQTYPE_LOGIN				'L'
#define KI_REQTYPE_CALL					'C'
#define KI_REQTYPE_TRANSACT				'T'
#define KI_REQTYPE_EVENT				'E'

#define KI_ACCOUNTYPE_KLAYTN			'C'
#define KI_ACCOUNTYPE_KAS				'K'

/******************************************************************************
 * INTERFACE Header Info (stream)
 ******************************************************************************/

/* REQUEST/RESPONSE HEADER */
typedef struct
{
	char				trid;										/* 요청구분 */
	char				req_type;									/* 요청종류 (T: Transact, C: Call(View)) */
	char				service_name[16];							/* 서비스구분자 (요청 서비스를 구분하기 위해서 사용한다) */
	char				account_type;								/* 계좌구분 (C: 일반, K: KAS Account Poll) */
	char				account_group[16];							/* 계좌그룹 */
	char				user_key[32];								/* 사용자키 */
	char				body_length[KI_BODY_LENGTH];				/* 데이타부길이 */
} KI_REQRES_HEADER;

/* SEND HEADER */
typedef struct
{
	char				trid;										/* 요청구분 */
	char				req_type;									/* 요청종류 (T: Transact, C: Call(View)) */
	char				service_name[16];							/* 서비스구분자 (로그인시, 필수입력) */
	char				account_type;								/* 계좌구분 (C: 일반, K: KAS Account Poll) */
	char				account_group[16];							/* 계좌그룹 (로그인시, 모든 계좌그룹 수신을 원하면 * 입력) */
	char				user_key[32];								/* 사용자키 (로그인시, 모든 사용자 수신을 원하면 * 입력) */
	char				data_key[16];								/* 데이타키 */
	char				body_length[KI_BODY_LENGTH];				/* 데이타부길이 */
} KI_SEND_HEADER;

/******************************************************************************
 * INTERFACE Body Info (JSON)
 ******************************************************************************/

/* Login (No Response) */
#define KI_TRID_LOGIN					'L'


/* Create Account (Call) */
#define KI_TRID_CREATE_ACCOUNT			'C'
// Request
//  - cert: KAS Account Pool Name
// Response
//  - success: true or false
//  - msg: 성공시 account address, 실패시 에러메세지


/* Get Balance (Call) */
#define KI_TRID_BALANCEOF				'B'
// Request
//  - address: account address
// Response
//  - success: true or false
//  - msg: 성공시 balance, 실패시 에러메세지


/* Token Trasnfer (Transact) */
#define KI_TRID_TRANSFER				'T'
// Request
//  - sender: 보낼 주소
//  - recipient: 받을 주소
//  - amount: 전손수량
//  - cert: 인증정보 (1.Sender의 PrivateKey, 2.Sender의 암호화된 PrivateKey, 3.KAS Account Pool Name)
// Response
//  - success: true or false
//  - msg: 성공시 datakey(요청 고유값, Send Response Header에 세팅해서 전송), 실패시 에러메세지
// Send Response
//  - success: true or false
//  - msg: 성공시 transactionHash, 실패시 에러메세지

/* Safe Trasnfer (Transact) */
#define KI_TRID_SAFETRANSFER			'S'

/* Rework Contract (Transact) */
//#define KI_TRID_WRITE_RPINFO			'R'
//#define KI_TRID_REWORD_TRANSFER			'W'



/* Node Event */
#define KI_TRID_NODE_EVENT				'N'
// Send Node Event
//  - type: event type
//  - data: event data


#endif
