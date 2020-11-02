#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

void fifo_algorithm();
void sjf_algorithm();
void hpf_algorithm();
void rr_algorithm(int quantum);

void server_sockets(int, int);
void add_times(int burst);
void delete_node(int p_id);
int process_count = 0;
int terminated_count = 0;
int simulator_active = 1;
struct pcb *ready_head = NULL;
struct pcb *ready_tail = NULL;
struct pcb *rr_last = NULL;
struct term_process *term_processes_head = NULL;
struct term_process *term_processes_tail = NULL;
pthread_mutex_t mutex;
int run_time = 0;
int exec_time = 0;
int total_tat = 0;
int total_wt = 0;

char algorithms[4][15] = {"FIFO", "SJF", "HPF", "Round Robin"};
int ALGORITHM = 0;
int QUANTUM = 1;
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Error: Too few arguments, PORT and ALGORITHM are mandatories.\n");
        return 1;
    };

    const int PORT = atoi(argv[1]);

    ALGORITHM = atoi(argv[2]);
    if (ALGORITHM > 3)
        printf("Error, algorithm must between 0 and 3\n\n");
    if (ALGORITHM == 3 && argc == 4)
        QUANTUM = atoi(argv[3]);
        if (QUANTUM <= 0) {
            printf("Error, QUATUM value must be a positive integer.\n");
            return 1;
        }

    else if (ALGORITHM == 3 && argc < 4)
    {
        printf("Round Robin algorithm requires QUANTUM value.\n");
        return 1;
    }
    

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

//

// PCB struct
struct pcb
{
    struct pcb *prev;
    int p_id;
    int arrival_time;
    int burst;
    int priority;
    int final_burst;
    int finish_time;
};
struct term_process
{
    struct term_process *next;
    int p_id;
    int burst;
    int arrival_time;
    int finish_time;
};

void terminate_process(int p_id, int burst, int arrival_time, int finish_time)
{
    struct term_process *terminated_process = (struct term_process *)malloc(sizeof(struct term_process));
    terminated_process->p_id = p_id;
    terminated_process->burst = burst;
    terminated_process->arrival_time = arrival_time;
    terminated_process->finish_time = finish_time;

    if (term_processes_head == NULL)
    {
        term_processes_head = terminated_process;
        term_processes_tail = term_processes_head;
    }
    else
    {
        term_processes_tail->next = terminated_process;
        term_processes_tail = term_processes_tail->next;
    };
    terminated_count++;
};

struct pcb *create_pcb(char process[])
{
    // Increases the process count by 1.
    process_count++;
    // Creates a new PCB for the incoming process.
    char *delimiter = ",";
    struct pcb *proc_pcb = (struct pcb *)malloc(sizeof(struct pcb));
    // Inits the pcb with the corresponding info.
    proc_pcb->p_id = process_count;
    proc_pcb->arrival_time = run_time;
    proc_pcb->burst = atoi(strtok(process, delimiter));
    proc_pcb->priority = atoi(strtok(NULL, delimiter));
    proc_pcb->prev = NULL;
    proc_pcb->final_burst = proc_pcb->burst;

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

void print_final_info()
{
    printf("\n\n|Process|Arrival|Finish\t|Burst\t|TAT\t|WT\t|\n");
    printf("-------------------------------------------------\n");

    struct term_process *node = term_processes_head;

    while (node != NULL)
    {
        const int p_id = node->p_id;
        const int burst = node->burst;
        const int arr_time = node->arrival_time;
        const int finish_time = node->finish_time;

        const int tat = finish_time - arr_time;
        total_tat += tat;

        const int wt = tat - burst;
        total_wt += wt;

        printf("|P%d\t|%d\t|%d\t|%d\t|%d\t|%d\t|\n", p_id, arr_time, finish_time, burst, tat, wt);

        struct term_process *temp = node;
        node = node->next;
        free(temp);
    };

    term_processes_head = NULL;

    printf("\n[+] Terminated processes: \t%d\n", terminated_count);
    printf("[+] CPU Idle Time: \t%d seconds\n", run_time - exec_time);

    if (terminated_count == 0)
    {
        terminated_count = 1;
    };

    const float av_tat = (float)total_tat / terminated_count;
    const float av_wt = (float)total_wt / terminated_count;

    printf("\n[+] Average TAT: %f seconds\n", av_tat);
    printf("[+] Average  WT: %f seconds\n", av_wt);
}

void print_ready_queue()
{
    pthread_mutex_lock(&mutex);
    printf("\n\n----| READY - QUEUE |----\n\n");
    printf("|PID\t|BURST\t|PRIORITY\n");
    printf("-------------------------\n");
    // Prints all PBCs stored in the ready queue.
    struct pcb *node = ready_head;
    while (node != NULL)
    {
        char test[256] = "";
        snprintf(test, sizeof test, "|%d\t|%d\t|%d\t\n", node->p_id, node->burst, node->priority);
        printf("%s", test);

        node = node->prev;
    };
    printf("\n\n");
    pthread_mutex_unlock(&mutex);
};

void *cpu_scheduler(void *args)
{
    printf("Executing %s algorithm\n\n", algorithms[ALGORITHM]);
    while (simulator_active)
    {
        sleep(1);
        run_time++;
        switch (ALGORITHM)
        {
        case 0:
            fifo_algorithm();
            break;
        case 1:
            sjf_algorithm();
            break;
        case 2:
            hpf_algorithm();
            break;
        case 3:
            rr_algorithm(QUANTUM);
            break;
        default:
            break;
        }
    };
    pthread_exit(NULL);
}

void fifo_algorithm()
{
    pthread_mutex_lock(&mutex);

    struct pcb *node = ready_head;
    if (node == NULL)
    {
        pthread_mutex_unlock(&mutex);
        return;
    };
    ready_head = node->prev;

    pthread_mutex_unlock(&mutex);
    printf("[+] Executing process - PID = %d - BURST = %d - PRIORITY = %d\n", node->p_id, node->burst, node->priority);
    printf("[-] Process with PID %d finished.\n\n", node->p_id);
    sleep(node->burst);
    add_times(node->burst);
    terminate_process(node->p_id, node->burst, node->arrival_time, run_time);
    free(node);
    return;
};

void sjf_algorithm()
{
    pthread_mutex_lock(&mutex);
    struct pcb *node = ready_head;
    struct pcb *min = ready_head;
    struct pcb *min_forw = NULL;
    if (node == NULL)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }
    while (node != NULL && node->prev != NULL)
    {

        if (node->prev->burst < min->burst)
        {
            min = node->prev;
            min_forw = node;
        }
        node = node->prev;
    };

    if (min_forw == NULL)
    {
        ready_head = min->prev;
    }
    else
    {
        min_forw->prev = min->prev;
    }

    pthread_mutex_unlock(&mutex);
    printf("[+] Executing process - PID = %d - BURST = %d - PRIORITY = %d\n", min->p_id, min->burst, min->priority);
    sleep(min->burst);
    printf("[-] Process with PID %d finished.\n\n", min->p_id);
    add_times(min->burst);
    terminate_process(min->p_id, min->burst, min->arrival_time, run_time);
    free(min);
}

void hpf_algorithm()
{
    pthread_mutex_lock(&mutex);
    struct pcb *node = ready_head;
    struct pcb *min = ready_head;
    struct pcb *min_forw = NULL;

    if (node == NULL)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }
    while (node != NULL && node->prev != NULL)
    {

        if (node->prev->priority < min->priority)
        {
            min = node->prev;
            min_forw = node;
        }
        node = node->prev;
    };

    if (min_forw == NULL)
    {
        ready_head = min->prev;
    }
    else
    {
        min_forw->prev = min->prev;
    }

    pthread_mutex_unlock(&mutex);
    printf("[+] Executing process - PID = %d - BURST = %d - PRIORITY = %d\n", min->p_id, min->burst, min->priority);
    sleep(min->burst);
    printf("[-] Process with PID %d finished.\n\n", min->p_id);
    add_times(min->burst);
    terminate_process(min->p_id, min->burst, min->arrival_time, run_time);
    free(min);
}

void rr_algorithm(int quantum)
{
    pthread_mutex_lock(&mutex);
    struct pcb *node = NULL;
    int sleep_time = quantum;

    // If rr_last was the las element of the list, node will be the head
    if (rr_last == NULL || rr_last->prev == NULL)
        node = ready_head;
    else
    {
        node = rr_last->prev;
    }

    if (node == NULL)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }

    if (node->burst <= quantum)
    {
        sleep_time = node->burst;

        // If node is the head, change the head to the prev
        if (node->p_id == ready_head->p_id)
        {
            ready_head = node->prev;
        }

        // If node isn't the head, then change the last prev to node prev
        if (rr_last != NULL && rr_last->prev != NULL)
        {
            rr_last->prev = node->prev;
        }

        printf("[+] Executing process - PID = %d - BURST = %d - PRIORITY = %d\n", node->p_id, sleep_time, node->priority);
        printf("[-] Process with PID %d finished.\n\n", node->p_id);

        // If node is the last element, set rr_last to null, else, free the node.
        if (rr_last != NULL && rr_last->p_id == node->p_id )
        {
            terminate_process(node->p_id, node->final_burst, node->arrival_time, run_time + sleep_time - 1);
            rr_last = NULL;
        }
        else
        {
            terminate_process(node->p_id, node->final_burst, node->arrival_time, run_time + sleep_time - 1);
            free(node);
        }
    }
    else
    {
        node->burst = node->burst - quantum;
        rr_last = node;
        printf("[+] Executing process - PID = %d - BURST = %d - PRIORITY = %d\n", node->p_id, sleep_time, node->priority);
    }

    pthread_mutex_unlock(&mutex);
    add_times(sleep_time);
    sleep(sleep_time);
}

void *io_handler(void *args)
{
    int option;
    while (simulator_active)
    {
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
    print_final_info();
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

    if (pthread_create(&cpu_scheduler_th, NULL, cpu_scheduler, (void *)NULL) == -1)
    {
        printf("Error: Could not create the CPU scheduler.\n");
        return;
    };

    if (pthread_create(&io_th, NULL, io_handler, (void *)NULL) == -1)
    {
        printf("Error: Could not create the CPU scheduler.\n");
        return;
    };

    pthread_exit(NULL);
};

void add_times(int burst)
{
    run_time += burst - 1;
    exec_time += burst;
}