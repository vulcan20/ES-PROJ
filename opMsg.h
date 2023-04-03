#ifndef __op_MSG_H__
#define __op_MSG_H__

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

typedef enum
{
	CONSOLE_REQUEST_FOR_VERSION 		= 0,
	CONSOLE_REQUEST_BY_AIRCRAFT_ID,
	CONSOLE_REQUEST_FOR_LOG_CONTROL,

} RequestType_t;


typedef struct _ConsoleRequest_s
{
	RequestType_t requestType;
	int32_t params[16];
} ConsoleRequest_t;


typedef struct _pulse msg_header_t;


/* Our real data comes after the header */
typedef struct _opMsg_s
{
    msg_header_t hdr;

    ConsoleRequest_t consoleRequest;

} opMsg_t;

#endif // __op_MSG_H__
