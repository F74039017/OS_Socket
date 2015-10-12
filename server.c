#include <stdio.h>
#include <string.h>    
#include <stdlib.h>    
#include <sys/socket.h>
#include <arpa/inet.h>   
#include <pthread.h> 
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <signal.h>

#include "aes.h"
#define CBC 1
#define ECB 1

#define TRUE 1
#define FALSE 0

/* Fail Code */
#define SERVER_SOCKET_FAIL 1
#define SERVER_BIND_FAIL 2
#define PTHREAD_CREATE_FAIL 3

#define SERVER_PORT 8888
#define MAX_CLIENT 3

#define MAX_FILENAME_LEN 100
#define MAX_COMMAND_LEN 50
#define MAX_MESSAGE_LEN 4096

/* thansfer flags */
#define TRANSFER_WAIT 0
#define TRANSFER_OK 1
#define TRANSFER_DISCARD 2

struct node
{
    int desc;
    int isDownload;
    struct node *next;
}*head;

void message_trim(char*);
 
void *connection_handler(void *);
void *server_cmd_handler();

// linked list handle client sockets
int client_cnt = 0;
void append(int);
void addBeginning(int);
void addAfter(int, int);
void insert(int);
int delete(int);

void setDownloadFlag(int, int);
void send2All(char *);

// clear before exit
void clean();
void intHandler(int sig);

// DEBUG - CHECK THE HEX OF THE STRING
// static void phex(uint8_t* str);
 
int main(int argc , char *argv[])
{
    signal(SIGINT, intHandler);

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

    // set reuse address => avoid TCP TIME_WAIT session
    /* Enable address reuse */
    int on = 1;
    setsockopt( ss_desc, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
     
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

    // Create pthread for broadcast
    pthread_t thread;
    if( pthread_create( &thread , NULL ,  server_cmd_handler , NULL) < 0)
    {
        perror("Fail to create broadcast thread");
        return PTHREAD_CREATE_FAIL;
    }
     
    // Listen. Accept at most MAX_CLIENT clients
    listen(ss_desc , MAX_CLIENT);
     
    // accept() to wait for new clients
    puts("Waiting for incoming connections...");
    int structlen = sizeof(struct sockaddr_in);
    while( (cs_desc = accept(ss_desc, (struct sockaddr *)&client, (socklen_t*)&structlen)) )
    {
        //  DEBUG - CHECK CLIENT DESC
        printf("Client %d connect!!\n", cs_desc);
         
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

/* Handle client's process */
void *connection_handler(void *socket_desc)
{
    int sock = *(int*)socket_desc;
    size_t read_size;
    char *server_message , client_command[MAX_COMMAND_LEN], client_message[MAX_MESSAGE_LEN];
    char filename[MAX_FILENAME_LEN];

    // add client list
    client_cnt++;
    insert(sock);
     
	// Welcome message
    server_message = "#############################################\n";
    write(sock , server_message , strlen(server_message));
    server_message = "Welcome to Internet editor\n";
    write(sock , server_message , strlen(server_message));
    server_message = "#############################################\n";
    write(sock , server_message , strlen(server_message));
    server_message = "There are some options you can choose below:\n";
    write(sock , server_message , strlen(server_message));
    server_message = "#############################################\n";
    write(sock , server_message , strlen(server_message));
    server_message = "1. create\n2. edit\n3. cat\n4. remove (rm)\n5. list (ls)\n6. download (dl)\n7. encrypt (en)\n8. decrypt (de)\n9. rename\n10. quit (bye)\n11. alias <command> <alias>\n\n";
    write(sock , server_message , strlen(server_message));
	
	// Get command from user and response
    while( (read_size = recv(sock , client_command , MAX_COMMAND_LEN-1 , 0)) > 0 )
    {
        // DEBUG - CHECK RECEIVE COMMAND
        printf("get command %s\n", client_command);

		//	c => create new file
		if(strncmp(client_command, "create", 6) == 0)
		{
			server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
			if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
                /* check whether the file exists */
                // BUG - IF FILE NAME IS EMPTY => CRASH
                if(filename[0]=='\n')
                {
                    server_message = "File name can't be empty\n";
                    write(sock, server_message, strlen(server_message));
                }
    			else if(access(filename, F_OK) != -1)
    			{
    				server_message = "The file has already existed!\n";
                    write(sock, server_message, strlen(server_message));
    			}
    			else
    			{
                    FILE* fp = fopen(filename, "w");  // create new file
                    fclose(fp);
                    fp = NULL;
                    server_message = "Create successfully\n";
                    write(sock, server_message, strlen(server_message));
                }

            }
            else
                perror("recv failed");
		}

        // e => edit
        // BUG - GET GARBAGE POSTFIX CHAR AFTER OTHER COMMAND
        else if(strncmp(client_command, "edit", 4) == 0)
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
                if(access(filename, F_OK) != -1)
                {
                    server_message = "Stop and save by `EOF` in a line.\n";
                    write(sock, server_message, strlen(server_message));

                    FILE* fp = fopen(filename, "a+");
                    if(fp)
                    {
                        while((read_size = recv(sock, client_message, MAX_MESSAGE_LEN-1, 0)) > 0)
                        {
                            client_message[read_size] = '\n';
                            client_message[read_size+1] = '\0';
                            if(strcmp(client_message, "`EOF`\n") == 0)
                                break;
                            // printf("get message: %s\n", client_message);
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

        // cat
        else if(strncmp(client_command, "cat", 3) == 0)
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
                if(access(filename, F_OK) != -1)
                {
                    FILE* fp = fopen(filename, "r");
                    char segment[MAX_MESSAGE_LEN];
                    while(fgets(segment, MAX_MESSAGE_LEN, fp))
                    {
                        segment[strlen(segment)] = '\0';
                        write(sock, segment, strlen(segment));
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

        // r => remove
        else if(!(strncmp(client_command, "remove", 6)&&strncmp(client_command, "rm", 2)))
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
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
        else if(!(strncmp(client_command, "list", 4)&&strncmp(client_command, "ls", 2)))
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
        /*  1.  Normal send data
        *   2.  Target file doesn't exist => discard
        *   3.  Client doesn't allow to overwrite data => discard */
        else if(!(strncmp(client_command, "download", 8)&&strncmp(client_command, "dl", 2)))
        {
            /* ask downloaded filename */
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));
            if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
                // DEBUG - CHECK TRIMED FILE NAME SENDED BY CLIENT
                printf("%s\n", filename);
                
                uint8_t segment[MAX_MESSAGE_LEN];
                if(access(filename, F_OK) != -1)
                {
                    /* send file's size */
                    struct stat st;
                    int result = stat(filename, &st);
                    long long fsize = st.st_size;
                    sprintf(segment, "%lld", fsize);
                    // printf("convert file size = %s\n", segment);
                    write(sock, segment, strlen(segment));

                    /* Wait for TRANSFER signal */
                    int transfer_flag = TRANSFER_WAIT;
                    while(transfer_flag == TRANSFER_WAIT)
                    {
                        // printf("start wait\n");
                        if((read_size = recv(sock, client_message, MAX_MESSAGE_LEN-1, 0)) > 0)
                        {
                            client_message[read_size] = '\0';
                            if(strcmp(client_message, "TRANSFER_OK") == 0)
                                transfer_flag = TRANSFER_OK;
                            else if(strcmp(client_message, "TRANSFER_DISCARD") == 0)
                                transfer_flag = TRANSFER_DISCARD;
                        }   
                        else if(read_size == -1)
                            perror("tranfer flag recv failed");
                        else if(read_size == 0)
                            break;
                        // printf("trans_state = %d\n", transfer_flag);
                    }


                    if(transfer_flag == TRANSFER_OK)
                    {
                        setDownloadFlag(sock, TRUE);
                        // puts("ready to send the data");
                        /* Start transfer data */
                        FILE* fp = fopen(filename, "r");
                        while(read_size = fread(segment, sizeof(uint8_t), MAX_MESSAGE_LEN-1, fp))
                        {
                            write(sock, segment, read_size);  // tranfer data to client until EOF
                        }
                        setDownloadFlag(sock, FALSE);
                    }
                }
                else
                {
                    server_message = "FILE_NOT_EXIST";  // transfer flag
                    write(sock, server_message, strlen(server_message));
                }

            }
            else
                perror("recv failed");
        }

        // AES128 ECB encrypt
        else if(!(strncmp(client_command, "encrypt", 7)&&strncmp(client_command, "en", 2)))
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));

            if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
                /* concat .cip ext */
                const uint8_t ext[] = ".cip";
                char tempfilename[MAX_FILENAME_LEN];
                strcpy(tempfilename, filename);
                int namelen = strlen(filename);
                strncpy(tempfilename+namelen, ext, 5);
                // DEBUG - CHECK EXT FILENAME
                // printf("%s %s\n", filename, tempfilename);  


                if(access(filename, F_OK) != -1)
                {
                    /* ask client for 16 bytes password */
                    server_message = "Password <= 16 letters: ";
                    write(sock, server_message, strlen(server_message));

                    uint8_t key[16];
                    if((read_size = recv(sock, client_message, MAX_MESSAGE_LEN-1, 0)) > 0)
                    {
                        /* discard letter if the length is more than 16 */
                        strncpy(key, client_message, 16);
                        // printf("key length = %zu\n", strlen(client_message));
                        if(strlen(client_message) > 16+1)
                        {
                            server_message = "Key too long.\nThe letters longer than 16 will be discarded.\n";
                            write(sock, server_message, strlen(server_message));
                        }

                        FILE *pfp = fopen(filename, "r");
                        FILE *cfp = fopen(tempfilename, "w");
                        const segment_size = 16;
                        uint8_t in[segment_size];
                        uint8_t out[segment_size];

                        /* write file size */
                        uint64_t fsize;
                        struct stat st;
                        int result = stat(filename, &st);
                        fsize = st.st_size;
                        fwrite(&fsize, sizeof(uint64_t), 1, cfp);

                        /* write contents */
                        while(read_size = fread(in, sizeof(uint8_t), segment_size, pfp))
                        {

                            AES128_ECB_encrypt(in, key, out);
                            
                            // DEBUG - CHECK DECRYPT
                            // printf("Plain text: \n");
                            // phex(in);
                            // phex(out);
                            // AES128_ECB_decrypt(out, key, rec);
                            // phex(rec);

                            fwrite(out, sizeof(uint8_t), 16, cfp);
                        }
                        remove(filename);
                        rename(tempfilename, filename);
                        fclose(pfp);
                        pfp = NULL;
                        fclose(cfp);  
                        cfp = NULL; 
                    }
                    else
                        perror("Fail to get key");
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

        // decrypt
        // AES128 ECB encrypt
        else if(!(strncmp(client_command, "decrypt", 7)&&strncmp(client_command, "de", 2)))
        {
            server_message = "File name: ";
            write(sock, server_message, strlen(server_message));

            if((read_size = recv(sock, filename, MAX_FILENAME_LEN-1, 0)) > 0)  // get filename from client
            {
                /* discard .cip ext */
                const char ext[] = ".rec";
                char tempfilename[MAX_FILENAME_LEN];
                strcpy(tempfilename, filename);
                int namelen = strlen(filename);
                strncpy(tempfilename+namelen, ext, 5);
                // DEBUG - CHECK EXT FILENAME
                // printf("%s %s\n", filename, tempfilename);  

                if(access(filename, F_OK) != -1)
                {
                    /* ask client for 16 bytes password */
                    server_message = "Password <= 16 letters: ";
                    write(sock, server_message, strlen(server_message));

                    uint8_t key[16];
                    if((read_size = recv(sock, client_message, MAX_MESSAGE_LEN-1, 0)) > 0)
                    {
                        /* discard letter if the length is more than 16 */
                        strncpy(key, client_message, 16);
                        if(strlen(client_message) > 16+1)
                        {
                            server_message = "Key too long.\nThe letters longer than 16 will be discarded.\n";
                            write(sock, server_message, strlen(server_message));
                        }

                        FILE *cfp = fopen(filename, "rb");
                        FILE *rfp = fopen(tempfilename, "wb");
                        const segment_size = 16;
                        uint8_t in[segment_size];
                        uint8_t out[segment_size];

                        /* get file size */
                        uint64_t fsize;
                        fread(&fsize, sizeof(uint64_t), 1, cfp);
                        // printf("file size = %lld\n", fsize);
                        uint64_t times = fsize/segment_size+1;
                        // printf("times = %lld\n", times);
                        int remainer = fsize-(times-1)*segment_size;
                        // printf("remainer = %d\n", remainer);
                        while(times--)
                        {
                            read_size = fread(in, sizeof(uint8_t), segment_size, cfp);
                            AES128_ECB_decrypt(in, key, out);
                            if(times)
                                fwrite(out, sizeof(uint8_t), read_size, rfp);
                            else
                                fwrite(out, sizeof(uint8_t), remainer, rfp);
                        }
                        remove(filename);
                        rename(tempfilename, filename);
                        fclose(cfp);
                        cfp = NULL;
                        fclose(rfp);   
                        rfp = NULL;
                    }
                    else
                        perror("Fail to get key");
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

        // rename
        else if(strncmp(client_command, "rename", 6) == 0)
        {
            server_message = "Old and New file name. (Seperate by a blank)\n";
            write(sock, server_message, strlen(server_message));

            if((read_size = recv(sock, client_message, MAX_MESSAGE_LEN-1, 0)) > 0)  // get filename from client
            {
                char newfilename[MAX_FILENAME_LEN];
                sscanf(client_message, "%s %s", filename, newfilename);
                //  DEBUG - CHEKC OLD AND NEW FILE NAME
                // printf("checking\n");
                // printf("%s %s\n", filename, newfilename);
                if(access(filename, F_OK) != -1)
                {
                    if(rename(filename, newfilename) == 0)
                        server_message = "Successfully\n";
                    else
                        server_message = "Fail\n";
                    write(sock, server_message, strlen(server_message));
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
		else if(!(strncmp(client_command, "quit", 4)&&strncmp(client_command, "bye", 3)))
		{
			server_message = "Bye Bye\n";
			write(sock, server_message, strlen(server_message));
			break;
		}	

        else
        {
            server_message = "No such command\n";
            write(sock, server_message, strlen(server_message));            
        }

        memset(filename, 0, sizeof(filename));  // prevent garbage character
        memset(client_command, 0, sizeof(client_command));
        memset(client_message, 0, sizeof(client_message));
    }
     
    if(read_size == 0)
    {
        printf("Client %d disconnected\n", sock);
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        // perror("recv failed");
    }

    
    close(sock);
    /* delete client list */
    delete(sock);
    client_cnt--;
    /* Free private socket_desc */
    free(socket_desc);

    return 0;
} 

void *server_cmd_handler()
{
    /* Server welcome message */
    puts("#############################################");
    puts("1. broadcast (bc)");
    puts("2. showip (si)");
    puts("3. shutdown [sec] (sd)");
    puts("#############################################");

    char message[MAX_MESSAGE_LEN], command[MAX_COMMAND_LEN];
    while(1)
    {
        fgets(command, MAX_COMMAND_LEN-1, stdin);
        message_trim(command);

        //  broadcast. If client is downloading then not receive
        if(!(strncmp(command, "bc", 2)&&strncmp(command, "broadcast", 9)))
        {
            printf("Message: \n");
            fgets(message, MAX_MESSAGE_LEN-1, stdin);
            message[strlen(message)] = '\0';

            send2All(message);            
        }
        else if(!(strncmp(command, "shutdown", 8)&&strncmp(command, "sd", 2)))
        {
            char dummy[8];
            unsigned t;
            if(sscanf(command, "%s %u", dummy, &t)==2)
            {
                sprintf(message, "Notice: Server Will shutdown after %u second\n", t);
                send2All(message);
                while(t)
                {
                    // count down
                    if(t<=10)
                    {
                        printf("%u ", t);
                        fflush(stdout);
                    }
                    t--;
                    sleep(1);
                }
                puts("");
                clean(head);
                exit(0);
            }
        }
        else if(!(strncmp(command, "showip", 6)&&strncmp(command, "si", 2)))
        {
            socklen_t len;
            struct sockaddr_storage addr;
            char ipstr[INET_ADDRSTRLEN];
            int port;

            struct sockaddr_in *s;
            struct node *temp = head;
            // BUG - MUST GET FIRST FOR GARBAGE
            if(temp!=NULL)
                getpeername(temp->desc, (struct sockaddr*)&addr, &len);
            while(temp!=NULL)
            {
                getpeername(temp->desc, (struct sockaddr*)&addr, &len);
                s = (struct sockaddr_in *)&addr;
                port = ntohs(s->sin_port);
                inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

                printf("%s:%d\n", ipstr, port);
                temp = temp->next;
            }
        }
        else
            printf("No such command\n");
    }
}

void send2All(char *message)
{
    struct node* temp = head;
    while(temp!=NULL)
    {
        if(!temp->isDownload)
            write(temp->desc, message, strlen(message));

        temp = temp->next;
    }
}

/* remove trailing newline character */
void message_trim(char* msg)
{
    int len = strlen(msg);
    msg[len-1] = '\0';
}

// static void phex(uint8_t* str)
// {
//     // original
//     unsigned char i;
//     for(i = 0; i < 16; ++i)
//         printf("%.2x ", str[i]);
//     printf("\n");
// }

void setDownloadFlag(int desc, int flag)
{
    int cnt = 0;
    struct node *temp;
    temp = head;
    if(temp == NULL)
        return;
    else
    {
        while(temp!=NULL)
        {
            if(temp->desc==desc)
            {
                temp->isDownload = flag;
                return;
            }
            else
                temp = temp->next;
        }
    }    
}
 
void append(int desc)
{
    struct node *temp,*right;
    /* create new node */
    temp = (struct node *)malloc(sizeof(struct node));
    temp->desc = desc;
    temp->isDownload = FALSE;
    /* move to the end */
    right=(struct node *)head;
    while(right->next != NULL)
        right=right->next;

    right->next = temp;
    temp->next = NULL;
}

void addBeginning(int desc)
{
    struct node *temp;
    temp = (struct node *)malloc(sizeof(struct node));
    temp->desc = desc;
    temp->isDownload = FALSE;
    if(head == NULL)
    {
        head = temp;
        head->next = NULL;
    }
    else
    {
        temp->next = head;
        head = temp;
    }
}


void addAfter(int desc, int loc)
{
    int i;
    struct node *temp,*left,*right;
    right=head;
    for(i=1;i<loc;i++)
    {
        left=right;
        right=right->next;
    }
    temp=(struct node *)malloc(sizeof(struct node));
    temp->desc=desc;
    temp->isDownload = FALSE;
    left->next=temp;
    left=temp;
    left->next=right;
    return;
}
 
 
 
void insert(int desc)
{
    int cnt = 0;
    struct node *temp;
    temp = head;
    if(temp == NULL)
        addBeginning(desc);
    else
    {
        while(temp!=NULL)
        {
            if(temp->desc<desc)
                cnt++;
            temp=temp->next;
        }
        if(cnt==0)
            addBeginning(desc);
        else if(cnt<client_cnt)
            addAfter(desc,++cnt);
        else
            append(desc);
    }

    client_cnt++;
}
 
// return whether the element exist
int delete(int desc)
{
    struct node *temp, *prev;
    temp = head;
    while(temp!=NULL)
    {
        if(temp->desc==desc)
        {
            if(temp==head)
            {
                head=temp->next;
                free(temp);
                client_cnt--;
                return 1;
            }
            else
            {
                prev->next=temp->next;
                free(temp);
                client_cnt--;
                return 1;
            }
        }
        else
        {
            prev=temp;
            temp= temp->next;
        }
    }
    return 0;
}

void clean(struct node* cur)
{
    if(cur==NULL)
    {
        client_cnt = 0;
        head = NULL;
        return;
    }
    clean(cur->next);
    close(cur->desc);
    free(cur);
}

void intHandler(int sig)
{
    clean(head);
    exit(0);
}