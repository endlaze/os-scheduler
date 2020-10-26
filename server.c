#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

void fifo_algorithm();
void server_sockets(int, int);
int process_count = 1;
int simulator_active = 1;
struct pcb *ready_head = NULL;
struct pcb *ready_tail = NULL;
pthread_mutex_t mutex;

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
    struct pcb *prev;
    int p_id;
    int burst;
    int priority;
};

struct pcb *create_pcb(char process[])
{
    // Creates a new PCB for the incoming process.
    char *delimiter = ",";
    struct pcb *proc_pcb = (struct pcb *)malloc(sizeof(struct pcb));
    // Inits the pcb with the corresponding info.
    proc_pcb->p_id = process_count;
    proc_pcb->burst = atoi(strtok(process, delimiter));
    proc_pcb->priority = atoi(strtok(NULL, delimiter));
    proc_pcb->prev = NULL;

    // Increases the process count by 1.
    process_count++;
    return proc_pcb;
};

void *job_scheduler(void *args)
{
    int listener = *((int *)args);
    while (simulator_active)
    {
        // Stores the address of the client.
        struct sockaddr_storage client;
        unsigned int address_size = sizeof(client);

        // Accepts an incoming connection.
        int connect = accept(listener, (struct sockaddr *)&client, &address_size);
        if (connect == -1)
        {
            printf("Error: The connection could not be stablished.\n");
        };

        // Buffer used to store the incoming process from the client.
        char client_process[256];
        recv(connect, client_process, sizeof(client_process), 0);

        // Creates a PCB for the incoming process and pushes it into the ready queue.
        struct pcb *pr_pcb = create_pcb(client_process);

        //Tries to lock the mutex to insert the process into the ready queue
        pthread_mutex_lock(&mutex);
        // Inserts the new PCB in the ready queue
        if (ready_head == NULL)
        {
            ready_head = pr_pcb;
            ready_tail = ready_head;
        }
        else
        {
            ready_tail->prev = pr_pcb;
            ready_tail = ready_tail->prev;
        };
        // Returns the mutex to unlock state
        pthread_mutex_unlock(&mutex);
        // Buffer used to send the new process info to the client.
        char new_process[256] = "";
        snprintf(new_process, sizeof new_process, "%d\t%d\t%d\n", pr_pcb->p_id, pr_pcb->burst, pr_pcb->priority);

        // Sends the info of the new process to the client.
        send(connect, new_process, strlen(new_process), 0);
    };
    pthread_exit(NULL);
};

void print_ready_queue()
{
    pthread_mutex_lock(&mutex);
    printf("\n\nPID\tBURST\tPRIORITY\n");
    // Prints all PBCs stored in the ready queue.
    struct pcb *node = ready_head;
    while (node != NULL)
    {
        char test[256] = "";
        snprintf(test, sizeof test, "%d\t%d\t%d\n", node->p_id, node->burst, node->priority);
        printf("%s", test);

        node = node->prev;
    };
    printf("\n\n\n");
    pthread_mutex_unlock(&mutex);
};

void *cpu_scheduler(void *args) {
    while (simulator_active)
    {
        sleep(1);
        fifo_algorithm();
    };
    pthread_exit(NULL);
}

void fifo_algorithm() {
    pthread_mutex_lock(&mutex);

    struct pcb *node = ready_head;
    if (node == NULL) {
        pthread_mutex_unlock(&mutex);
        return;
    };
    ready_head = node->prev;
    pthread_mutex_unlock(&mutex);
    char test[256] = "";
    snprintf(test, sizeof test, "Proceso PID %d\t con burst %d\t con prioridad %d entra en ejecucion\n", node->p_id, node->burst, node->priority);
    printf("%s", test);
    
    sleep(node->burst);
    free(node);
    return;
};

void *io_handler(void *args) {
    int option;
    while (simulator_active)
    {
        printf("Ingresar opcion: ");
        scanf("%d", &option);

        switch (option)
        {
        case 1:
            print_ready_queue();
            break;
        case 2:
            simulator_active = 0;
            break;
        default:
            break;
        }
    };
    pthread_exit(NULL);
    
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
    // Binds the socket to an IP address and port.
    bind_socket(listener, port);

    // Tries to listen for connections on the given port.
    if (listen(listener, max_connections))
    {
        printf("Error: Can't listen on port %d.\n", port);
        return;
    };

    printf("Info: Server listening on port: %d.\n\n", port);

    //Initialize the mutex
    pthread_mutex_init(&mutex, NULL);

    pthread_t job_scheduler_th;
    pthread_t cpu_scheduler_th;
    pthread_t io_th;

    if (pthread_create(&job_scheduler_th, NULL, job_scheduler, (void *)&listener) == -1)
    {
        printf("Error: Could not create the job scheduler.\n");
        return;
    };

    if (pthread_create(&cpu_scheduler_th, NULL, cpu_scheduler, (void *) NULL) == -1 ) 
    {
        printf("Error: Could not create the CPU scheduler.\n");
        return;
    };

    if (pthread_create(&io_th, NULL, io_handler, (void *) NULL) == -1 ) 
    {
        printf("Error: Could not create the CPU scheduler.\n");
        return;
    };

    pthread_exit(NULL);
};