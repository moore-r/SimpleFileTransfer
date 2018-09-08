// Ryan Moore, CS494, PP!
/* 
	Header file To define some constants, including buffer size
	and create packets. The packets will be as simple as possible.
*/

// Some buffers
#define SIZE 2048
#define MaxBuffer 65536
#define MinBuffer 1024
#define FileBuffer 64
#define PacketSize 1121 

// The packet structure I plan to use
// Types:
// 1 - connection
// 2 - ACK packet
// 3 - file request packet
// 4 - file size packet
// 5 - file data packet
// 6 - Close packet
typedef struct{
	unsigned int frame_type;
	unsigned int seq_num;
	unsigned int ack;
	unsigned int file_size;
	char file_path[FileBuffer];
	char data[MinBuffer];
} Frame;

/*
// Each node of the window will point to one 
// of these structures that hold where the info 
// it will be reading from the packet starts, 
// its state, and a time stamp.
typedef struct{
	long int begin_read;
	int state;
	time_t time_stamp;
	Packet * next;
} Packet;
*/


// Functions in SRCP.c
int delay(int num_secs);
char *ser(Frame *, char []);
Frame *des(char *, Frame *);
void clear_frame(Frame *);
void print_frame(Frame *);
Frame *create_close_packet();


/* 
	Header file that creates the window LLL
	and names the functions that will operate on it.
*/

/*
	The nodes of the server LLL, hold info of the
	Frames seqeuence number (fir easy access) were
	this particular Frame begins reading, the 
	state of the frame, and a timestampe so 
	timeouts can be monitered and of course 
	a next pointer.

	THE STATES of a Frame
	Server:
	0 - Already ACK'ed Frame
	1 - Sent Frame, but not yet ACK'ed
	2 - Usable, not yet sent
	3 - Not usable
*/
struct Node
{
	long int begin_read;
	unsigned int seq;
	int state;
	time_t time_stamp;
	struct Node * next;
};
/*
	The nodes of the client LLL, hold the Frame
	the state of the frame, knows if its been
	acked, and of course a next pointer.

	Client
	0 - Out of order -buffered- but acked
	1 - Expected but no received
	2 - Acceptable
	3 - Not usable
*/
struct CNode
{
	Frame * cpacket;
	int cstate;
	int acked;
	unsigned int cseq;
	struct CNode * cnext;
};


/*
	Below is the struct for the window, it will be a LLL
	of packets. it has fields
	for the total number of frames it curently holds (total),
	the total number of frames it can hold(max), the number
	of frames it should be holding(capacity), and a pointer
	to the base node of the window. 
*/
typedef struct{
	int max;
	int capacity;
	int total;
	struct Node *base;
	struct CNode *cbase; 
} Window;


/*
	Functions for the server in window.c
*/
struct Node *init_LLL();
Window *init_window(int, struct Node *);
void check_timeout(union sigval);
void increase_window(Window *, unsigned int);
void decrease_window(Window *);
void shift_window(Window *);
struct Node *to_send(struct Node *);
struct Node *traversal(unsigned int, struct Node *); 
int state_change(Window *, unsigned int);
void increase_capacity(Window *);
Frame *fill(FILE *, Frame *, struct Node *);

/*
	Functions for the client in c_window.c
*/
struct CNode *c_init_LLL(int);
Window *c_init_window(int, struct CNode *);
void c_shift(Window *);
int data_buffer(Frame *, Window *);
char *deliver(char *, struct CNode *, Window *to_shift);
unsigned int finder(Window *);


/*
	Simple struct to pass arguments with a pthread 
	to the receiver function in the server.
*/
struct arg_struct
{
	int arg1;	// Ack socket
	int arg2;	// Sending socket
	struct sockaddr_in arg3;	// sockaddr struct
	socklen_t arg4;	// The length of the socket
	Window * arg5;	// The window itself
	int arg6;	// The timeout
	FILE *arg7;	// The file ptr
	int arg8;	// Size of the file

	struct sockaddr_in arg9;	// sockaddr struct for other socker
	socklen_t arg10;	// The length of the socket for other socket

};

/*
	Simple struct to pass arguments with a pthread 
	to the receiver function in the client.
*/
struct c_arg_struct
{
	int arg1;	// Ack socket
	struct sockaddr_in arg2;	// sockaddr struct
	socklen_t arg3;	// The length of the socket
	Window *arg4;	// The head of the window
	int arg5;	// Size of the file
};
