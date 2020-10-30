#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

void init_server_conn(const char ip_address[], int port);
void client_sockets(const char ip_address[], int port);
void manual_client(const char file_path[]);
void auto_client(int min_burst, int max_burst, int min_cre_rate, int max_cre_rate);
const char *generate_process(int min_burst, int max_burst);
const int gen_cre_rate();
int MODE = 0;
int auto_client_active = 1;
char FILENAME[50];
int MINBURST;
int MAXBURST;
int MINCREATION;
int MAXCREATION;
struct sockaddr_in server_addr;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Error: Too few arguments, IP_ADDRESS, PORT and MODE are mandatory.\n");
        return 1;
    };

    time_t t;
    srand((unsigned)time(&t));

    const char *IP_ADDRESS = argv[1];
    const int PORT = atoi(argv[2]);
    MODE = atoi(argv[3]);

    if (MODE == 0) {
        printf("Enter filename: ");
        scanf("%s", &FILENAME);
    }
    else if (MODE == 1){
        printf("Enter MINBURST MAXBURST MINCREATIONRATE MAXCREATIONRATE: ");
        scanf("%d %d %d %d", &MINBURST, &MAXBURST, &MINCREATION, &MAXCREATION);
    }
    else
    {
        printf("Error, MODE is malformed. If MODE is manual you should provide textfile name, if auto mode is selected provide min and max burst\n");
        return 1;
    }
    

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
    int range = (rand() % (8 - 3 + 1)) + 3;
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
    switch (MODE)
    {
    case 0:
        manual_client(FILENAME);
        break;
    case 1:
        auto_client(MINBURST, MAXBURST, 1, 5);
        break;
    
    default:
        break;
    }
    
};

void manual_client(const char file_path[])
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

void auto_client(int min_burst, int max_burst, int min_cre_rate, int max_cre_rate)
{
    printf("Sending processes...\n\n\n");
    printf("PID\tBURST\tPRIORITY\n");

    while (auto_client_active)
    {

        const char *proc_data = generate_process(min_burst, max_burst);
        pthread_t proc_thread;

        if (pthread_create(&proc_thread, NULL, send_process, (void *)proc_data) == -1)
        {
            printf("Error: Could not create the thread\n");
            return;
        };
        int creation_rate = gen_cre_rate(min_cre_rate, max_cre_rate);
        sleep(creation_rate);
    };
    pthread_exit(NULL);
};

const int gen_cre_rate(int min_cre_rate, int max_cre_rate)
{
    return (rand() % (max_cre_rate - min_cre_rate + 1)) + min_cre_rate;
}

const char *generate_process(int min_burst, int max_burst)
{
    char *new_process = malloc(256);
    int burst = (rand() % (max_burst - min_burst + 1)) + min_burst;
    int priority = (rand() % 5) + 1;

    snprintf(new_process, sizeof new_process, "%d,%d\n", burst, priority);
    return (const char *)new_process;
};
