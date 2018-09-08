//Ryan Moore, CS 494, Programming Project Part 2
//Some functions to help manipulate the window

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
//#include "window.h"


// Initialize the LLL for the server
struct Node *init_LLL()
{
	// Allocate mem and fill the first node
	struct Node *current = (struct Node*)malloc(sizeof(struct Node));

	current->begin_read = 0;
	current->seq = 1;
	current->state = 2; 
	current->time_stamp = 0; 
	current->next = NULL;

	return current;
}

// Initialize the window with the first node
Window *init_window(int w_max, struct Node *head)
{
	Window *init = (Window *)malloc(sizeof(Window));

	init->max = w_max;
	init->capacity = 1;
	init->total = 1;
	init->base = head;	
	init->cbase = NULL;
 
	return init;
}


// Run through the window and see if any of the packets have timed
// out by using their timestamp, runs until a -1 is returned.
void check_timeout(union sigval arg)
{
	// Get the arguments and save them in local variables
	struct arg_struct * args = arg.sival_ptr;
	int socket = args->arg2;
	struct sockaddr_in to_send = args->arg3;
	socklen_t len = args->arg4;
	Window *win = args->arg5;
	int resend = args->arg6;
	FILE *to_read = args->arg7;

	// Create a few more variables
	struct Node *current = win->base;
	time_t out;
	Frame *to_resend = malloc(sizeof(Frame));
	char data[PacketSize];
	char *data_ptr = malloc(PacketSize);

	// Loop and find all timed out nodes
	while(current != NULL)
	{
		if(current->state == 1)
		{
			// Get the time
			out = time(NULL);
			if((out - current->time_stamp) >= resend)
			{
				// Frame has timed out, resend it.
				// Get a new time stamp on it
				current->time_stamp = time(NULL);
				
				// Refill the Frame
				to_resend = fill(to_read, to_resend, current);

				// Serialize the frame
				data_ptr = ser(to_resend, data);
				strcpy(data, data_ptr);

				// Send it!
				sendto(socket, data, sizeof(data), 0, (struct sockaddr *)&to_send, len);
			}
		}
		current = current->next;
	}

	// Nothing left, stop transversing
	return;
}


// When the window is not yet full size, add a slot to it
// after the firt spot of the window is acked
void increase_window(Window *win, unsigned int the_seq)
{
	int count = 1;
	struct Node *current = win->base; 
	
	struct Node *prev = current;

	// Check if the totalnodes is already equal to the capacityof the window
	if(win->capacity == win->total)
	{
		// If they are equal we can just return
		return;
	}

	// transverse to the last node
	while(current != NULL)
	{

		prev = current;
		current = current->next;
		count += 1;
	}
	current = prev;

	// Now add nodes until our counter equals the current capacity of the window
	while(count < win->capacity)
	{

		// Allocate a new node
		struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));

		if(win->base == NULL)
		{

			current = new_node;
			new_node->begin_read = (MinBuffer * (the_seq * count));
			new_node->seq = the_seq;
			new_node->state = 2;
			new_node->time_stamp = 0; 
			new_node->next = NULL;

			count += 1;
			win->total += 1;
			win->base = current;
		}
		else
		{

			current->next = new_node;

			// Fill the new node with data
			new_node->begin_read = (current->seq * MinBuffer);
			new_node->seq = (current->seq + 1); 
			new_node->state = 2;
			new_node->time_stamp = 0; 
			new_node->next = NULL;

			count += 1;
			win->total += 1;
			current = current->next;
		}
	}

	return;
}


// Multiplicative decrease for the window, cut it in half during a loss
void decrease_window(Window *head)
{
	int count = 1;
	struct Node *current = head->base;

	head->capacity = (head->capacity / 2);
	head->capacity += 1; // Ceiling it

	// Cut off Nodes out of the new range
	while(count < head->capacity)
	{
		current = current->next;
		count += 1;
	}

	// Current is now at the place where the list should end
	head->total = count;
	struct Node * to_delete = current->next;
	struct Node * prev;
	
	current->next = NULL;

	while(to_delete != NULL)
	{
		prev = to_delete;
		to_delete = to_delete->next;

		// Free the memory
		free(prev);
	} 

	return;
}

// When the first slot in the window is acked every other
// slot in the window needs to be moved over 1, and the
// last slots which are now empty will need to be filled
void shift_window(Window *win)
{
	struct Node *prev = win->base;
	win->base = win->base->next;

	// Free the memory
	prev->next = NULL;
	free(prev);

	// Lower the total count
	win->total -= 1;

	return;
}

// Goes through the LLL and finds sendable nodes, returns
// seq number and changes the state. returns NULL when all
// sendable nodes hav beens sent
struct Node *to_send(struct Node *head)
{
	struct Node *current = head;
	while(current != NULL)
	{
		if(current->state == 2)
		{
			// First give the Node a timestampe
			current->time_stamp = time(NULL); 

			// Change the state of the node,
			// then return it to be sent.
			current->state = 1;
	
			return current;
		}
		
		current = current->next;
	}

	return NULL;
}

// Go through the LLL and find a specific seq number
struct Node *traversal(unsigned int to_find, struct Node *head)
{
	struct Node *current = head;
	
	while(current != NULL)
	{
		if(current->seq == to_find)
		{
			return current;
		}

		current = current->next;
	}

	// Correct packet not found
	return NULL;
}

// This function is for when an ack is recieved. The seq
// number is passed in and searched for in the LLL, if 
// found, its state is changed to alread ACK'ed
int state_change(Window *win, unsigned int to_find)
{
	struct Node *current = win->base;

	// Handling the special case, if the first node is the
	// match, we need to shift it out.
	if(current->seq == to_find)
	{

		// Make the state not usable, as it will be dropped
		// from the window with the shift;
		current->state = 4;
		shift_window(win);

		// Since the acked frame was found, increase the windows capacity
		increase_capacity(win);

		return 1;
	}
	current = current->next;

	while(current != NULL)
	{
		if(current->seq == to_find)
		{
			current->state = 1;

			// Inrease capacity in the window
			increase_capacity(win);

			return 0;
		}

		current = current->next;
	}

	// Return -1 if the node is not found. This could
	// happen if the window was decreased and the node
	// was removed already. (a late ACK)
	return -1;
}

// Simple function that increases a windows capacity, and makes
// sure not to go over the maximum allowed frames.
void increase_capacity(Window *win)
{
	if(win->capacity < win->max)
	{
		win->capacity += 1;
	}

	return;
}

// Fill a Frame with data
Frame *fill(FILE *to_read, Frame *to_fill, struct Node *getter)
{
	// Fill the nodes basic data, and get the seq number from
	// the passed in node
	to_fill->frame_type = 5;
	to_fill->seq_num = getter->seq;
	to_fill->ack = 0;
	to_fill->file_size = 0;

	// Now read data from the file into the Frame
	// get teh starting point from the node as well
	fseek(to_read, getter->begin_read, SEEK_SET);
	fread(&to_fill->data, 1, MinBuffer, to_read);

	return to_fill;
}
