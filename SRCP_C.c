//Ryan Moore, CS 494, Programming Project 1
//Client Side

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

// Declare some functions
void quit(int, int);
void *sender(void *);
unsigned int IP_converter(char *);
void *client_receiver(void *);


int main(int argc, char *argv[])
{
	// Variables
	int Port, Ack_Port, sock, acksock, Time_Out, max_window;

	char *IP_Address, *Path; 
	struct sockaddr_in server_address;
	struct sockaddr_in ack_server_address;
	socklen_t server_len;
	socklen_t ack_server_len;

	char rec_buffer[MinBuffer], rec2_buffer[MinBuffer];

	unsigned int packet_number = 0, size;


	// Check and make sure we have the correct ammount of args
	if(argc < 6){
		printf("Error: command line args\n");
		exit(0);
	}


	// Save the arguments in variables.
	IP_Address = malloc(strlen(argv[1] + 1));
	strcpy(IP_Address, argv[1]);
	Port = atoi(argv[2]);
	Ack_Port = Port + 1;
	Path = malloc(strlen(argv[3] + 1));
	strcpy(Path, argv[3]);
	Time_Out = atoi(argv[4]);
	max_window = atoi(argv[5]);

	printf("The IP is: %s, port: %d, Second port: %d, Path: %s, timeout: %d, Max window size: %d.\n", IP_Address, Port, Ack_Port, Path, Time_Out, max_window);

	// Create a socket to receive data
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("Error when creating socket.\n");
		exit(0);
	}
	// Create a socket to send acks
	if((acksock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("Error when creating ack socket.\n");
		exit(0);
	}

	// Identify the sockets
	memset((char *)&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = IP_converter(IP_Address);
	server_address.sin_port = htons(Port);
	server_len = sizeof(server_address);

	// Identify the ack socket
	memset((char *)&ack_server_address, 0, sizeof(ack_server_address));
	ack_server_address.sin_family = AF_INET;
	ack_server_address.sin_addr.s_addr = IP_converter(IP_Address);
	ack_server_address.sin_port = htons(Ack_Port);
	ack_server_len = sizeof(ack_server_address);


	// Now attempt to connect to the server with the data socket
	if(connect(sock, (struct sockaddr *)&server_address, server_len) < 0){
		printf("Error connecting to receiveing socket to server.\n");
		exit(0);
	}

	// Now attempt connection with the ack socket
	if(connect(acksock, (struct sockaddr *)&ack_server_address, ack_server_len) < 0){
		printf("Error binding to ack socket server.\n");
		exit(0);
	}

	// Send packet to request connection form both sockets
	Frame *hello = malloc(sizeof(Frame));
	hello->frame_type = 1;
	hello->seq_num = packet_number;
	
	// Fill buffer with the packet - Serialize the data
	char connect[PacketSize];
	char *connect_buf = malloc(PacketSize);
	connect_buf = ser(hello, connect);
	strcpy(connect, connect_buf);
	sendto(acksock, connect, sizeof(connect), 0, (struct sockaddr *)&ack_server_address, ack_server_len);
	sendto(sock, connect, sizeof(connect), 0, (struct sockaddr *)&server_address, server_len);

	// Test the receiving sid eof the client
	recvfrom(sock, rec_buffer, MinBuffer, 0,(struct sockaddr *)&server_address, &server_len);	
	recvfrom(acksock, rec2_buffer, MinBuffer, 0,(struct sockaddr *)&ack_server_address, &ack_server_len);

	// Deserialize the packet
	Frame *rec_ack0 = malloc(sizeof(Frame));
	rec_ack0 = des(rec_buffer, rec_ack0);

	Frame *rec_ack1 = malloc(sizeof(Frame));
	rec_ack1 = des(rec2_buffer, rec_ack1);

	// If the frames are acks the connection is good to go. 
	if(rec_ack0->frame_type != 2 || rec_ack1->frame_type != 2)
	{
		printf("Incorrect frame type received. Not an ack.\n");
		exit(0);
	}

	// Send the file request packet
	Frame *file_request = malloc(sizeof(Frame));
	file_request->frame_type = 3;
	file_request->seq_num = packet_number;
	strcpy(file_request->file_path, Path);
	
	// Fill buffer with the packet - Serialize the data
	char connect_path[PacketSize];
	char *connect_buf_path = malloc(PacketSize);
	connect_buf_path = ser(file_request, connect_path);
	strcpy(connect_path, connect_buf_path);

	sendto(acksock, connect_path, sizeof(connect_path), 0, (struct sockaddr *)&ack_server_address, ack_server_len);
	
	// Recieve the file size
	memset(rec_buffer, '\0', sizeof(rec_buffer));
	recvfrom(sock, rec_buffer, MinBuffer, 0,(struct sockaddr *)&server_address, &server_len);

	// Deserialize the request packets
	Frame *rec_size = malloc(sizeof(Frame));
	rec_size = des(rec_buffer, rec_size);

	// Check if we received a file close packet
	if(rec_size->frame_type == 6)
	{
		// If the file isnt found, shutdown client sockets and close 
		printf("Could not locate file, closing connections.\n");
		shutdown(sock, SHUT_RDWR);
		shutdown(acksock, SHUT_RDWR);

		exit(0);
	}

	// Store the file size in a short variable
	size = rec_size->file_size;

	// Send an ack for the file size, so the server can
	// begin transmitting
	Frame *ready = malloc(sizeof(Frame));	
	ready->frame_type = 2;
	ready->seq_num = packet_number;
	// Sneaking the commandline parameters needed in server
	// into non important info in this packet.
	ready->ack = Time_Out;
	ready->file_size = max_window; 

	// Fill buffer with the packet - Serialize the data
	char ready_ack[PacketSize];
	char *readyack = malloc(PacketSize);
	readyack = ser(ready, ready_ack);
	strcpy(ready_ack, readyack);

	sendto(acksock, ready_ack, sizeof(ready_ack), 0, (struct sockaddr *)&ack_server_address, ack_server_len);


/*
	Now the handshake and file reuest packets
	are out of the way, so we will now begin to 
	receive the actual file 
*/
	// Create the client side window LLL
	struct CNode *chead = c_init_LLL(max_window);
	Window *cwin = c_init_window(max_window, chead);
	
	// Variables for the loop
	Frame *c_ack_loop = malloc(sizeof(Frame));
	char c_ack[PacketSize];
	char *c_ack_ptr = malloc(PacketSize);
	unsigned int ret;
	int count;




	// Variables for the loop
	Frame *client_loop = malloc(sizeof(Frame));
	char Cbuffer[PacketSize];

	// Buffer that has the actual file
	char * TheFile;
	TheFile = malloc(size);

	// Begin the loop to receive the file
	for(;;)
	{
		// This is a dumb counter I made, it save a value
		// from a function that tells us if we should deliver
		count = 2;

		// Receive the ACK
		recvfrom(sock, Cbuffer, PacketSize, 0,(struct sockaddr *)&server_address, &server_len);

		// Deserialize the information
		client_loop = des(Cbuffer, client_loop);

		// Buffer the data
		count = data_buffer(client_loop, cwin); 

		// Find which acks to send
		ret = finder(cwin);

		if(ret != -1)
		{
			// Create the Frame
			c_ack_loop->frame_type = 2;
			c_ack_loop->seq_num = ret;
			c_ack_loop->ack = ret;

			// Serialize the Frame
			c_ack_ptr = ser(c_ack_loop, c_ack);
			strcpy(c_ack, c_ack_ptr);
	
			// Send the ACK
			sendto(acksock, c_ack, sizeof(c_ack), 0, (struct sockaddr *)&ack_server_address, ack_server_len);

			// Clear the memory of the frames and buffers
			memset(c_ack, '\0', PacketSize);
			memset(c_ack_ptr, '\0', PacketSize);
			clear_frame(c_ack_loop);
		}


		// If count is 1, then we need to deliver the frame
		if(count == 1)
		{
			TheFile = deliver(TheFile, cwin->cbase, cwin);			
		}
		
		// CHECK FOR CLOSE PACKET
		if(client_loop->frame_type == 6)
		{
			printf("Close packet received from server.\n");
			printf("Contents of the file: \n%s", TheFile);

			quit(sock, acksock);
		}

		// Clear the memory of the buffer and frame
		memset(Cbuffer, '\0', sizeof(Cbuffer));
		clear_frame(client_loop);
	}

	return 0;
}


// Function to convert a string to an IP address
unsigned int IP_converter(char *string_IP)
{
	// Variables to save the IP address
	int a, b, c, d;
	char array_IP[4];

	// Convert the string to ints
	sscanf(string_IP, "%d.%d.%d.%d", &a, &b, &c, &d);

	// Save the ints in the array
	array_IP[0] = a;
	array_IP[1] = b;
	array_IP[2] = c;
	array_IP[3] = d;

	//return the array in the proper format
	return *(unsigned int *)array_IP;
}


// A function that simply closes the sockets and exits
void quit(int socket, int ack_socket)
{
	printf("Closing sockets and shutting down.\n");
	shutdown(socket, SHUT_RDWR);
	shutdown(ack_socket, SHUT_RDWR);

	exit(0);		
}

