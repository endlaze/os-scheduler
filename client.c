#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

#define MIN_RAND 3;
#define MAX_RAND 8;

void init_server_conn(const char ip_address[], int port);
void client_sockets(const char ip_address[], int port);
void manual_client(const char file_path[], struct sockaddr_in server_addr);

struct sockaddr_in server_addr;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Error: Too few arguments, IP_ADDRESS and PORT are mandatory.\n");
        return 1;
    };

    time_t t;
    srand((unsigned)time(&t));

    const char *IP_ADDRESS = argv[1];
    const int PORT = atoi(argv[2]);

    init_server_conn(IP_ADDRESS, PORT);
    client_sockets(IP_ADDRESS, PORT);

    return 0;
};

void init_server_conn(const char ip_address[], int port)
{
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = (in_port_t)htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);
};

int open_socket()
{
    int new_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (new_socket < 0)
    {
        printf("Error: Could not open the socket.\n");
    };

    return new_socket;
};

// Sends a process to the server and waits for a response;
void *send_process(void *args)
{
    int range = MAX_RAND - MIN_RAND + 1;
    range = (rand() % range) + MIN_RAND;

    sleep(range);

    char process_data[256];
    strncpy(process_data, (char *)args, sizeof(args));

    char recv_buffer[256];

    // Opens a new socket
    int client_socket = open_socket();

    // Attemps to connect to the server

    int connection = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (connection == -1)
    {
        printf("Error: Could not connect to server\n");
    };

    // Sends the process info to the server
    int msg_status = send(client_socket, process_data, sizeof(process_data), 0);

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
    free(args);
    pthread_exit(NULL);
};

void client_sockets(const char ip_address[], int port)
{
    const char filePath[] = "processes.txt";
    manual_client(filePath, server_addr);
};

void manual_client(const char file_path[], struct sockaddr_in server_addr)
{
    FILE *processes;
    processes = fopen(file_path, "r");

    char line[256];

    printf("Sending processes...\n\n\n");
    printf("PID\tBURST\tPRIORITY\n");

    while (fgets(line, sizeof(line), processes))
    {
        char *proc_data = (char *)malloc(sizeof(line));
        strcpy(proc_data, line);

        pthread_t proc_thread;

        if (pthread_create(&proc_thread, NULL, send_process, (void *)proc_data) == -1)
        {
            printf("Error: Could not create the thread\n");
            return;
        };
    };
    pthread_exit(NULL);
};
