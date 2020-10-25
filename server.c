#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

void server_sockets(int, int);
int process_count = 1;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Error: Too few arguments, PORT is mandatory.\n");
        return 1;
    };

    const int PORT = atoi(argv[1]);
    server_sockets(PORT, 1);

    return 0;
};

int open_socket()
{
    printf("Info: Opening socket... \n");
    int new_socket = socket(PF_INET, SOCK_STREAM, 0);

    if (new_socket < 0)
    {
        printf("Error: Could not open the socket.\n");
    };

    return new_socket;
};

void bind_socket(int socket, int port)
{
    // Contains the information of the socket.
    struct sockaddr_in sock_addr;

    // Designates the type of addresses that the socket can communicate with.
    // AF_INET --> IP address.
    sock_addr.sin_family = AF_INET;
    // Designates the communication endpoint (port).
    sock_addr.sin_port = (in_port_t)htons(port);
    // Designates the IP address.
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int reuse_socket = 1;

    // Tries to reuse the socket.
    int can_reuse = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_socket, sizeof(int));

    if (can_reuse < 0)
    {
        printf("Error: Could not reuse the socket.\n");
    };

    // Binds the address to the socket.
    int bound_socket = bind(socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr));

    if (bound_socket < 0)
    {
        printf("Error: Address already in use.");
    };
}

// PCB struct
struct pcb
{
    int p_id;
    int burst;
    int priority;
};

struct pcb create_pcb(char process[])
{
    struct pcb proc_pcb;
    proc_pcb.p_id = process_count;

    char *delimiter = ",";

    proc_pcb.burst = atoi(strtok(process, delimiter));
    proc_pcb.priority = atoi(strtok(NULL, delimiter));

    process_count++;
    return proc_pcb;
}

void job_scheduler(int listener)
{
    while (1)
    {
        struct sockaddr_storage client;
        unsigned int address_size = sizeof(client);

        // Accepts a connection request from a client.
        int connect = accept(listener, (struct sockaddr *)&client, &address_size);
        if (connect == -1)
        {
            printf("Error: The connection could not be stablished.\n");
        };

        char client_message[2000];
        recv(connect, client_message, 2000, 0);

        struct pcb pr_pcb = create_pcb(client_message);
        char new_process[256] = "";

        snprintf(new_process, sizeof new_process, "%d\t%d\t%d\n", pr_pcb.p_id, pr_pcb.burst, pr_pcb.priority);
        printf("%s", new_process);

        send(connect, new_process, strlen(new_process), 0);
    };
}

void server_sockets(int port, int max_connections)
{
    // Creates the listener.
    printf("Info: Creating listener...\n");
    int listener = open_socket();

    if (listener < 0)
    {
        printf("Error: Could not create the listener.\n");
        return;
    };

    printf("Info: Binding socket...\n");
    // // Binds the socket to an IP address and port.
    bind_socket(listener, port);

    // // Tries to listen for connections on the given port.
    if (listen(listener, max_connections))
    {
        printf("Error: Can't listen on port %d.\n", port);
        return;
    };

    printf("Info: Server listening on port: %d.\n", port);

    printf("PID\tBURST\tPRIORITY\n");

    job_scheduler(listener);
};