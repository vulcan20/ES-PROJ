#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>

#include <errno.h>
#include <sys/dispatch.h>

#include <string.h>

#include <mqueue.h>

#include "Aircraft/Aircraft.h"
#include "RadarMsg/RadarMsg.h"

#define ATTACH_POINT "radar"

#define MQNAME "/my_mq"
#define MSGSIZE 64  // Make sure to size it according to the sizeof(Aircraft_t)

#define NUM_THREADS 20


//make classes
//if aircraft doesn't enter the range what happens?

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int event_flag = 0;

uint32_t signal_handler_invokation_counter = 0;

/*******
typedef struct Aircraft_s
{
	uint64_t id;
	double position[3];
	double velocity[3];

	uint32_t in_ATC_tracking_range; // 1 = in range; 0 = outside range

	uint32_t reserved;

} Aircraft_t;  // Current size is 64 bytes
*****/

static Aircraft_t av[NUM_THREADS];

//
// Message Queue
//
static mqd_t mq;

//
// Radar message attachement to the computer system
//

//static name_attach_t *radar_msg_attachment_to_computer_system;

int server_conn_id; //server connection ID. // server ID for handle server


void init_Aircrafts()
{
	int i;
	// All distances are in feet
	for(i=0;i<NUM_THREADS;i++)
	{
	av[i].id = 0x80081+i;
	av[i].position[0] = (double)(rand() % 100000); // x
	av[i].position[1] = (double)(rand() % 100000); // y
	av[i].position[2] = 15000.0 + (double)(rand() % 25000); // z
	av[i].velocity[0] = (double)(rand() % 100); // x
	av[i].velocity[1] = (double)(rand() % 1); // y
	av[i].velocity[2] = (double)(rand() % 1); // z
	av[i].in_ATC_tracking_range = 1;
	}

}


// signal handler function
void signal_handler(int signum)
{
	signal_handler_invokation_counter++;
	printf("\n[INFO][%s, %d]: In signal_handler(): signum = %d; tickCount = %u\n",
    		__FILE__,
			__LINE__,
			signum,
    		signal_handler_invokation_counter);
	pthread_mutex_lock(&mutex);
    //event_flag = 2;
	event_flag = NUM_THREADS;
    pthread_cond_broadcast(&cond); // wake up all waiting threads
    pthread_mutex_unlock(&mutex);
}

// thread function
void* airCraft_state_update(void* arg)
{
    //int id = *(int*)arg;
	Aircraft_t* av = (Aircraft_t*) arg;
    int counter = 0;
    printf("[INFO][%s, %d]: TP1, Plane ID = 0x%08X\n", __FILE__, __LINE__, (uint32_t)av->id);

    /*******
    //
    // Init message pipe
    //
    my_data_t msg;
	int server_coid; //server connection ID.

	if ((server_coid = name_open(ATTACH_POINT, 0)) == -1)
	{
		return (void*)EXIT_FAILURE;
	}

	// We would have pre-defined data to stuff here
	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x01;
	**********/

    while (av->in_ATC_tracking_range == 1)
    {
        pthread_mutex_lock(&mutex);
        //printf("[INFO][%s, %d]: thread_id = %d acquired the mutex lock!\n", __FILE__, __LINE__,id);
        while (event_flag == 0)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        counter++;

        // Update position given that position updates at 5Hz ( s = s0 + vt)
		av->position[0] = av->position[0] + (av->velocity[0] * 5);
		av->position[1] = av->position[1] + (av->velocity[1] * 5);
		av->position[2] = av->position[2] + (av->velocity[2] * 5);

		//
		// Check if the AV is inside ATC's tracking range
		//
		if( (av->position[0] < 0.0f) || (av->position[0] > 100000.0) )
		{
			av->in_ATC_tracking_range = 0;
			printf("Plane ID = 0x%08X\n out of range", (uint32_t)av->id );
		}

		if( (av->position[1] < 0.0f) || (av->position[1] > 100000.0) )
		{
			av->in_ATC_tracking_range = 0;
			printf("Plane ID = 0x%08X\n out of range", (uint32_t) av->id );
		}

		if( (av->position[2] < 15000.0) || (av->position[2] > 40000.0) )
		{
			av->in_ATC_tracking_range = 0;
			printf("Plane ID = 0x%08X\n out of range", (uint32_t)av->id );
		}

		/******
		//
		// Send message to radar
		//
		if (MsgSend(server_coid, &msg, sizeof(msg), NULL, 0) == -1) // if using message queue: mq_send();
		{
			break;
		}

		// mq_send(queue_instance, (void*)av, sizeof()

		*******/
		/****/
		// to send a message to a message queue

		uint8_t currentStateBytes[sizeof(Aircraft_t)];
		//creating an array of bytes called currentStateBytes with the same size as a struct typedef- Aircraft_t
		memcpy(&currentStateBytes, av, sizeof(Aircraft_t));
		//mq_send(queue_instance, (void*)&currentState, sizeof(Aircraft_t));

		int retCode = mq_send(mq, (char*)currentStateBytes, sizeof(Aircraft_t), 0);
		if(retCode == -1)
		{
			printf("mq_send() failed! RetCode = %d\n", retCode);
		}
		/****/

        printf("[INFO][%s, %d]: Thread with plane id 0x%08X got signal event, counter = %d; tickCount = %u\n",
        		__FILE__,
				__LINE__,
				(uint32_t)av->id,
				counter,
				signal_handler_invokation_counter);

        //event_flag = 0; // reset event flag
        event_flag--;

        pthread_mutex_unlock(&mutex);

    } // End of while (av->in_ATC_tracking_range == 1)


    /* Close the connection */
	//name_close(server_coid);
	return (void*)EXIT_SUCCESS;
}

static void _print_plane_info(const Aircraft_t planeInfo)
{
	printf("\nAV ID = 0x%08X waiting..\n", (uint32_t)planeInfo.id);
	printf("Position (x, y, z) = (%lf, %lf, %lf)\n", planeInfo.position[0], planeInfo.position[1], planeInfo.position[2]);
	printf("Velocity (x, y, z) = (%lf, %lf, %lf)\n", planeInfo.velocity[0], planeInfo.velocity[1], planeInfo.velocity[2]);
}

static int _send_to_server(const Aircraft_t planeInfo)
{
	RadarMsg_t radarMsg;
	radarMsg.hdr.type = 0x00;
	radarMsg.hdr.subtype = 0x01;

	memcpy(&radarMsg.aircraftInfo, &planeInfo, sizeof(Aircraft_t));
	if (MsgSend(server_conn_id, &radarMsg, sizeof(RadarMsg_t), NULL, 0) == -1)
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void* thread_radar_callback_func(void* arg)
{
	int thread_id = *(int*)arg;

	int retCode = 0;

	printf("STARTING RADAR THREAD WITH ID = %d\n", thread_id);

	Aircraft_t planeInfo;
	uint8_t rcvdBytes[sizeof(Aircraft_t)];

	   /* Do your MsgReceive's here now with the chid */
	   while (1)
	   {
		   retCode = mq_receive(mq, (char*)rcvdBytes, MSGSIZE, NULL);
		   if(retCode == -1)
		   {
			   printf("mq_receive() failed!\n");
		   }

		   memcpy(&planeInfo, rcvdBytes, sizeof(Aircraft_t));

		   _print_plane_info(planeInfo);

		   //
		   // Send the same info to the server channel (= computer system)
		   //
		   retCode = _send_to_server(planeInfo);
		   if(retCode != EXIT_SUCCESS)
		   {
			   printf("_send_to_server() failed!\n");
		   }

	   } // End of while(1)

	   //
	   // Close the message channel
	   //
	   name_close(server_conn_id); // another client can connect to that server
}


int init_message_queue(void) // returns on on success
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

int init_computer_system_msg_channel(void)
{
	/************
	server_conn_id = name_open(ATTACH_POINT, 0); // NAME_FLAG_ATTACH_GLOBAL

	if (server_conn_id != 0)
	{
		printf("name_open failed! RetCode = %d; errno = %d\n",
				server_conn_id,
				errno);
		return EXIT_FAILURE;
	}
	**************/

	if ((server_conn_id = name_open(ATTACH_POINT, 0)) == -1) {
	        return EXIT_FAILURE;
	    }

	return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    int retCode = 0;

    retCode = init_computer_system_msg_channel();
	if(retCode != 0)
	{
		printf("init_computer_system_msg_channel() failed!\n");
		return -1;
	}

	init_Aircrafts();

    init_message_queue(); // For accessing data from threads maintaining aircraft states


    // install signal handler
    signal(SIGUSR1, signal_handler);

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
    timer_spec.it_interval.tv_sec = 1; // 1-second period
    timer_spec.it_interval.tv_nsec = 0;
    timer_spec.it_value = timer_spec.it_interval;
    timer_settime(timer, 0, &timer_spec, NULL);

    // create threads
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, &airCraft_state_update, &av[i]);
    }

    //
    // Spawn the radar thread
    //
    int radarThreadId = thread_ids[NUM_THREADS-1] + 1;
    pthread_t radarThread;
    pthread_create(&radarThread, NULL, thread_radar_callback_func, &radarThreadId);

    // wait for threads to finish (should never happen)
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Wait for radar thread
    pthread_join(radarThreadId, NULL);

    // cleanup
    timer_delete(timer);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}
