#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>

#include <vector>

#include "Aircraft/Aircraft.h"
#include "opMsg/opMsg.h"

#include "DisplayMsg/DisplayMsg.h"

#define ATTACH_POINT "display_msg_channel"

#define ATTACH_POINT_DISPLAY_ALARM_MSG "display_alarm_msg_channel"

FILE *opreq_logfile_fd; //LOG FILE

typedef struct AlarmPlaneIds_s
{
	uint32_t ids[2];

} AlarmPlaneIds_t;

static std::vector<AlarmPlaneIds_t> alarmMsgVector;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

uint32_t g_msg_counter = 0;

// signal handler function
void resetAlarmMsgVector_handler(int signum)
{
	pthread_mutex_lock(&mutex);
	{
		alarmMsgVector.clear();
	}
    pthread_cond_broadcast(&cond); // wake up all waiting threads
    pthread_mutex_unlock(&mutex);
}

static void _print_plane_info(const Aircraft_t aircraftInfo)
{
	if(opreq_logfile_fd == NULL)
		{
		opreq_logfile_fd = fopen("C:\\Users\\user\\ide-7.1-workspace\\displaysys\\src\\opreq_logfile.txt", "a+");
			if (opreq_logfile_fd == NULL)
			{
				printf("Failed to open log file.\n");
				//exit(EXIT_FAILURE);
				return;
			}
		}
	printf("DISPLAYSYS: AV ID = 0x%08X update rcvd..\n", (uint32_t)aircraftInfo.id);
	printf("DISPLAYSYS: Position (x, y, z) = (%lf, %lf, %lf)\n", aircraftInfo.position[0], aircraftInfo.position[1], aircraftInfo.position[2]);
	printf("DISPLAYSYS: Velocity (x, y, z) = (%lf, %lf, %lf)\n", aircraftInfo.velocity[0], aircraftInfo.velocity[1], aircraftInfo.velocity[2]);

	fprintf(opreq_logfile_fd, "\nAV ID = 0x%08X update rcvd..\n", (uint32_t)aircraftInfo.id);
		fprintf(opreq_logfile_fd, "Position (x, y, z) = (%lf, %lf, %lf)\n", aircraftInfo.position[0], aircraftInfo.position[1], aircraftInfo.position[2]);
		fprintf(opreq_logfile_fd, "Velocity (x, y, z) = (%lf, %lf, %lf)\n", aircraftInfo.velocity[0], aircraftInfo.velocity[1], aircraftInfo.velocity[2]);

		fflush(opreq_logfile_fd);
		fclose(opreq_logfile_fd);
		opreq_logfile_fd = NULL;
}

static void _print_route_alarm_present(const uint32_t aircraftId1ForAlarm, const uint32_t aircraftId2ForAlarm, const uint32_t rel_dist_feet)
{
	int isFoundInAlarmMsgVectorAtIndex = -1;
	pthread_mutex_lock(&mutex);

	for(uint32_t i = 0; i < alarmMsgVector.size(); i++)
	{
		AlarmPlaneIds_t alarmPlaneIds = alarmMsgVector.at(i);

		if(alarmPlaneIds.ids[0] == aircraftId1ForAlarm)
		{
			if(alarmPlaneIds.ids[1] == aircraftId2ForAlarm)
			{
				isFoundInAlarmMsgVectorAtIndex = i;
			}
		}
	} // for(int i = 0; i < alarmMsgVector.size(); i++)

	if(isFoundInAlarmMsgVectorAtIndex == -1)
	{
		printf("DISPLAYSYS: [%04d ALARM-PRESENT] Constraint violation for ID1 = 0x%08X and ID2 = 0x%08X; RelDistFt = %u\r\n",
				g_msg_counter,
				aircraftId1ForAlarm,
				aircraftId2ForAlarm,
				rel_dist_feet);
		g_msg_counter++;

		AlarmPlaneIds_t alarmPlaneIds;
		alarmPlaneIds.ids[0] = aircraftId1ForAlarm;
		alarmPlaneIds.ids[1] = aircraftId2ForAlarm;
		alarmMsgVector.push_back(alarmPlaneIds);
	}

	pthread_cond_broadcast(&cond); // wake up all waiting threads
	pthread_mutex_unlock(&mutex);
}

static void _print_route_alarm_for_future_violation(const uint32_t aircraftId1ForAlarm, const uint32_t aircraftId2ForAlarm, const uint32_t rel_dist_feet)
{
	printf("DISPLAYSYS: [%04d ALARM-FUTURE] Potential constraint violation for ID1 = 0x%08X and ID2 = 0x%08X; RetDistFt = %u\r\n",
			g_msg_counter,
			aircraftId1ForAlarm,
			aircraftId2ForAlarm,
			rel_dist_feet);
	g_msg_counter++;
}

/*** Client Side of the code ***/
void* RunDisplayMsgSystem(void* arg)
{
   name_attach_t *attach;

   DisplayInfo_t displayInfo;
   int rcvid;

   /* Create a local name (/dev/name/local/...) */
   if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL)
   {
	   return (void*)EXIT_FAILURE;
   }

   /* Do your MsgReceive's here now with the chid */
   while (1)
   {

	   rcvid = MsgReceive(attach->chid, &displayInfo, sizeof(DisplayInfo_t), NULL);


	   if (rcvid == -1) {/* Error condition, exit */
		   break;
	   }

	   //if (msg.hdr.type == 0x00)
	   {
		  //if (msg.hdr.subtype == 0x01)
		  {
			  /* A message (presumable ours) received, handle */

			  printf("\nDISPLAYSYS: Received display message type: %d\n", (int)displayInfo.displayType);
			  //
			  // TO DO: Implement how to print for different display types
			  //
			  switch(displayInfo.displayType)
			  {
			  case DISPLAY_TYPE_AIRCRAFT_INFO:
				  _print_plane_info(displayInfo.aircraftInfoToReport);
				  break;

			  default:
				  break;
			  } // End of switch(msg.displayInfo.displayType)

		  }
	   }

	   MsgReply(rcvid, EOK, 0, 0);

   } // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

   /* Remove the name from the space */
   name_detach(attach, 0);

   return (void*)EXIT_SUCCESS;
}

void* RunAlarmMsgDisplayThread(void* arg)
{
   name_attach_t *attach;

   DisplayInfo_t displayInfo;
   int rcvid;

   alarmMsgVector.clear();

   /* Create a local name (/dev/name/local/...) */
   if ((attach = name_attach(NULL, ATTACH_POINT_DISPLAY_ALARM_MSG, 0)) == NULL)
   {
	   return (void*)EXIT_FAILURE;
   }

   /* Do your MsgReceive's here now with the chid */
   while (1)
   {
	   rcvid = MsgReceive(attach->chid, &displayInfo, sizeof(DisplayInfo_t), NULL);

	   /* In the above call, the received message will be stored at msg when the server receives a message.
		* Moreover, rcvid */

	   if (rcvid == -1) {/* Error condition, exit */
		   break;
	   }
	   //if (msg.hdr.type == 0x00)
	   {
		  //if (msg.hdr.subtype == 0x01)
		  {
			  // Implement how to print for different display types
			  //
			  switch(displayInfo.displayType)
			  {
			  case DISPLAY_TYPE_ROUTE_ALARM_PRESENT:
				  _print_route_alarm_present(displayInfo.aircraftIdsForAlarm[0], displayInfo.aircraftIdsForAlarm[1], displayInfo.rel_dist);
				  break;

			  case DISPLAY_TYPE_ROUTE_ALARM_FUTURE:
				  _print_route_alarm_for_future_violation(displayInfo.aircraftIdsForAlarm[0], displayInfo.aircraftIdsForAlarm[1], displayInfo.rel_dist);
				  break;

			  default:
				  break;
			  } // End of switch(msg.displayInfo.displayType)

		  }
	   }

	   MsgReply(rcvid, EOK, 0, 0);

   } // End of while (KEEP_RUNNING_COMPUTER_SERVER == 1)

   /* Remove the name from the space */
   name_detach(attach, 0);

   return (void*)EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
    int ret = 0;

    // install signal handler
	signal(SIGUSR1, resetAlarmMsgVector_handler);

	//
	// Have the TIMER fire SIGUSR1 on expiration
	//
	struct sigevent event;
	SIGEV_SIGNAL_INIT (&event, SIGUSR1);

	// create timer
	timer_t timer;
	struct itimerspec timer_spec;
	timer_create(CLOCK_REALTIME, &event, &timer);
	//timer_create(CLOCK_REALTIME, NULL, &timer);
	timer_spec.it_interval.tv_sec = 10; // 10-second period,it'll update
	timer_spec.it_interval.tv_nsec = 0;
	timer_spec.it_value = timer_spec.it_interval;
	timer_settime(timer, 0, &timer_spec, NULL);

    printf("Running RunDisplayMsgSystem ... \n");


	pthread_t thread_id_for_running_display_system;
	pthread_create(&thread_id_for_running_display_system, NULL, &RunDisplayMsgSystem, NULL);

	pthread_t thread_id_for_running_alarm_msg_display_system;
	pthread_create(&thread_id_for_running_alarm_msg_display_system, NULL, &RunAlarmMsgDisplayThread, NULL);

    //
	// Wait for the threads to finish
	//
	pthread_join(thread_id_for_running_display_system, NULL);
	pthread_join(thread_id_for_running_alarm_msg_display_system, NULL);

    return ret;
}
