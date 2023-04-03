#ifndef __RADAR_MSG_H__
#define __RADAR_MSG_H__

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

#include "../Aircraft/Aircraft.h"

typedef struct _pulse msg_header_t;



/* Our real data comes after the header */
typedef struct _radar_msg_s
{
    msg_header_t hdr;

    Aircraft_t aircraftInfo;

} RadarMsg_t;

#endif // __RADAR_MSG_H__
