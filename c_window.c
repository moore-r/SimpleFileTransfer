//Ryan Moore, CS 494, Programming Project Part 2
//Some functions to help manipulate the client window

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


// Initialize the LLL
struct CNode *c_init_LLL(int size)
{
	// Create a counter
	int count = 1;

	// Allocate mem and fill the first node
	struct CNode *head_c = (struct CNode*)malloc(sizeof(struct CNode));

	head_c->cpacket = NULL;
	head_c->cstate = 2;
	head_c->acked = 0;
	head_c->cseq = count; 
	head_c->cnext = NULL;

	struct CNode *currentc = head_c;
	// Loop and create the rest of the window
	while(count < size)
	{
		// Allocate a CNode
		struct CNode *cnew_node = (struct CNode *)malloc(sizeof(struct CNode));
		currentc->cnext = cnew_node;
		
		// Fill the CNode with data
		cnew_node->cpacket = NULL;
		cnew_node->cstate = 2;
		cnew_node->acked = 0;
		cnew_node->cseq = count;
		cnew_node->cnext = NULL;

		count += 1;
		currentc = currentc->cnext;
	}

	return head_c;
}


// Initialize the window with the first node
Window *c_init_window(int w_max, struct CNode *chead)
{
	Window *cinit = (Window *)malloc(sizeof(Window));

	cinit->max = w_max;
	cinit->capacity = w_max;
	cinit->total = w_max;
	cinit->base = NULL;
	cinit->cbase = chead;	
 
	return cinit;
}


// Shift the window, data has been delivered so
// the first slot is now unneeded. Since its an 
// LLL w/ a fix sized window, just going to have 
// move the first node to the end and empty it of 
// its contents
void c_shift(Window *win)
{
	struct CNode *current = win->cbase; 
	struct CNode *tail = win->cbase;
	unsigned int seq_tobe;
	seq_tobe = current->cseq;

	// Traverse to the end
	while(current->cnext != NULL)
	{
		current = current->cnext;
		seq_tobe += 1;
	}

	// Link it up into a circle
	current->cnext = tail;

	// Change the base to the second cnode
	win->cbase = tail->cnext;

	//unlink the circle
	tail->cnext = NULL;

	// Reset the contents of the now tail CNode
	tail->cpacket = NULL;
	tail->cseq = seq_tobe;
	tail->acked = 0;
	tail->cstate = 2;	

	return;
}


// Buffer the received data in the window
int data_buffer(Frame *filler, Window *win)
{
	int count = win->cbase->cseq;
	int counter = count;

	// First we need to find the correct slot to fill
	struct CNode *current = win->cbase;
 
	while(current != NULL)
	{
		if(current->cseq == filler->seq_num)
		{
			// Found the right spot!
			current->cpacket = filler;

			// Check if the data should be delivered (in order)
			// If count is 1, we are in the first CNode
			if(count == counter)
			{
				return 1;
			}

			// Change the state
			current->cstate = 0; 

			return 0;
		}

		count += 1;
		current = current->cnext;
	}

	// Nothing found
	return -1;
}


// Inorder delivery of data
char *deliver(char *file, struct CNode *to_fill, Window *to_shift)
{
	strcat(file, to_fill->cpacket->data);

	// Now shift the window
	c_shift(to_shift);

	// Lastly check if the next CNode in the window is up for delivery
	// Loop as there could be multiple cnodes in a row that are ready
	// the shifter function changes the Windows base for us.
	while(to_shift->cbase->cstate == 0)
	{
		strcat(file, to_fill->cpacket->data);
	}

	return file;
}


// Function that returns sequence numbers of frames to be acked
unsigned int finder(Window *win)
{
	// Loop through the window and find the 
	// Node with a state of 0
	struct CNode *current = win->cbase; 
	while(current != NULL)
	{
		// If the cpacket isnt null, then it has 
		// been buffered and is good to ack, also
		//  check is it has already been acked
		if(current->cpacket != NULL && current->acked == 0)
		{
			current->acked = 1;
			return current->cseq;
		}

		current = current->cnext;
	}

	// Nothing found to ack
	return -1;
}
