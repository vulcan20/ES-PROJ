#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

#include <vector>

#include "Aircraft/Aircraft.h"
#include "opMsg/opMsg.h"

#include "DisplayMsg/DisplayMsg.h"

#define ATTACH_POINT "display_msg_channel" //change the attach point


/*** Client Side of the code ***/
int RunDisplayMsgSystem()
{
   name_attach_t *attach;

   DisplayMsg_t msg;
   int rcvid;

   /* Create a local name (/dev/name/local/...) */
   if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL) {
	   return EXIT_FAILURE;
   }

   /* Do your MsgReceive's here now with the chid */
   while (1)
   {
	   /* Server will block in this call, until a client calls MsgSend to send a message to
		* this server through the channel named "myname", which is the name that we set for the channel,
		* i.e., the one that we stored at ATTACH_POINT and used in the name_attach call to create the channel. */
	   //rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
	   rcvid = MsgReceive(attach->chid, &msg, sizeof(DisplayMsg_t), NULL);

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
			  printf("Received display message type: %d\n", (int)msg.displayInfo.displayType);

			  //
			  // TO DO: Implement how to print for different display types
			  //

		  }
	   }

	   MsgReply(rcvid, EOK, 0, 0);

   } // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

   /* Remove the name from the space */
   name_detach(attach, 0);

   return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int ret;


//    if (strcmp(argv[1], "-c") == 0) {
        printf("Running RunDisplayMsgSystem ... \n");
        ret = RunDisplayMsgSystem();   /* see name_open() for this code */
//    }

    return ret;
}
