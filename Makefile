all : server client

aes.o : aes.h aes.c
	gcc -c aes.c -o aes.o

server : server.c aes.o
	gcc server.c aes.o -pthread -o server

client : client.c
	gcc client.c -pthread -o client

clean :
	rm server client *.o
