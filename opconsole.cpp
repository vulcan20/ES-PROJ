#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

#include "Aircraft/Aircraft.h"
#include "opMsg/opMsg.h"


#define ATTACH_POINT "op_msg_channel"




static void _print_operator_instruct(const Aircraft_t planeInfo)
{
	printf("\nPrint info of AV ID = 0x%08X \n", (uint32_t)planeInfo.id);
}//todo

/*** Client Side of the code ***/
int client()
{
    char opInput[128];

	//my_data_t msg;
	opMsg_t msg;
    int server_coid; //server connection ID. // server ID for handle server

    if ((server_coid = name_open(ATTACH_POINT, 0)) == -1) {
        return EXIT_FAILURE;
    }

    /* We would have pre-defined data to stuff here */
    msg.hdr.type = 0x00;
    msg.hdr.subtype = 0x01;

    //
    // Read operator's input
    //
    fgets(opInput, sizeof(opInput), stdin);

    /* Do whatever work you wanted with server connection */
    //for (msg.aircraftInfo.id=0; msg.aircraftInfo.id < 5; msg.aircraftInfo.id++)
    {
    	//msg.aircraftInfo.in_ATC_tracking_range = 1; //if??

    	//_print_operator_instruct(msg.aircraftInfo);

    	//printf("Client sending %d \n", msg.aircraftInfo.id);
        if (MsgSend(server_coid, &msg, sizeof(msg), NULL, 0) == -1) {
            return EXIT_FAILURE;
        }
    }

    /* Close the connection */
    name_close(server_coid); // another client can connect to that server
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int ret;


//    if (strcmp(argv[1], "-c") == 0) {
        printf("Running Client ... \n");
        ret = client();   /* see name_open() for this code */
//    }

    return ret;
}
