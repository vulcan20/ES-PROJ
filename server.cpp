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

#define VERSION			(1)

#define MQNAME "/my_opconsole_mq"
#define MSGSIZE 68  // Make sure to size it according to the sizeof(ConsoleRequest_t)


#define MAX_AIRCRAFT_TO_TRACK		(100)

/* This is the name that will be used by the server to create a channel and used by
 * the client to connect to the appropriated channel */
//#define ATTACH_POINT "myname"
#define ATTACH_POINT_RADAR "radar"

#define ATTACH_POINT_OP_MSG "op_msg_channel"

#define ATTACH_POINT_DISPLAY_MSG "display_msg_channel"

//
// Message Queue
//
static mqd_t mq;

/* We specify the header as being at least a pulse */
/* All of your messages should start with this header */
/* This struct contains a type/subtype field as the first 4 bytes.
 * The type and subtype field can be used to identify the message that is being received.
/ * This allows you to identify data which isn't destined for your server. */
///typedef struct _pulse msg_header_t;



/* Our real data comes after the header */
/***
typedef struct _my_data {
    msg_header_t hdr;
    int data;
} my_data_t;
***/

static Aircraft_t most_recent_aircraft_log[MAX_AIRCRAFT_TO_TRACK];
static uint32_t nAircraftsBeingTracked = 0;

static uint32_t KEEP_RUNNING_COMPUTER_SERVER = 1;

static void _print_plane_info(const Aircraft_t planeInfo)
{
	printf("\nAV ID = 0x%08X update rcvd..\n", (uint32_t)planeInfo.id);
	printf("Position (x, y, z) = (%lf, %lf, %lf)\n", planeInfo.position[0], planeInfo.position[1], planeInfo.position[2]);
	printf("Velocity (x, y, z) = (%lf, %lf, %lf)\n", planeInfo.velocity[0], planeInfo.velocity[1], planeInfo.velocity[2]);
}

//std::vector<Aircraft_t> aircraftVector;

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
	}
}

// thread functions
void* process_radar_messages_thread(void* arg)
{
   name_attach_t *attach;

   RadarMsg_t msg;
   int rcvid;

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
	   //rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
	   rcvid = MsgReceive(attach->chid, &msg, sizeof(RadarMsg_t), NULL);

	   /* In the above call, the received message will be stored at msg when the server receives a message.
		* Moreover, rcvid */

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

	   /* Here are the messages that you will work on, i.e., messages that will have a meaning for
		* your application. Let's assume that you have one message (e.g., data to be displayed) and several subtypes.
		* Thus, we first test to check if it is a message we expect. Next, we can have a switch that check
		* what is the subtype of the message. In your project, for instance, you can have a subtype for each
		* variable, e.g., (0x01 - speed, 0x02 - temperature, 0x03 - gear, and so on...).
		* Then, based on the subtype the server is receiving, it would display the information
		* contained in msg.data in the proper place, e.g., at the proper location in a GUI.
		* You can use that as well to work on the output your thread should provide.
		*
		* In addition, you might have another type of message. For instance, you might have a type of message
		* that would be used for configuration (e.g., type==0x01 used for configuration and type==0x00 for data).
		* This can be used to implement the mechanism to change the period of your consumer thread (server).
		* For instance, let's assume that you implemented this server in a separate thread and, instead of having
		* a forever loop, you implement the thread as a periodic task as we have seen (this is not necessary in this
		* case because the server will block when waiting for a message, i.e., it will not be hogging CPU.). Then, the
		* configuratin message could be used to send the period/frequency in which the client would be sending messages.
		* Thus, the server could consider that while doing other things. */
	   if (msg.hdr.type == 0x00)
	   {
		  if (msg.hdr.subtype == 0x01)
		  {
			  /* A message (presumable ours) received, handle */
			  //printf("Server receive %d \n", msg.data);
			  printf("Server received AircraftID  = 0x%08X\n", (uint32_t)msg.aircraftInfo.id);
			  //_print_plane_info(msg.aircraftInfo);

			  //
			  // Action - 1: Write the updated info to the global array of aircrafts that matches the ID
			  //
			  _update_most_recent_info(msg.aircraftInfo);

			  //
			  // Action - 2: If logging flag is set from the input console, save the aircraft info into file
			  //

			  //
			  // Action - 3: Perform calculations for constaint validation
			  //


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
		   /* Server will block in this call, until a client calls MsgSend to send a message to
			* this server through the channel named "myname", which is the name that we set for the channel,
			* i.e., the one that we stored at ATTACH_POINT and used in the name_attach call to create the channel. */
		   //rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
		   rcvid = MsgReceive(attach->chid, &msg, sizeof(opMsg_t), NULL);

		   /* In the above call, the received message will be stored at msg when the server receives a message.
			* Moreover, rcvid */

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

		   //
		   // Own Application messages
		   //
		   if (msg.hdr.type == 0x00)
		   {
			  if (msg.hdr.subtype == 0x01)
			  {
				  /* A message (presumable ours) received, handle */
				  //printf("Server receive %d \n", msg.data);
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
	uint8_t mq_rcvdBytes[68]; // sizeof(ConsoleRequest_t);
	ConsoleRequest_t consoleRequest;
	DisplayMsg_t displayMsg;
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
			displayMsg.displayInfo.displayType = DISPLAY_TYPE_SW_VERSION;
			displayMsg.displayInfo.swVersion = VERSION;
			break;

		case CONSOLE_REQUEST_BY_AIRCRAFT_ID:
			displayMsg.displayInfo.displayType = DISPLAY_TYPE_AIRCRAFT_INFO;
			_get_most_recent_aircraft_info(consoleRequest.params[0], &aircraftInfoToReport);
			break;

		case CONSOLE_REQUEST_FOR_LOG_CONTROL:
			//
			// @ TODO
			//
			break;

		default:
			displayMsg.displayInfo.displayType = DISPLAY_TYPE_UNDEFINED;
			break;
		} // end of switch(consoleRequest.requestType)

		//
		// Now send the display info the the display subsystem
		//

		displayMsg.hdr.type = 0x00;
		displayMsg.hdr.subtype = 0x01;

		if (MsgSend(server_coid, &displayMsg, sizeof(DisplayMsg_t), NULL, 0) == -1)
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
	if (mq == -1) {
		perror("mq_open");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{


    printf("Running Server ... \n");

    init_OpConsole_To_Display_message_queue();

    pthread_t thread_id_for_radar_interface;
    pthread_create(&thread_id_for_radar_interface, NULL, &process_radar_messages_thread, NULL);

    pthread_t thread_id_for_console_interface;
    pthread_create(&thread_id_for_console_interface, NULL, &console_input_messages_thread, NULL);

    pthread_t thread_id_for_display_interface;
    pthread_create(&thread_id_for_display_interface, NULL, &display_messages_thread, NULL);

    pthread_t thread_id_for_communication_interface;
    pthread_create(&thread_id_for_communication_interface, NULL, &communication_subsystem_thread, NULL);

    //
    // Wait for the threads to finish
    //
    pthread_join(thread_id_for_radar_interface, NULL);
    pthread_join(thread_id_for_console_interface, NULL);
    pthread_join(thread_id_for_display_interface, NULL);
    pthread_join(thread_id_for_communication_interface, NULL);

    return 0;
}
