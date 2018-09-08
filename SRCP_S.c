//Ryan Moore, CS 494, Programming Project 1
//Server Side

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

pthread_t thread1;
pthread_t thread2;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;


// Declaring functions
unsigned int file_size(char *);
void create_timer(void *);
int find_array_size(int, int);
void create_frame();
void shut_down(void *);
void *receiver(void *);
void *server_send(void *);


int main(int argc, char *argv[])
{
	// Variables
	time_t begin_time_stamp, end_time_stamp; 
	begin_time_stamp = time(NULL);

	int portn, ack_portn, socks, acksocks,/* timeout,*/ window_size;
	struct sockaddr_in client_address, ack_client_address;
	socklen_t client_length, ack_client_length;
	char Sbuffer[MinBuffer], bbuffer[MinBuffer]; 
	unsigned int packet_num = 0, size_of_file;
	FILE * fp;

	// Check the passed in args
	if(argc < 2){
		printf("Error with provided port number.\n");
		exit(0);
	}

	// Transfer the port number to a local variable
	portn = atoi(argv[1]);
	ack_portn = portn + 1;

	// Create the socket
	// Sends data to the client
	if((socks = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("Error in server sending socket creation.\n");
		exit(0);
	}
	// Recieves things from the client, mainly acks
	if((acksocks = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("Error in server ack socket creation.\n");
		exit(0);
	}

	// Get the size of the sockets
	client_length = sizeof(client_address);
	ack_client_length = sizeof(ack_client_address);

	// Identify the sockets
	memset((char *)&client_address, 0, client_length);
	client_address.sin_family = AF_INET;
	client_address.sin_addr.s_addr = htonl(INADDR_ANY);
	client_address.sin_port = htons(portn);

	memset((char *)&ack_client_address, 0, ack_client_length);
	ack_client_address.sin_family = AF_INET;
	ack_client_address.sin_addr.s_addr = htonl(INADDR_ANY);
	ack_client_address.sin_port = htons(ack_portn);

	// Binding the server sockets
	if(bind(socks, (struct sockaddr *)&client_address, client_length) < 0){
		printf("Error binding server sending socket.\n");
		exit(0);
	}	
	listen(socks, 5);
	accept(socks, (struct sockaddr *)&client_address, &client_length);

	if(bind(acksocks, (struct sockaddr *)&ack_client_address, ack_client_length) < 0){
		printf("Error binding server socket.\n");
		exit(0);
	}
	listen(acksocks, 5);
	accept(acksocks, (struct sockaddr *)&ack_client_address, &ack_client_length);

	// Receive the handshake connection packet from the client
	recvfrom(acksocks, Sbuffer, MinBuffer, 0,(struct sockaddr *)&ack_client_address, &ack_client_length);
	recvfrom(socks, bbuffer, MinBuffer, 0,(struct sockaddr *)&client_address, &client_length);

	// Deserialize the packets
	Frame *hand = malloc(sizeof(Frame));
	hand = des(Sbuffer, hand);

	Frame *shake = malloc(sizeof(Frame));
	shake = des(bbuffer, shake);

	// Reply to the connection packets with ack packets
	// Use the sequence number as the ack number
	Frame *ack_hand = malloc(sizeof(Frame));
	ack_hand->frame_type = 2;
	ack_hand->seq_num = packet_num;
	ack_hand->ack = 1;

	Frame *ack_shake = malloc(sizeof(Frame));
	ack_shake->frame_type = 2;
	ack_shake->seq_num = packet_num;
	ack_shake->ack = 1;

	// Fill buffer with the packet - Serialize the data
	char connect1[PacketSize];
	char *connect_buf1 = malloc(PacketSize);
	connect_buf1 = ser(ack_hand, connect1);
	strcpy(connect1, connect_buf1);

	char connect2[PacketSize];
	char *connect_buf2 = malloc(PacketSize);
	connect_buf2 = ser(ack_shake, connect2);
	strcpy(connect2, connect_buf2);

	sendto(socks, connect2, sizeof(connect2), 0, (struct sockaddr *)&client_address, client_length);
	sendto(acksocks, connect1, sizeof(connect1), 0, (struct sockaddr *)&ack_client_address, ack_client_length);

	// Receive the file request packet
	memset(Sbuffer, '\0', sizeof(Sbuffer));
	recvfrom(acksocks, Sbuffer, MinBuffer, 0,(struct sockaddr *)&ack_client_address, &ack_client_length);

	// Deserialize the request packets
	Frame *rec_path = malloc(sizeof(Frame));
	rec_path = des(Sbuffer, rec_path);

	// Store the path in a local variable with a shorter name
	char path[strlen(rec_path->file_path) + 1];
	strcpy(path, rec_path->file_path);

	// Get the file size
	size_of_file = file_size(path);

	// Open the file
	if(NULL == (fp = fopen(path, "r")))
	{
		// If the file isnt found, shutdown server sockets, send shutdown packet to client  
		printf("Could not locate file.\n");

		Frame *close = create_close_packet();
		char close_s[PacketSize];
		char *closer_s = malloc(PacketSize);
		closer_s = ser(close, close_s);
		strcpy(close_s, closer_s);
		
		sendto(socks, close_s, sizeof(close_s), 0, (struct sockaddr *)&client_address, client_length);

		shutdown(socks, SHUT_RDWR);
		shutdown(acksocks, SHUT_RDWR);

		exit(0);
	}
	else
	{
		printf("File found and opened.\n");
	}

	// Send the file size to the client
	Frame *send_size = malloc(sizeof(Frame));
	send_size->frame_type = 4;
	send_size->seq_num = packet_num;
	send_size->file_size = size_of_file;

	// Fill buffer with the packet - Serialize the data
	char connect_size[PacketSize];
	char *connect_buf_size = malloc(PacketSize);
	connect_buf_size = ser(send_size, connect_size);
	strcpy(connect_size, connect_buf_size);

	sendto(socks, connect_size, sizeof(connect_size), 0, (struct sockaddr *)&client_address, client_length);

	// Receive the ack, last packet before we begin 
	// the file transmission
	memset(Sbuffer, '\0', sizeof(Sbuffer));
	recvfrom(acksocks, Sbuffer, MinBuffer, 0,(struct sockaddr *)&ack_client_address, &ack_client_length);
	Frame *ready_s = malloc(sizeof(Frame));
	ready_s = des(Sbuffer, ready_s);

	if(ready_s->frame_type == 2)
	{
		printf("Ack for file size received, beginning to transmit file.\n");
	}

	//Get the command line args for timeout and window size from the last ack packet
//	timeout = ready_s->ack;
	window_size = ready_s->file_size;

/*
	 Now the handshake and other stuff is finished, time
	 to start actually sending the file 
*/	
	// Create the window and LLL	
	struct Node *head = init_LLL();
	Window *window = init_window(window_size, head);

	// Store the arguments need for the reciever in a struct
	struct arg_struct args;
	args.arg1 = acksocks; 
	args.arg2 = socks;
	args.arg3 = ack_client_address; 
	args.arg4 = ack_client_length;
	args.arg5 = window;
	args.arg8 = size_of_file;
	args.arg9 = client_address;
	args.arg10 = client_length;

	// Store the arguments need for sending in a struct
	struct arg_struct send_args;
	send_args.arg2 = socks;
	send_args.arg3 = client_address; 
	send_args.arg4 = client_length;
	send_args.arg5 = window;
	send_args.arg7 = fp;
	send_args.arg8 = size_of_file;

	// Create the thread and mutex lock
	if(pthread_create(&thread1, NULL, &receiver, (void *)&args) != 0)
	{
		printf("Server receiver thread creation failed, aborting.\n");
		exit(0);
	}

	if(pthread_create(&thread2, NULL, &server_send, (void *)&send_args) != 0)
	{
		printf("Server receiver thread creation failed, aborting.\n");
		exit(0);
	}

	pthread_join(thread2, NULL);

	// Get the total time the file transfer took
	end_time_stamp = time(NULL);
	end_time_stamp -= begin_time_stamp;
	printf("Elapsed time to send the entire file: %ld\n", end_time_stamp);

	shut_down(&args);
	return 0;
}



// Function to check if a file exsists and get its size
unsigned int file_size(char * name)
{
	// Variables
	struct stat file;
	unsigned int size;

	// Get its size, if we get a -1, file doesnt exsist
	if(stat(name, &file) == -1){
		printf("Error finding file.\n");
		return -1;
	}
	
	// Convert the size to an unsigned int
	size = file.st_size;

	// Return the size
	return size;
}


// A function to create the timer
void create_timer(void *timer_args)
{
	// Variables
	timer_t timer_id;
	int status;
	struct itimerspec its;
	struct sigevent sev;

	struct arg_struct *args = timer_args;
	int millis = args->arg6;

	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = &args;
	sev.sigev_notify_function = check_timeout;  
	sev.sigev_notify_attributes = NULL;

	its.it_value.tv_sec = millis / 1000000;
	its.it_value.tv_nsec = millis % 1000000;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	status = timer_create(CLOCK_REALTIME, &sev, &timer_id);
	if(status == -1)
	{
		printf("Failed to create timer, aborting.\n");
		exit(0);
	}

	status = timer_settime(timer_id, 0, &its, 0);
	if(status == -1)
	{
		printf("Failed to create timer. Aborting.\n");
		exit(0);
	}

	return;
}

// Find out how many total packets will be sent
int find_array_size(int buffer_size, int size_file)
{
	int num_index;
	num_index = (size_file / buffer_size) + 1;
	return num_index;
}

// A function that simply closes the sockets and exits
void shut_down(void *closer)
{
	// Get the arguments and save them in local variables
	struct arg_struct *close = closer;
	int csocket = close->arg1;
	int closesocket = close->arg2;
	struct sockaddr_in cc = close->arg9;
	socklen_t clen = close->arg10;

	Frame * toclose =  malloc(sizeof(Frame));
	toclose->frame_type = 6;

	char cdata[PacketSize];
	char *cdata_ptr = malloc(PacketSize);
	cdata_ptr = ser(toclose, cdata);
	strcpy(cdata, cdata_ptr);

	sendto(closesocket, cdata, sizeof(cdata), 0, (struct sockaddr *)&cc, clen);

	printf("Closing sockets and shutting down.\n");
	shutdown(csocket, SHUT_RDWR);
	shutdown(closesocket, SHUT_RDWR);

	exit(0);		
}


// This function handles the sending of frames and is 
// controlled by a thread
void *server_send(void * s_arguments)
{
	// Get the arguments and save them in local variables
	struct arg_struct * s_args = s_arguments;
	int s_sock = s_args->arg2;
	struct sockaddr_in s_send = s_args->arg3;
	socklen_t s_len = s_args->arg4;
	Window *swin = s_args->arg5;
	FILE *s_read = s_args->arg7;
	int size_f = s_args->arg8;

	// Get how many frames will be sent
	int num_frames = find_array_size(MinBuffer, size_f);


	// Create variables for the loop
	Frame *data_loop = malloc(sizeof(Frame));
	char data[PacketSize];
	char *data_ptr = malloc(PacketSize);
	struct Node *holder = (struct Node *)malloc(sizeof(struct Node));

	// Now begin the loop that sends the file!
	for(;;)
	{
		// Lock up
		pthread_mutex_lock(&lock);

		// Get the first sendable node before we loop
		holder = to_send(swin->base);
	
		while(holder != NULL)
		{
			// Fill the frame with data
			data_loop = fill(s_read, data_loop, holder);

			// Serialize the frame
			data_ptr = ser(data_loop, data);
			strcpy(data, data_ptr);

			// Send it!
			sendto(s_sock, data, sizeof(data), 0, (struct sockaddr *)&s_send, s_len);

			if(data_loop->seq_num == num_frames)
			{
				return NULL;
			}

			// Get the next sendable node, restart the loop with it
			holder = to_send(swin->base);
	
			// Clear the memory of the frames and buffers
			memset(data, '\0', PacketSize);
			memset(data_ptr, '\0', PacketSize);
			clear_frame(data_loop);

		}
	
		// Signal the ack receiver
		pthread_cond_signal(&cond);

		// Unlock the mutex
		pthread_mutex_unlock(&lock);
	}

	return NULL;
}


// This function will handle the receiving of acks, state
// changes, and increasing the window when acks are received
void *receiver(void *arguments)
{
	// Get the arguments and save them in local variables
	struct arg_struct * args = arguments;
	int socket = args->arg1;

	struct sockaddr_in ack = args->arg3;
	socklen_t len = args->arg4;
	Window *win = args->arg5;
	int size_f = args->arg8;

	// Get how many frames will be sent
	int num_frames = find_array_size(MinBuffer, size_f);

	// Frame to store data in during the loop 
	Frame *ack_loop = malloc(sizeof(Frame));

	// Buffer for the acks
	char buffer[PacketSize];
	unsigned int sequence;
	int counter = 1;

	// This loop loops until told to exit or all acks are received
	for(;;)
	{
		// Lock up
		pthread_mutex_lock(&lock);

		// Wait for the signal
		pthread_cond_wait(&cond, &lock);

		// Receive the ACK
		recvfrom(socket, buffer, PacketSize, 0,(struct sockaddr *)&ack, &len);

		// Deserialize the information
		ack_loop = des(buffer, ack_loop);

		// CHECK FOR CLOSE PACKET
		if(ack_loop->frame_type == 6)
		{
			printf("Close packet received from client.\n");
			exit(0);
		}

		if(win->base->seq == num_frames && win->base->next == NULL)
		{
			printf("Close packet received from client.\n");
			shut_down(arguments);
		}

		sequence = win->base->seq;

		// Change the state of the node in the window
		counter = state_change(win, ack_loop->seq_num);

		if(counter == 1)
		{
			increase_window(win, (sequence +1));		
		}
		if(counter == 0)
		{
			increase_window(win, sequence);
		}
	
		// Clear the memory of the buffer and frame
		memset(buffer, '\0', sizeof(buffer));
		clear_frame(ack_loop);
	
		// Unlock mutex
		pthread_mutex_unlock(&lock);
	}
	
	shut_down(arguments);
	return NULL;
}



