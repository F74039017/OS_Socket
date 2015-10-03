#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> // socket
#include <arpa/inet.h> //inet_addr
#include <pthread.h>

// Fail code
#define RECV_PTHREAD_FAIL 1
#define SEND_PTHREAD_FAIL 2
#define DOWN_PTHREAD_FAIL 3

#define MAX_FILENAME_LEN 100
#define MAX_MESSAGE_LEN 1024

//  MODE
#define MESSAGE_MODE 1
#define DOWNLOAD_MODE 2

int mode;

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
        fgets(message, MAX_MESSAGE_LEN, stdin);
        write_size = write(sock, message, strlen(message));

        if(message[0] == 'd')
            mode = DOWNLOAD_MODE;
    }


    // Free the socket pointer
    free(socket_desc);
}