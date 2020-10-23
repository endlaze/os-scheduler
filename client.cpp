#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <list>
#include <iterator>

using namespace std;

void client_sockets(const char ip_address[], int port);
void manual_client(const char file_path[], struct sockaddr_in server_addr);

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        printf("Error: Too few arguments, IP_ADDRESS and PORT are mandatory.\n");
        return 1;
    };

    const char *IP_ADDRESS = argv[1];
    const int PORT = atoi(argv[2]);
    client_sockets(IP_ADDRESS, PORT);

    return 0;
};

int open_socket()
{
    printf("Info: Opening socket... \n");
    // Opens a new socket
    int new_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (new_socket < 0)
    {
        printf("Error: Could not open the socket.\n");
    };

    return new_socket;
};

// Contains the server address and the process data
struct proc_thr_args
{
    struct sockaddr_in server_addr;
    char process_data[256];
};

// Sends a process to the server and waits for a response;
void *send_process(void *args)
{
    struct proc_thr_args *th_args = (struct proc_thr_args *)args;

    char recv_buffer[256];

    // Opens a new socket
    int client_socket = open_socket();

    struct sockaddr_in sock_in = th_args->server_addr;

    // Attemps to connect to the server

    int connection = connect(client_socket, (struct sockaddr *)&sock_in, sizeof(sock_in));

    if (connection == -1)
    {
        printf("Error: Could not connect to server\n");
    };

    // Send the process info to the server

    int msg_status = send(client_socket, th_args->process_data, sizeof(th_args->process_data), 0);
    if (msg_status == -1)
    {
        printf("Error: The message could not be sent.\n");
    };

    // Waits for a server response
    int read_msg_status = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0);

    if (read_msg_status == -1)
    {
        printf("Error: The message could not be received\n");
    };

    printf("%s", recv_buffer);
    close(client_socket);
    pthread_exit(NULL);
}

void client_sockets(const char ip_address[], int port)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = (in_port_t)htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    const char filePath[] = "processes.txt";
    manual_client(filePath, server_addr);
}

void manual_client(const char file_path[], struct sockaddr_in server_addr)
{
    FILE *processes;
    processes = fopen(file_path, "r");
    char line[256];

    struct proc_thr_args pta;
    pta.server_addr = server_addr;

    list<pthread_t> threads;

    while (fgets(line, sizeof(line), processes))
    {
        int line_len = strlen(line);

        strncpy(pta.process_data, line, line_len);

        pthread_t proc_thread;
        if (pthread_create(&proc_thread, NULL, send_process, &pta) == -1)
        {
            printf("Error: Could not create the thread\n");
            return;
        };

        threads.push_front(proc_thread);
    };

    list<pthread_t>::iterator it;
    for (it = threads.begin(); it != threads.end(); it++)
    {
        pthread_join(*it, NULL);
    }
}
