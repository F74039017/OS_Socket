Online Editor
===================

Operating System Homework 1<br>
Simple command line utility. Provide server and client program. User can connect to the server by socket and type some command such as create, list, remove... etc.<br><br>

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
Just execute it.

*Client* :
Execute by passing two parameter.<br>
`./client [server_ip] [port]`<br>
There are default ip = localhost and port = 8888.<br>

Support Command
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

**Notice** :<br>

alias command has two parameter.<br>
`alias [command] [alias]`<br>
Other commands has no parameter.



