#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <mqueue.h>

#include <vector>

#include "Aircraft/Aircraft.h"
#include "RadarMsg/RadarMsg.h"

#include "opMsg/opMsg.h"
#include "DisplayMsg/DisplayMsg.h"


#include <fcntl.h> //LOG FILE

#include <math.h>


#define VERSION			(1)

#define MQNAME "/my_opconsole_mq"
#define MQNAME_ALARM "/alarm_mq_disp"
#define MSGSIZE sizeof(ConsoleRequest_t)  // Make sure to size it according to the sizeof(ConsoleRequest_t)
#define MSGSIZE_ALARM 16  // Make sure to size it according to the sizeof(AlarmPlaneIds_t)


#define MAX_AIRCRAFT_TO_TRACK		(500)

/* This is the name that will be used by the server to create a channel and used by
 * the client to connect to the appropriated channel */

#define ATTACH_POINT_RADAR "radar"

#define ATTACH_POINT_OP_MSG "op_msg_channel"

#define ATTACH_POINT_DISPLAY_MSG "display_msg_channel"

#define ATTACH_POINT_DISPLAY_ALARM_MSG "display_alarm_msg_channel"


#define DISTANCE_CONSTRAINT_FEET	(9846.0) //minimum distance of safety violation is assumed to be approx 3 km.


FILE *logfile_fd; //LOG FILE


//
// Message Queue
//
static mqd_t mq;

static mqd_t mq_alarm_msg;


static Aircraft_t most_recent_aircraft_log[MAX_AIRCRAFT_TO_TRACK];
static uint32_t nAircraftsBeingTracked = 0;

static uint32_t KEEP_RUNNING_COMPUTER_SERVER = 1;

typedef enum
{
	CONSTRAINT_VOLTATION_NONE_e 	= 0,
	CONSTRAINT_VOLTATION_PRESENT_e 	= 1,
	CONSTRAINT_VOLTATION_FUTURE_e,

} ConstraintViolationType_t;

typedef struct AlarmPlaneIds_s
{
	ConstraintViolationType_t constraintViolationType;
	uint32_t relative_distance_feet;
	uint32_t ids[2];

} AlarmPlaneIds_t;


static double _get_linear_distance(const double* p1, const double* p2)
{
	double linear_dist = sqrt( ( (p1[0] - p2[0])*(p1[0] - p2[0]) ) +
							   ( (p1[1] - p2[1])*(p1[0] - p2[1]) ) +
							   ( (p1[2] - p2[2])*(p1[0] - p2[2]) ) );
	return linear_dist;
}

static ConstraintViolationType_t Run_Constraint_Validation(const uint32_t nAircrafts,
		const Aircraft_t* aircraft_log,
		uint32_t* relative_distance_feet,
		uint32_t* idsFailedConstraints)
{
	// Returns CONSTRAINT_VOLTATION_NONE_e if there is a present constraint violation
	// Returns CONSTRAINT_VOLTATION_PRESENT_e if there is a present constraint violation
	// Returns CONSTRAINT_VOLTATION_FUTURE_e if there is a future constraint violation

	uint32_t i = 0, j = 0;

	for(i = 0; i < nAircrafts; i++)
	{
		for(j = i+1; j < nAircrafts; j++)
		{
			double sqrt_dist_present = _get_linear_distance(aircraft_log[i].position, aircraft_log[j].position);

			if(sqrt_dist_present < DISTANCE_CONSTRAINT_FEET)
			{
				*relative_distance_feet = sqrt_dist_present;
				idsFailedConstraints[0] = aircraft_log[i].id;
				idsFailedConstraints[1] = aircraft_log[j].id;
				return CONSTRAINT_VOLTATION_PRESENT_e;
			}

			//
			// @TODO: Calculate for future violation of 3 minutes
			// using s = s0 + vt
		}
	} // end of for(i = 0; i < nAircrafts; i++)

	return CONSTRAINT_VOLTATION_NONE_e;
}

static void _
_plane_info(const Aircraft_t aircraftInfo)
{
	//
	// If file is not opened yet, open it here first
	//
	if(logfile_fd == NULL)
	{
		logfile_fd = fopen("C:\\Users\\user\\ide-7.1-workspace\\computersystem\\src\\logfile.txt", "a+");
		if (logfile_fd == NULL)
		{
			printf("Failed to open log file.\n");
			//exit(EXIT_FAILURE);
			return;
		}
	}

	//LOG FILE

	fprintf(logfile_fd, "\nAV ID = 0x%08X update rcvd..\n", (uint32_t)aircraftInfo.id);
	fprintf(logfile_fd, "Position (x, y, z) = (%lf, %lf, %lf)\n", aircraftInfo.position[0], aircraftInfo.position[1], aircraftInfo.position[2]);
	fprintf(logfile_fd, "Velocity (x, y, z) = (%lf, %lf, %lf)\n", aircraftInfo.velocity[0], aircraftInfo.velocity[1], aircraftInfo.velocity[2]);

	fflush(logfile_fd);

	// close the file before returning
	fclose(logfile_fd);
	logfile_fd = NULL;
}

static void _update_most_recent_info(const Aircraft_t aircraftInfo)
{
	int isAircraftFound = 0;

	for(int i = 0; i < MAX_AIRCRAFT_TO_TRACK; i++)
	{
		if(aircraftInfo.id == most_recent_aircraft_log[i].id)
		{
			isAircraftFound = 1;
			memcpy(&most_recent_aircraft_log[i], &aircraftInfo, sizeof(Aircraft_t));
			break;
		}
	} // for(int i = 0; i < MAX_AIRCRAFT_TO_TRACK; i++)

	if(isAircraftFound == 0) // means we need to add it to the list
	{
		if(nAircraftsBeingTracked < MAX_AIRCRAFT_TO_TRACK)
		{
			memcpy(&most_recent_aircraft_log[nAircraftsBeingTracked], &aircraftInfo, sizeof(Aircraft_t));
			nAircraftsBeingTracked++;
		}
		else
		{
			printf("[WARNING]: Maximum number of aircraft being tracked has been reached! aircraftInfo.id = %u is not \n");
		}
	}

	_print_plane_info(aircraftInfo);

}

static void _get_most_recent_aircraft_info(const uint32_t aircraftIdToSearch, Aircraft_t* aircraftInfo)
{
	int isAircraftFound = 0;

	for(int i = 0; i < MAX_AIRCRAFT_TO_TRACK; i++)
	{
		if(aircraftIdToSearch == most_recent_aircraft_log[i].id)
		{
			isAircraftFound = 1;
			memcpy(aircraftInfo, &most_recent_aircraft_log[i], sizeof(Aircraft_t));
			break;
		}
	} // for(int i = 0; i < MAX_AIRCRAFT_TO_TRACK; i++)

	if(isAircraftFound == 0) // means we need to add it to the list
	{
		printf("[WARNING]: The request aircraft id %d is not found in the tracking log \n");
		aircraftInfo->id = 0;
		fprintf(logfile_fd, "[WARNING]: The requested aircraft ID %d is not found in the tracking log\n", aircraftIdToSearch);

	}
}

// thread functions
void* process_radar_messages_thread(void* arg)
{
	name_attach_t *attach;

   RadarMsg_t msg;
   AlarmPlaneIds_t alarmPlaneIds;
   uint8_t mq_snd_bytes[sizeof(AlarmPlaneIds_t)];

   int rcvid;
   uint32_t relative_dist_ft = 0;

   /* Create a local name (/dev/name/local/...) */
   if ((attach = name_attach(NULL, ATTACH_POINT_RADAR, 0)) == NULL)
   {
	   printf("[ERROR]: name_attach() failed for %s\n", ATTACH_POINT_RADAR);
	   return (void*)EXIT_FAILURE;
   }


   /* Do your MsgReceive's here now with the chid */
   while (KEEP_RUNNING_COMPUTER_SERVER == 1)
   {
	   /* Server will block in this call, until a client calls MsgSend to send a message to
		* this server through the channel named "myname", which is the name that we set for the channel,
		* i.e., the one that we stored at ATTACH_POINT and used in the name_attach call to create the channel. */

	   rcvid = MsgReceive(attach->chid, &msg, sizeof(RadarMsg_t), NULL);


	   if (rcvid == -1) {/* Error condition, exit */
		   break;
	   }

	   if (rcvid == 0) {/* Pulse received */
		   switch (msg.hdr.code) {
		   case _PULSE_CODE_DISCONNECT:
			   /*
				* A client disconnected all its connections (called
				* name_close() for each name_open() of our name) or
				* terminated
				*/
			   ConnectDetach(msg.hdr.scoid);
			   break;
		   case _PULSE_CODE_UNBLOCK:
			   /*
				* REPLY blocked client wants to unblock (was hit by
				* a signal or timed out).  It's up to you if you
				* reply now or later.
				*/
			   break;
		   default:
			   /*
				* A pulse sent by one of your processes or a
				* _PULSE_CODE_COIDDEATH or _PULSE_CODE_THREADDEATH
				* from the kernel?
				*/
			   break;
		   }
		   continue;
	   }

	   /* name_open() sends a connect message, must EOK this */
	   if (msg.hdr.type == _IO_CONNECT ) {
		   MsgReply( rcvid, EOK, NULL, 0 );
		   continue;
	   }

	   /* Some other QNX IO message was received; reject it */
	   if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
		   MsgError( rcvid, ENOSYS );
		   continue;
	   }

	   if (msg.hdr.type == 0x00)
	   {
		  if (msg.hdr.subtype == 0x01)
		  {
			  /* A message (presumable ours) received, handle */
			  printf("Server received AircraftID  = 0x%08X\n", (uint32_t)msg.aircraftInfo.id);
			  //
			  // Action - 1: Write the updated info to the global array of aircrafts that matches the ID
			  //
			  _update_most_recent_info(msg.aircraftInfo);
			  _print_plane_info(msg.aircraftInfo);

			  //
			  // Action - 2: Perform calculations for constraint validation
			  //
			  uint32_t idsFailedConstraints[2];
			  ConstraintViolationType_t constraintFailedFlag = Run_Constraint_Validation(nAircraftsBeingTracked,
					  most_recent_aircraft_log,
					  &relative_dist_ft,
					  idsFailedConstraints);
			  if(constraintFailedFlag != CONSTRAINT_VOLTATION_NONE_e) // Constraint validation failed for 2 Aircrafts
			  {
				  alarmPlaneIds.constraintViolationType = constraintFailedFlag;

				  alarmPlaneIds.relative_distance_feet = relative_dist_ft;
				  alarmPlaneIds.ids[0] = idsFailedConstraints[0];
				  alarmPlaneIds.ids[1] = idsFailedConstraints[1];

				  //printf("[DEBUG] Warning print (ID1, ID2) = 0x%08X, 0x%08X\n", idsFailedConstraints[0], idsFailedConstraints[1]);

				  //
				  // Add the alarm message to the alarm mq
				  //
				  memcpy(&mq_snd_bytes[0], &alarmPlaneIds, sizeof(AlarmPlaneIds_t) );
				  int retCode = mq_send(mq_alarm_msg, (char*)mq_snd_bytes, sizeof(AlarmPlaneIds_t), 0);
				  if(retCode == -1)
				  {
						printf("mq_send() failed for alarm message! RetCode = %d\n", retCode);
				  }

			  } // End of if(constraintFailed != 0)

		  }
	   }

	   MsgReply(rcvid, EOK, 0, 0);

   } // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

   /* Remove the name from the space */
   name_detach(attach, 0);

   return (void*)EXIT_SUCCESS;
}

void* console_input_messages_thread(void* arg)
{
	   name_attach_t *attach;

	   opMsg_t msg;

	   int rcvid;

	   uint8_t consoleRequestBytes[68]; // = sizeof(ConsoleRequest_t)

	   /* Create a local name (/dev/name/local/...) */
	   if ((attach = name_attach(NULL, ATTACH_POINT_OP_MSG, 0)) == NULL)
	   {
		   printf("[ERROR]: name_attach() failed for %s\n", ATTACH_POINT_OP_MSG);
		   return (void*)EXIT_FAILURE;
	   }

	   /* Do your MsgReceive's here now with the chid */
	   while (KEEP_RUNNING_COMPUTER_SERVER == 1)
	   {

		   rcvid = MsgReceive(attach->chid, &msg, sizeof(opMsg_t), NULL);

		   if (rcvid == -1) {/* Error condition, exit */
			   break;
		   }

		   if (rcvid == 0) {/* Pulse received */
			   switch (msg.hdr.code) {
			   case _PULSE_CODE_DISCONNECT:


				   ConnectDetach(msg.hdr.scoid);
				   break;
			   case _PULSE_CODE_UNBLOCK:

				   break;
			   default:

				   break;
			   }
			   continue;
		   }

		   /* name_open() sends a connect message, must EOK this */
		   if (msg.hdr.type == _IO_CONNECT ) {
			   MsgReply( rcvid, EOK, NULL, 0 );
			   continue;
		   }

		   /* Some other QNX IO message was received; reject it */
		   if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
			   MsgError( rcvid, ENOSYS );
			   continue;
		   }

		   //
		   // Own Application messages
		   //
		   if (msg.hdr.type == 0x00)
		   {
			  if (msg.hdr.subtype == 0x01)
			  {
				  /* A message (presumable ours) received, handle */

				  printf("Processing message type: %d with param = %d\n",
						  (int)msg.consoleRequest.requestType,
						  msg.consoleRequest.params[0]);

				  // Add the ConsoleRequest to mqueue
				  memcpy(&consoleRequestBytes[0], &msg.consoleRequest, sizeof(ConsoleRequest_t) );
				  int retCode = mq_send(mq, (char*)consoleRequestBytes, sizeof(ConsoleRequest_t), 0);
				  if(retCode == -1)
				  {
						printf("mq_send() failed! RetCode = %d\n", retCode);
				  }
			  }
		   }

		   MsgReply(rcvid, EOK, 0, 0);

	   } // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

	   /* Remove the name from the space */
	   name_detach(attach, 0);

	   return (void*)EXIT_SUCCESS;
}

void* display_messages_thread(void* arg)
{
	int retCode = 0;
	uint8_t mq_rcvdBytes[sizeof(ConsoleRequest_t)]; // sizeof(ConsoleRequest_t);
	ConsoleRequest_t consoleRequest;

	DisplayInfo_t displayInfo;
	Aircraft_t aircraftInfoToReport;

	int server_coid; //server connection ID. // server ID for handle server

	if ((server_coid = name_open(ATTACH_POINT_DISPLAY_MSG, 0)) == -1)
	{
		printf("[ERROR]: name_open() failed for %s\n", ATTACH_POINT_DISPLAY_MSG);
		return (void*)EXIT_FAILURE;
	}

	while (KEEP_RUNNING_COMPUTER_SERVER == 1)
	{
		//delay(5000);
		// Pickup the ConsoleRequest from mqueue
		// And process the display message for the respective ConsoleRequest.
		//
		retCode = mq_receive(mq, (char*)mq_rcvdBytes, MSGSIZE, NULL);
		if(retCode == -1)
		{
			printf("mq_receive() failed!\n");
		}

		memcpy(&consoleRequest, mq_rcvdBytes, sizeof(ConsoleRequest_t));

		//
		// Based on the type of request, prepare the DisplayMsg_t message and Msg_send to the channel for Display
		//
		switch(consoleRequest.requestType)
		{
		case CONSOLE_REQUEST_FOR_VERSION:
			displayInfo.displayType = DISPLAY_TYPE_SW_VERSION;
			displayInfo.swVersion = VERSION;
			break;

		case CONSOLE_REQUEST_BY_AIRCRAFT_ID:
			displayInfo.displayType = DISPLAY_TYPE_AIRCRAFT_INFO;
			_get_most_recent_aircraft_info(consoleRequest.params[0], &aircraftInfoToReport);

			memcpy(&displayInfo.aircraftInfoToReport, &aircraftInfoToReport, sizeof(Aircraft_t));
			break;

		case CONSOLE_REQUEST_FOR_LOG_CONTROL:
			//
			// @ TODO
			//
			break;

		default:
			displayInfo.displayType = DISPLAY_TYPE_UNDEFINED;
			break;
		} // end of switch(consoleRequest.requestType)

		//
		// Now send the display info the the display subsystem
		//

		//displayMsg.hdr.type = 0x00;
		//displayMsg.hdr.subtype = 0x01;

		if (MsgSend(server_coid, &displayInfo, sizeof(DisplayInfo_t), NULL, 0) == -1)
		{
			return (void*)EXIT_FAILURE;
		}

	} // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

	return (void*)EXIT_SUCCESS;
}

void* alarm_messages_thread(void* arg)
{
	int retCode = 0;
	uint8_t mq_rcvdBytes[MSGSIZE_ALARM]; // sizeof(DisplayMsg_t);

	DisplayInfo_t displayInfo;
	AlarmPlaneIds_t alarmPlaneIds;

	int server_coid; //server connection ID.

	if ((server_coid = name_open(ATTACH_POINT_DISPLAY_ALARM_MSG, 0)) == -1)
	{
		printf("[ERROR]: name_open() failed for %s\n", ATTACH_POINT_DISPLAY_MSG);
		return (void*)EXIT_FAILURE;
	}

	while (KEEP_RUNNING_COMPUTER_SERVER == 1)
	{
		//delay(5000);
		// Pickup the alarm display message from mqueue
		// And process the alarm display message ()
		//
		retCode = mq_receive(mq_alarm_msg, (char*)mq_rcvdBytes, MSGSIZE_ALARM, NULL);
		if(retCode == -1)
		{
			printf("mq_receive() failed in alarm_messages_thread()!\n");
			return (void*)EXIT_FAILURE;
		}

		memcpy(&alarmPlaneIds, mq_rcvdBytes, sizeof(AlarmPlaneIds_t));

		//displayMsg.hdr.code = 0;
		//displayMsg.hdr.subtype = 1;

		if(alarmPlaneIds.constraintViolationType == CONSTRAINT_VOLTATION_PRESENT_e)
		{
			displayInfo.displayType = DISPLAY_TYPE_ROUTE_ALARM_PRESENT;
		}
		else if(alarmPlaneIds.constraintViolationType == CONSTRAINT_VOLTATION_FUTURE_e)
		{
			displayInfo.displayType = DISPLAY_TYPE_ROUTE_ALARM_FUTURE;
		}

		displayInfo.rel_dist = alarmPlaneIds.relative_distance_feet;
		displayInfo.aircraftIdsForAlarm[0] = alarmPlaneIds.ids[0];
		displayInfo.aircraftIdsForAlarm[1] = alarmPlaneIds.ids[1];

		//
		// Send the alarm message channel for display
		//

		if (MsgSend(server_coid, &displayInfo, sizeof(DisplayInfo_t), NULL, 0) == -1)
		{
			return (void*)EXIT_FAILURE;
		}

	} // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

	return (void*)EXIT_SUCCESS;
}


void* communication_subsystem_thread(void* arg)
{
	while (KEEP_RUNNING_COMPUTER_SERVER == 1)
	{
		delay(5000);
	} // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

	return (void*)EXIT_SUCCESS;
}


int init_OpConsole_To_Display_message_queue(void) // returns on on success
{
	struct mq_attr attr;
	attr.mq_maxmsg = 128;  //maximum number of messages that the queue can hold
	attr.mq_msgsize = MSGSIZE; //maximum size of each message in the queue

	mq = mq_open(MQNAME, O_CREAT | O_RDWR, 0666, &attr);
	if (mq == -1)
	{
		perror("mq_open");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}

int init_alarm_display_message_queue(void)
{
	//
	// Message queue for alarm messages
	//
	struct mq_attr attr_alarm;
	attr_alarm.mq_maxmsg = 64;  //maximum number of messages that the queue can hold
	attr_alarm.mq_msgsize = MSGSIZE_ALARM; //maximum size of each message in the queue

	mq_alarm_msg = mq_open(MQNAME_ALARM, O_CREAT | O_RDWR, 0666, &attr_alarm);
	if (mq_alarm_msg == -1)
	{
		perror("mq_open failed for mq_alarm_msg");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
    printf("Running Server ... \n");

    init_OpConsole_To_Display_message_queue();
    init_alarm_display_message_queue();

    pthread_t thread_id_for_radar_interface;
    pthread_create(&thread_id_for_radar_interface, NULL, &process_radar_messages_thread, NULL);

    pthread_t thread_id_for_console_interface;
    pthread_create(&thread_id_for_console_interface, NULL, &console_input_messages_thread, NULL);

    pthread_t thread_id_for_display_interface;
    pthread_create(&thread_id_for_display_interface, NULL, &display_messages_thread, NULL);

    pthread_t thread_id_for_alarm_msg_interface;
    pthread_create(&thread_id_for_alarm_msg_interface, NULL, &alarm_messages_thread, NULL);

    pthread_t thread_id_for_communication_interface;
    pthread_create(&thread_id_for_communication_interface, NULL, &communication_subsystem_thread, NULL);

    //
    // Wait for the threads to finish
    //
    pthread_join(thread_id_for_radar_interface, NULL);
    pthread_join(thread_id_for_console_interface, NULL);
    pthread_join(thread_id_for_display_interface, NULL);
    pthread_join(thread_id_for_alarm_msg_interface, NULL);
    pthread_join(thread_id_for_communication_interface, NULL);

    return 0;
}
