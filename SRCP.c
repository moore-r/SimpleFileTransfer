// Ryan Moore, CS494
// File to implement functions from header file

#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#include "SRCP.h"

// Timer function, for timeouts and such
int delay(int num_secs)
{
	time_t start, end;
	double passed;
	int stop = 0;
	start = time(NULL);

	while(stop != -1)
	{
		end = time(NULL);
		passed = difftime(end, start);
		if(passed > num_secs){
			stop = -1;
			return -1;
		}
	}

	return 0;
}


// Clear the data in a packet so that we can loop 
// using the same packet.
void clear_frame(Frame *clear)
{
	clear->frame_type = '\0';
	clear->seq_num = '\0'; 
	clear->ack = '\0';
	clear->file_size = '\0';
	memset(clear->file_path, '\0', sizeof(clear->file_path));
	memset(clear->data, '\0', sizeof(clear->data));

	return;
}


// Print the contents of a frame
void print_frame(Frame * to_print)
{
	printf("Here are the contents of the frame: \n");
	printf("Type: %d\n", to_print->frame_type);
	printf("seq num: %d\n", to_print->seq_num);
	printf("ack: %d\n", to_print->ack);
	printf("file size: %d\n", to_print->file_size);
	printf("file path: %s\n", to_print->file_path);
	printf("data: %s\n", to_print->data);

	return;
}

// Testing a different kind of serialize function
char *ser(Frame *to_send, char dbuffer[])
{
	int off = 0;

	// Serialize the frame type and equence number
	memcpy(dbuffer, &to_send->frame_type, (sizeof(to_send->frame_type)));
	off += sizeof(to_send->frame_type);
	memcpy(dbuffer + off, &to_send->seq_num, (sizeof(to_send->seq_num)));
	off += sizeof(to_send->seq_num);
	memcpy(dbuffer + off, &to_send->ack, (sizeof(to_send->ack)));
	off += sizeof(to_send->ack);
	memcpy(dbuffer + off, &to_send->file_size, (sizeof(to_send->file_size)));
	off += sizeof(to_send->file_size);
	memcpy(dbuffer + off, &to_send->file_path, (sizeof(to_send->file_path)));
	off += sizeof(to_send->file_path);
	memcpy(dbuffer + off, &to_send->data, (sizeof(to_send->data)));
	off += sizeof(to_send->data);

/*
	printf("print in function: \n");
	for(int qre = 0; qre < off; qre += 1)
	{
		printf("%02x ", dbuffer[qre]);

	}
	printf("\n");
*/

	return dbuffer;
}


// Deserialize data
Frame *des(char *received, Frame *ret)
{

	memcpy(&ret->frame_type, (received),																(sizeof(unsigned int)));
	memcpy(&ret->seq_num,    (received + sizeof(ret->frame_type)),													(sizeof(unsigned int)));
	memcpy(&ret->ack,        (received + sizeof(ret->seq_num) + sizeof(ret->frame_type)),  										(sizeof(unsigned int)));
	memcpy(&ret->file_size,  (received + sizeof(ret->ack) + sizeof(ret->seq_num) + sizeof(ret->frame_type)), 							(sizeof(unsigned int)));
	memcpy(&ret->file_path,  (received + sizeof(ret->ack) + sizeof(ret->seq_num) + sizeof(ret->frame_type) + sizeof(ret->file_size)), 				(sizeof(ret->file_path)));
	memcpy(&ret->data,       (received + sizeof(ret->ack) + sizeof(ret->seq_num) + sizeof(ret->frame_type) + sizeof(ret->file_size) + sizeof(ret->file_path)), 	(sizeof(ret->data)));

	return ret;
}

Frame *create_close_packet()
{
	Frame *exit = malloc(sizeof(Frame));
	exit->frame_type = 6;

	return exit;
}

