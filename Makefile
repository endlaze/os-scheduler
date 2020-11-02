all: client.c server.c
	gcc client.c -o client.o -pthread
	gcc server.c -o server.o -pthread

server: server.c
	gcc server.c -o server.o -pthread

client: client.c
	gcc client.c -o client.o -pthread