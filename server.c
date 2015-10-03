#include <stdio.h>
#include <string.h>    
#include <stdlib.h>    
#include <sys/socket.h>
#include <arpa/inet.h>   
#include <pthread.h> 
#include <unistd.h>
#include <dirent.h>
// #include <signal.h>

/* Fail Code */
#define SERVER_SOCKET_FAIL 1
#define SERVER_BIND_FAIL 2
#define PTHREAD_CREATE_FAIL 3

#define SERVER_PORT 8888
#define MAX_CLIENT 3

#define MAX_FILENAME_LEN 100
#define MAX_COMMAND_LEN 50
#define MAX_MESSAGE_LEN 1024
 
void *connection_handler(void *);
void message_trim(char*);
void signalHandler(int);
 
int main(int argc , char *argv[])
{
	/* server's and client's file descriptor => -1 means error */
    int ss_desc , cs_desc;
    struct sockaddr_in server , client;
    char *message;
     
    // Create socket
    ss_desc = socket(AF_INET , SOCK_STREAM , 0);	// IPv4, TCP
    if (ss_desc == -1)
	{
        fprintf(stderr, "Fail to create server's socket");
		return SERVER_SOCKET_FAIL;
	}
     
    // Server socket info. sockaddr_in struct
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( SERVER_PORT );
     
    // Bind
    if( bind(ss_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        puts("bind failed");
        return SERVER_BIND_FAIL;
    }
    puts("bind done");
     
    // Listen. Accept at most MAX_CLIENT clients
    listen(ss_desc , MAX_CLIENT);
     
    // accept() to wait for new clients
    puts("Waiting for incoming connections...");
    int structlen = sizeof(struct sockaddr_in);
    while( (cs_desc = accept(ss_desc, (struct sockaddr *)&client, (socklen_t*)&structlen)) )
    {
        puts("Connection accepted");
         
		// Create pthread for each client
        pthread_t thread;
        int* param_desc = malloc(sizeof(int));
        *param_desc = cs_desc;
         
        if( pthread_create( &thread , NULL ,  connection_handler , (void*) param_desc) < 0)
        {
            perror("Fail to create new pthread");
            return PTHREAD_CREATE_FAIL;
        }
    }
    
    if (cs_desc<0)
    {
        perror("Accept failed");
        return 1;
    }
     
    return 0;
}

/* Ctrl-C => close socket before exit */
// void signalHandler(int sig)
// {
//     if(sig == SIGNIT)
//     {
        
//     }
// }

/* remove trailing newline character */
void message_trim(char* msg)
{
    int len = strlen(msg);
    msg[len-1] = '\0';
}

/* Handle client's process */
void *connection_handler(void *socket_desc)
{
    int sock = *(int*)socket_desc;
    int read_size;
    char *server_message , client_command[MAX_COMMAND_LEN], client_message[MAX_MESSAGE_LEN];
    char filename[MAX_FILENAME_LEN];
     
	// Welcome message
    server_message = "Welcome to remote editor.\nPlease type the command you need.\n";
    write(sock , server_message , strlen(server_message));
     
	
	// Get command from user and response
    while( (read_size = recv(sock , client_command , MAX_COMMAND_LEN-1 , 0)) > 0 )
    {
        // DEBUG - CHECK RECEIVE COMMAND
        message_trim(client_command);
        printf("get command %s\n", client_command);

		//	c => create new file
		if(client_command[0] == 'c')
		{
			server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
			if((read_size = recv(sock, filename, MAX_FILENAME_LEN, 0)) > 0)  // get filename from client
            {
                message_trim(filename);
                /* check whether the file exists */
    			if(access(filename, F_OK) != -1)
    			{
    				server_message = "The file has already existed!\n";
                    write(sock, server_message, strlen(server_message));
    			}
    			else
    			{
                    FILE* fp = fopen(filename, "w");  // create new file
                    fclose(fp);
                    server_message = "Create successfully\n";
                    write(sock, server_message, strlen(server_message));
                }

            }
            else
                perror("recv failed");
		}

        // e => edit
        if(client_command[0] == 'e')
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN, 0)) > 0)  // get filename from client
            {
                message_trim(filename);
                if(access(filename, F_OK) != -1)
                {
                    server_message = "Stop and save by `EOF`.\n";
                    write(sock, server_message, strlen(server_message));

                    FILE* fp = fopen(filename, "a+");
                    if(fp)
                    {
                        while((read_size = recv(sock, client_message, MAX_MESSAGE_LEN-1, 0)) > 0)
                        {
                            client_message[read_size] = '\0';
                            if(strcmp(client_message, "`EOF`\n") == 0)
                                break;
                            printf("get message: %s\n", client_message);
                            fprintf(fp, "%s", client_message);
                            fflush(fp);
                        }
                        server_message = "Saved!!\n";
                        write(sock, server_message, strlen(server_message));
                    }
                    else
                        fprintf(stderr, "Fail to open the file\n");
                }
                else
                {
                    server_message = "File doesn't exist\n";
                    write(sock, server_message, strlen(server_message));
                }

            }
            else
                perror("recv failed");

        }

        // r => remove
        if(client_command[0] == 'r')
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN, 0)) > 0)  // get filename from client
            {
                message_trim(filename);
                if(access(filename, F_OK) != -1)
                {
                    remove(filename);
                }
                else
                {
                    server_message = "File doesn't exist\n";
                    write(sock, server_message, strlen(server_message));
                }

            }
            else
                perror("recv failed");
        }   

        // l => list directory
        if(client_command[0] == 'l')
        {
            struct dirent **namelist;
            int n;
            // assign "current" files to namelist and sort by alphasort without filter.
            n = scandir(".", &namelist, NULL, alphasort);  
            if(n<0)
                perror("scandir error");
            else
            {
                int i;
                for(i=0; i<n; i++)
                {
                    write(sock, namelist[i]->d_name, strlen(namelist[i]->d_name));
                    write(sock, " ", 1);
                    free(namelist[i]);
                }
                write(sock, "\n", 1);
            }
        }

        // d => download
        if(client_command[0] == 'l')
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN, 0)) > 0)  // get filename from client
            {
                message_trim(filename);
                char segment[MAX_MESSAGE_LEN];
                if(access(filename, F_OK) != -1)
                {
                    FILE* fp = fopen(filename, "r");
                    while(fgets(segment, MAX_MESSAGE_LEN, fp) != NULL)
                    {
                        write(sock, segment, strlen(segment));  // tranfer data to client until EOF
                    }
                }
                else
                {
                    server_message = "File doesn't exist\n";
                    write(sock, server_message, strlen(server_message));
                }

            }
            else
                perror("recv failed");
        }


		// q => quit
		if(client_command[0] == 'q')
		{
			server_message = "Bye Bye\n";
			write(sock, server_message, strlen(server_message));
			break;
		}	
    }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }

    close(sock);

    //Free the socket pointer
    free(socket_desc);

    return 0;
} 
