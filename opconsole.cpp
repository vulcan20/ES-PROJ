#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

#include "Aircraft/Aircraft.h"
#include "opMsg/opMsg.h"

#define ATTACH_POINT "op_msg_channel"

static void _print_help(void) {
	printf("\nUSAGE:\n");
	printf(" pp=<planeID>\n");
	printf(" help\n");
	printf(" exit\n");
} //todo for comm system

static int _parse_operator_input(const char *inputChar,
		RequestType_t *requestType, int *params) {
	// Returns 0 on success

	int inputPlaneID = 0;
	int nConverted = sscanf(inputChar, "pp=%X", &inputPlaneID);
	if (nConverted != 1) {
		printf("[ERROR]: Incorrect command format. nConverted = %d\n",
				nConverted);
		return -1;
	}

	// Assign output before returning
	*requestType = CONSOLE_REQUEST_BY_AIRCRAFT_ID;
	params[0] = inputPlaneID;

	//printf("[DEBUG]: params[0] = 0x%08X [%d]\n", (uint32_t)params[0], params[0]);
	return 0;
}

/*** Client Side of the code ***/
int client() {
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
	int retCode = 0;
	RequestType_t requestType = CONSOLE_REQUEST_NOT_DEFINED;
	int requestParams[4];

	while (1) {
		printf("Input command: ");
		fgets(opInput, sizeof(opInput), stdin);

		if (strncmp(opInput, "help", strlen("help")) == 0) {
			_print_help();
			continue;
		}

		if (strncmp(opInput, "exit", strlen("exit")) == 0) {
			printf("[INFO] Exiting operator's input console service.\n");
			break;
		}

		retCode = _parse_operator_input(opInput, &requestType, requestParams);
		if (retCode != 0) {
			// Retry user input
			printf("[ERROR] _parse_operator_input() failed!\n");
			continue;
		}

		// At this point, RequestType_t requestType and requestParams have valid entries
		switch (requestType) {
		case CONSOLE_REQUEST_BY_AIRCRAFT_ID:
			msg.consoleRequest.requestType = CONSOLE_REQUEST_BY_AIRCRAFT_ID;
			msg.consoleRequest.params[0] = requestParams[0];

			//
			// Send the request to the displaysys server channel
			//
			if (MsgSend(server_coid, &msg, sizeof(msg), NULL, 0) == -1) {
				return EXIT_FAILURE;
			}

			break;

		default:
			break;

		} // End of switch(requestType)

	} // End of while(1)

	/* Close the connection */
	name_close(server_coid); // another client can connect to that server
	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	int ret;

	printf("Running Client ... \n");
	ret = client(); /* see name_open() for this code */

	return ret;
}
