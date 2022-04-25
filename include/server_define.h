/*****************************************************
 * 프로그램ID	: server_define.h
 * 프로그램명	: 서버의 일반정의를 가지고 있는다.
 *****************************************************/

#ifndef _SERVER_DEFINE_H
#define _SERVER_DEFINE_H

/******************************************************************************
 * System Common....
 ******************************************************************************/

#define _REAL_SERVICE                   'R'                 /* Real Server */
#define _TEST_SERVICE                   'T'                 /* Dev  Server */
#define __SERVICE                       _REAL_SERVICE

/******************************************************************************
 * Program Infomation
 ******************************************************************************/

#define PROGRAM_NAME					"KASConn"

#define CONF_PATH						"conf"
#define DATA_PATH						"data"
#define LOG_PATH						"log"
#define LIB_PATH						"lib"

#define SERVER_CONFIG_FILE				"server.conf"
#define PROCESS_LIST_FILE				"process.dat"

#define NODE_BASE						"/usr/local/nodejs/bin/node"

/******************************************************************************
 * Key Define...
 ******************************************************************************/

#define COMMON_SHM_KEY					0x9800

#define SYSTEM_STATUS_WAITING           0
#define SYSTEM_STATUS_OPEN              1

#define NODE_INTERFACE_KLAYTN		    "node/klaytn.js"
#define NODE_INTERFACE_KAS			    "node/KAS.js"

/******************************************************************************
 * Max Define...
 ******************************************************************************/

#define MAX_BUFFER						4096
#define MAX_PROCESS						16

#define MAX_INQUIRY_SERVER_USER         64
#define MAX_REQUEST_SERVER_USER         64
#define MAX_RESPONSE_SERVER_USER        64
#define MAX_SEND_SERVER_USER            64

/******************************************************************************
 * Process Define...
 ******************************************************************************/

#define _PROC_INQUIRY_CALL_             1
#define _PROC_REQUEST_TRANSACT_			2
#define _PROC_RESPONSE_TRANSACT_        3
#define _PROC_EXEC_TRANSACT_		    4
#define _PROC_SEND_NODE_EVENT_          5

/******************************************************************************
 * Variable Types Define....
 ******************************************************************************/
#include <type_define.h>

#endif
