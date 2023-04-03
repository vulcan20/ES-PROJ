#ifndef __DISPLAY_MSG_H__
#define __DISPLAY_MSG_H__

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

#include <vector>

#include "../Aircraft/Aircraft.h"

typedef struct _pulse msg_header_t;

typedef enum
{
	DISPLAY_TYPE_UNDEFINED = -1,
	DISPLAY_TYPE_SW_VERSION = 0,
	DISPLAY_TYPE_AIRCRAFT_INFO,
	DISPLAY_TYPE_ROUTE_ALARM,

} DisplayType_t;

typedef struct _DisplayInfo_s
{
	DisplayType_t displayType;
	int32_t swVersion;				// DISPLAY_TYPE_AIRCRAFT_INFO
	Aircraft_t aircraftInfoToReport; // DISPLAY_TYPE_AIRCRAFT_INFO
	uint32_t  aircraftIdsForAlarm[2]; // DISPLAY_TYPE_ROUTE_ALARM

} DisplayInfo_t;

/* Our real data comes after the header */
typedef struct _DisplayMsg_s
{
    msg_header_t hdr;

    DisplayInfo_t displayInfo;

} DisplayMsg_t;

#endif //__DISPLAY_MSG_H__
