#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> // socket
#include <arpa/inet.h> //inet_addr
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#define TRUE 1
#define FALSE 0

// Fail code
#define RECV_PTHREAD_FAIL 1
#define SEND_PTHREAD_FAIL 2
#define DOWN_PTHREAD_FAIL 3

#define MAX_FILENAME_LEN 100
#define MAX_COMMAND_LEN 50
#define MAX_MESSAGE_LEN 4096

/* thansfer flags */
#define TRANSFER_WAIT 0
#define TRANSFER_OK 1
#define TRANSFER_DISCARD 2

//  MODE
#define MESSAGE_MODE 1
#define DOWNLOAD_MODE 2
#define PUT_MODE 3

// alias
#define COMMAND_NUM 11
#define MAX_ALIAS 200
struct _alias
{
    char cmd[MAX_COMMAND_LEN];
    char ali[MAX_COMMAND_LEN];
}alias_list[MAX_ALIAS];

int alias_cnt;
// enum {CREATE, EDIT, CAT, REMOVE, LIST, DOWNLOAD, ENCRYPT, DECRYPT, RENAME, QUIT};
const char alias_file_name[] = ".alias";
const char *command_list[COMMAND_NUM] = {"create", "edit", "cat", "remove", "list", "download", "encrypt", "decrypt", "rename", "quit", "put"};


// sync wthread and rthread
pthread_mutex_t wr_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  fs_cond   = PTHREAD_COND_INITIALIZER;
pthread_cond_t  ow_cond   = PTHREAD_COND_INITIALIZER;
pthread_cond_t  cf_cond   = PTHREAD_COND_INITIALIZER;
pthread_cond_t  rd_cond   = PTHREAD_COND_INITIALIZER;

// download variable
int mode;
char savedName[MAX_FILENAME_LEN];
int trans_state;
long long filesize;
FILE *fp;

void* receive_handler(void* );
void* write_handler(void* );
void initAlias();
void addAlias(const char*, const char*);
void showAlias();
void convertAlias(char* );
void message_trim(char* );

int main(int argc , char *argv[])
{
    int socket_desc;
    char* ip;
    int port;
    struct sockaddr_in server;

    initAlias();
    // showAlias();

    if(argc == 1)
    {
        puts("Use default ip: localhost, port: 8888");
        ip = "127.0.0.1";
        port = 8888;
    }
    else
    {
        if(!strcmp(argv[1], "localhost"))
            ip = "127.0.0.1";
        else
            ip = argv[1];

        if(argc==2)
        {
            puts("Use default port: 8888");
            port = 8888;
        }
        else
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

void initAlias()
{   
    memset(alias_list, 0, sizeof(alias_list));
    alias_cnt = 0;

    FILE* afp = fopen(alias_file_name, "r+");
    if(afp)
    {   
        char temp[MAX_COMMAND_LEN*2+1];
        char cmd[MAX_COMMAND_LEN], ali[MAX_COMMAND_LEN];
        while(fgets(temp, MAX_COMMAND_LEN*2+1, afp))
        {
            sscanf(temp, "%s %s", cmd, ali);

            int i;
            for(i=0; i<COMMAND_NUM; i++)
            {
                if(strcmp(command_list[i], cmd) == 0)
                {
                    strcpy(alias_list[alias_cnt].cmd, cmd);
                    strcpy(alias_list[alias_cnt].ali, ali);
                    alias_cnt++;
                }
            }
        }
        fclose(afp);
        afp = NULL;
    }
    else
    {
        afp = fopen(alias_file_name, "w");
        fclose(afp);
        afp = NULL;   
    }
}

void addAlias(const char* cmd, const char* ali)
{
    /* check command existence */
    int flag = FALSE;
    int i;
    for(i=0; i<COMMAND_NUM; i++)
        if(strcmp(command_list[i], cmd) == 0)
            flag = TRUE;

    /* add alias */
    if(!flag)
        printf("Can't find command: \"%s\"\n", cmd);
    else
    {
        FILE* afp = fopen(alias_file_name, "a");
        fprintf(afp, "%s %s\n", cmd, ali);
        fclose(afp);
        afp = NULL;

        // printf("add alias %s = %s\n", ali, cmd);

        strcpy(alias_list[alias_cnt].cmd, cmd);
        strcpy(alias_list[alias_cnt].ali, ali);
        alias_cnt++;
    }
}

void showAlias()
{
    int i;
    for(i=0; i<alias_cnt; i++)
        printf("%s = %s\n", alias_list[i].ali, alias_list[i].cmd);
}

void convertAlias(char *cmd)
{
    int i;
    for(i=0; i<alias_cnt; i++)
    {
        if(strcmp(alias_list[i].ali, cmd) == 0)
        {
            strcpy(cmd, alias_list[i].cmd);
            return;
        }
    }
}

/* remove trailing newline character */
void message_trim(char* msg)
{
    int len = strlen(msg);
    msg[len-1] = '\0';
}

void* receive_handler(void* socket_desc)
{
    int sock = *(int*)socket_desc;
    int read_size;
    char message[MAX_MESSAGE_LEN];

    /* Get server message and output */
    while((read_size = recv(sock, message, MAX_MESSAGE_LEN-1, 0)) > 0)
    {
        /**
        DOWNLOAD_MODE is invoked in wthread when output command 'd'.
        MESSAGE_MODE will switch back automatically when download has been done.
        PUT_MODE will get exist and ready flags
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
            /* strat sync with wthread */
            pthread_mutex_lock(&wr_mutex);

            /* Get file's size */
            if((read_size = recv(sock, message, MAX_MESSAGE_LEN-1, 0))>0)
            {
                message[read_size] = '\0';
                if(strcmp(message, "FILE_NOT_EXIST") == 0)
                {
                    puts("The file doesn't exist");
                    trans_state = TRANSFER_DISCARD;
                }
                else
                    filesize = atoll(message);
                // printf("file size = %lld\n", filesize);
            }
            else
            {
                perror("Get file's size");
                trans_state = TRANSFER_DISCARD;   
            }
            pthread_cond_signal(&fs_cond);

            /* wait for setting overwrite flag */
            pthread_cond_wait(&ow_cond, &wr_mutex);
            if(trans_state == TRANSFER_OK)
            {
                long long receive_cnt = 0;
                /* start to receive data */
                uint8_t segment[MAX_MESSAGE_LEN];
                while(receive_cnt < filesize)
                {
                    read_size= recv(sock, segment, MAX_MESSAGE_LEN-1, 0);
                    receive_cnt += read_size;
                    // DEBUG - CHECK RECEIVE SIZE
                    // printf("receive size/ filesize = %lld/ %lld\n", receive_cnt, filesize);
                    // segment[read_size] = '\0';
                    // DEBUG - OUTPUT DATA TO STDIN
                    // printf("%s", message);
                    fwrite(segment, sizeof(uint8_t), read_size, fp);
                    fflush(fp);
                }                  
                puts("Download Finished!");
            }

            /* sync over */
            pthread_mutex_unlock(&wr_mutex);

            /* transfer done. resume message mode */
            if(fp)
                fclose(fp);
            fp = NULL;

            /* resume to message mode */
            mode = MESSAGE_MODE;
        }
        else if(mode == PUT_MODE)
        {
            /* strat sync with wthread */
            pthread_mutex_lock(&wr_mutex);

             // Get exist flag
            message[strlen(message)] = '\0';
            if(!strncmp(message, "TRANSFER_WAIT", 13))
                trans_state = TRANSFER_WAIT;    // Client confirm, send flag and wait ready
            else
                trans_state = TRANSFER_OK;      // transfer directly

            pthread_cond_signal(&ow_cond);

            if(trans_state == TRANSFER_WAIT)
            {
                // wait user confirm. Without this, the program will stuck in recv().
                pthread_cond_wait(&cf_cond, &wr_mutex); 
                recv(sock, message, MAX_MESSAGE_LEN-1, 0);  // wait READY flag
                pthread_cond_signal(&rd_cond);
            }

            /* sync over */
            pthread_mutex_unlock(&wr_mutex);
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
        if(message[0] == '\n')
            continue;
        message_trim(message);
        /* local command */
        if(!strncmp(message, "alias", 5))
        {
            char cmd[MAX_COMMAND_LEN], ali[MAX_COMMAND_LEN];
            if(sscanf(message, "alias %s %s", cmd, ali) == 2)
                addAlias(cmd, ali);
            else
                printf("Format Error: alias <command> <alias>\n");
            continue;
        }

        /* remote command */
        if(mode == MESSAGE_MODE)
        {
            convertAlias(message);
            write_size = write(sock, message, strlen(message));
        }
        if(!(strncmp(message, "download", 8)&&strncmp(message, "dl", 2)))
        {
            /* init trans_state */
            trans_state = TRANSFER_OK;

            /* change to download mode => prevent from invoking other commands */
            mode = DOWNLOAD_MODE;

            /* start sync with rthread */
            pthread_mutex_lock(&wr_mutex);

            /* write filename to server */
            fgets(savedName, MAX_FILENAME_LEN, stdin);
            message_trim(savedName);
            write_size = write(sock, savedName, strlen(savedName));
            /* wait rthread for getting file's size */
            pthread_cond_wait(&fs_cond, &wr_mutex);

            if(trans_state == TRANSFER_OK)
            {
                /* check existance and ask overwrite */
                if(access(savedName, F_OK) != -1)
                {
                    int overwrite = FALSE;

                    /* check overwrite */
                    printf("The file has already existed, do you want to overwrite?[y/n] ");
                    char check;
                    do
                    {
                        scanf("%c", &check);
                        if(check == 'y')
                            overwrite = TRUE;
                    }
                    while(check!='n'&&check!='y');

                    /* send transfer flag to server */
                    if(overwrite)
                    {
                        fp = fopen(savedName, "w");
                        trans_state = TRANSFER_OK;
                        write(sock, "TRANSFER_OK", 11);
                    }
                    else
                    {
                        puts("Download Cancel!");
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
                pthread_cond_signal(&ow_cond);
            }
            // else
            //     printf("DISCARD\n");    // DEBUG - CHECK NOT FILE CONDITION
            pthread_cond_signal(&ow_cond);
 
            /* sync over */
            pthread_mutex_unlock(&wr_mutex);
        }
        else if(!strncmp(message, "put", 3))
        {
            /* change to download mode => prevent from invoking other commands */
            mode = PUT_MODE;

            /* strat sync with wthread */
            pthread_mutex_lock(&wr_mutex);

            printf("File name: \n");
            char filename[MAX_FILENAME_LEN];
            fgets(filename, MAX_FILENAME_LEN-1, stdin);
            message_trim(filename);
            fp = fopen(filename, "r");
            if(fp)
            {
                /* send client file exist flag */
                /* Concat filename and file size then send */
                struct stat st;
                int result = stat(filename, &st);
                long long fsize = st.st_size;
                memset(message, 0, sizeof(message));
                strcpy(message, "FILE_EXIST/");
                strcat(message, filename);
                sprintf(message, "%s/%lld", message, fsize);
                write(sock, message, strlen(message));
                // DEBUG - CHECK INFO MESSAGE
                // printf("%s\n", message);

                /* wait for receiving exist flag */
                pthread_cond_wait(&ow_cond, &wr_mutex);

                // File already exist
                if(trans_state == TRANSFER_WAIT)
                {
                    int overwrite = FALSE;
                    /* check overwrite */
                    printf("The file has already existed, do you want to overwrite?[y/n] ");
                    fflush(stdout);
                    char check;
                    do
                    {
                        scanf("%c", &check);
                        if(check == 'y')
                            overwrite = TRUE;
                    }
                    while(check!='n'&&check!='y');

                    // send overwrite flag
                    if(overwrite)
                    {
                        trans_state = TRANSFER_OK;
                        write(sock, "TRANSFER_OK", 11);
                    }
                    else
                    {
                        trans_state = TRANSFER_DISCARD;
                        write(sock, "TRANSFER_DISCARD", 16);
                        puts("Transfer Cancel!");
                    }

                    pthread_cond_signal(&cf_cond);
                    //  wait READY flag => prevent content being polluted
                    pthread_cond_wait(&rd_cond, &wr_mutex);
                }
                
                // Start transfer
                if(trans_state == TRANSFER_OK)
                {
                    int read_size;
                    while(read_size = fread(message, sizeof(uint8_t), MAX_MESSAGE_LEN-1, fp))
                    {
                        write(sock, message, read_size);  // tranfer data to client until EOF
                    }
                    puts("Transfer Findished!");
                }


                fclose(fp);
                fp = NULL;
            }
            else
            {
                printf("File doesn't exist\n");
                write(sock, "FILE_NOT_EXIST", 14);
            }

            /* strat sync with wthread */
            pthread_mutex_unlock(&wr_mutex);

            /* resume to MESSAGE_MODE */
            mode = MESSAGE_MODE;
        }
    }


    // Free the socket pointer
    free(socket_desc);
}