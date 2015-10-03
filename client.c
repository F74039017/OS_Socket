#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> // socket
#include <arpa/inet.h> //inet_addr
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#define TRUE 1
#define FALSE 0

// Fail code
#define RECV_PTHREAD_FAIL 1
#define SEND_PTHREAD_FAIL 2
#define DOWN_PTHREAD_FAIL 3

#define MAX_FILENAME_LEN 100
#define MAX_MESSAGE_LEN 1024

/* thansfer flags */
#define TRANSFER_WAIT 0
#define TRANSFER_OK 1
#define TRANSFER_DISCARD 2

//  MODE
#define MESSAGE_MODE 1
#define DOWNLOAD_MODE 2

sem_t semaphore;

int mode;
char savedName[MAX_FILENAME_LEN];
int trans_state;
FILE *fp;

void* receive_handler(void* );
void* write_handler(void* );

int main(int argc , char *argv[])
{
    int socket_desc;
    char* ip;
    int port;
    struct sockaddr_in server;

    if(argc != 3)
    {
        puts("Use default ip: localhost, port: 8888");
        ip = "127.0.0.1";
        port = 8888;
    }
    else
    {
        if(argv[1]=="localhost")
            ip = "127.0.0.1";
        ip = argv[1];
        port = atoi(argv[2]);
    }
     
    // Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
         
    server.sin_addr.s_addr = inet_addr( ip );
    server.sin_family = AF_INET;
    server.sin_port = htons( port );
 
    // Connect to remote server
    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("connect error");
        return 1;
    }
    puts("Connected");

    /* init threads shared variable */
    sem_init(&semaphore, 0, 1);
    trans_state = TRANSFER_WAIT;
    mode = MESSAGE_MODE;

    pthread_t rthread;
    pthread_t wthread;
    int* rparam_desc = malloc(sizeof(int));
    *rparam_desc = socket_desc;
    int* wparam_desc = malloc(sizeof(int));
    *wparam_desc = socket_desc;


    if(pthread_create( &rthread, NULL, receive_handler, (void*)rparam_desc) < 0)
    {
         perror("Fail to create recv pthread");
        return RECV_PTHREAD_FAIL;
    }
    if(pthread_create( &wthread, NULL, write_handler, (void*)wparam_desc) < 0)
    {
         perror("Fail to create send pthread");
        return SEND_PTHREAD_FAIL;
    }

    /*  Terminate the program if recv() detect disconnection */
    // pthread_join(wthread, NULL);
    pthread_join(rthread, NULL);

    close(socket_desc);
    return 0;
}

void* receive_handler(void* socket_desc)
{
    int sock = *(int*)socket_desc;
    int read_size;
    char message[MAX_MESSAGE_LEN];

    /* Get server message and output */
    while((read_size = recv(sock, message, MAX_MESSAGE_LEN-1, 0)) > 0)
    {
        // DEBUG - INPUT MESSAGE INTEGERTY
        // printf("\nread_size = %d\n", read_size);

        /**
        DOWNLOAD_MODE is invoked in wthread when output command 'd'.
        MESSAGE_MODE will switch back automatically when download has been
        **/

        if(mode == MESSAGE_MODE)
        {
            message[read_size] = '\0';
            printf("%s", message);
            fflush(stdout);
            memset(message, 0, sizeof(message));
        }
        else if(mode == DOWNLOAD_MODE)  // invoke in wthread. command 'd'
        {
            /* Show filename asking message */
            message[read_size] = '\0';
            printf("%s", message);
            fflush(stdout);

            // Response the filename in wthread.
            // Block and wait for setting trans_state
            sem_wait(&semaphore);

            // DEBUG - CHECK RTHREAD GET IN SEMAPHORE
            // printf("in semaphore\n");

            /* start to receive data */
            if(trans_state == TRANSFER_OK)
            {
                while((read_size = recv(sock, message, MAX_MESSAGE_LEN-1, 0)) > 0)
                {
                    //++ write output to the file
                    // DEBUG - OUTPUT DATA TO STDIN
                    message[read_size] = '\0';
                    printf("%s", message);
                    fflush(stdout);
                }                  
            }

            sem_post(&semaphore);

            /* transfer done. resume message mode */
            if(fp)
                fclose(fp);

            /* resume to message mode */
            mode = MESSAGE_MODE;
            printf("mode change %d\n", mode);
        }
    }

     if(read_size == 0)
    {
        puts("Server disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }

    // Free the socket pointer
    free(socket_desc);
}

void* write_handler(void* socket_desc)
{
    int sock = *(int*)socket_desc;
    int write_size;
    char message[MAX_MESSAGE_LEN];

    /* Get server message and output */
    while(1)
    {
        /* get command and send it to server */
        fgets(message, MAX_MESSAGE_LEN, stdin);
        if(mode == MESSAGE_MODE)
            write_size = write(sock, message, strlen(message));
        
        if(message[0] == 'd')
        {
            /* change to download mode => prevent from invoking other commands */
            mode = DOWNLOAD_MODE;

            /* set semaphore to block rthread recv() */
            sem_wait(&semaphore);
            // DEBUG - CHECK SEMAPHORE IS LOCKED
            // printf("semaphore lock\n");

            /* write filename to server */
            fgets(savedName, MAX_FILENAME_LEN, stdin);
            savedName[strlen(savedName)-1] = '\0';  // trim
            write_size = write(sock, savedName, strlen(savedName));

            /* check existance and ask overwrite */
            if(access(savedName, F_OK) != -1)
            {
                int overwrite = FALSE;

                /* check overwrite */
                printf("The file has already existed, do you want to overwrite?[y/n] ");
                char check;
                scanf("%c", &check);
                if(check == 'y')
                    overwrite = TRUE;

                /* send transfer flag to server */
                if(overwrite)
                {
                    fp = fopen(savedName, "w");
                    trans_state = TRANSFER_OK;
                    write(sock, "TRANSFER_OK", 11);
                }
                else
                {
                    trans_state = TRANSFER_DISCARD;
                    write(sock, "TRANSFER_DISCARD", 16);
                }

            }
            else
            {
                fp = fopen(savedName, "w");
                trans_state = TRANSFER_OK;
                write(sock, "TRANSFER_OK", 11);
            }

            /* trans_state setting finish */
            sem_post(&semaphore);
        }
    }


    // Free the socket pointer
    free(socket_desc);
}