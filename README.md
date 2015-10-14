Online Editor
===================

Operating System Homework 1<br>
Simple command line utility. Provide server and client program. Client can connect to the server by socket and type some commands such as create, list, remove... etc. And the server can broadcast, show clients ip and shutdown commands.<br><br>

The program uses project, tiny-AES128-C, as encrypt core.<br>
Thanks for [kokke/tiny-AES128-C](https://github.com/kokke/tiny-AES128-C)


Environment
-------------
- OS: Ubuntu 12.04.3 LTS
- Compiler: gcc version 4.6.3

Compile
------------
- Use `make`command.
- execute **demo.sh** for demo.
(Create server and client in ser/ and cli/ folder)

Usage
-----------
*Server* :
Execute by passing one parameter.<br>
`./server [port]`<br>
There are default port = 8888.<br>

*Client* :
Execute by passing two parameters.<br>
`./client [server_ip] [port]`<br>
There are default ip = localhost and port = 8888.<br>

Server Command
-------------
1. broadcast (bc)
2. showip (si)
3. shutdown (sd)

**Notice** :<br>
If the client is transfering data including `put` and `download`, server won't send the message.<br>

Client Command
------------

1. create
2. edit
3. cat
4. remove (rm)
5. list (ls)
6. download (dl)
7. encrypt (en)
8. decrypt (de)
9. rename
10. quit (bye)
11. alias
12. put

**Notice** :<br>

alias command has two parameter.<br>
`alias [command] [alias]`<br>
The alias is record in the file named ".alias"<br>
You can modify it directly with pair [command] [alias].<br>
Other commands has no parameter.
