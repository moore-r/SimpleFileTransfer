all: client server

client: SRCP_C.c SRCP.c c_window.c
	gcc -g -Wall -o client SRCP_C.c SRCP.c c_window.c -lpthread -lrt

server: SRCP_S.c SRCP.c window.c
	gcc -g -Wall -o server SRCP_S.c SRCP.c window.c -lpthread -lrt
