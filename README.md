# multiconnectTCP
Multi Connection TCP/IP Server(with openSSL) and TCP/IP Client

A multiple connect TCP/ IP Server.
   The socket Listner backlog supports up to 64 TCP/IP connections. Each client connection 
   can send simple text messages (echoed on server to console) and ACK return.
   BUILD:
       gcc -std=c99 tm_skt.c -o sc
   Arguments:
   	   Port number is mandatory.
   	   IP Address is optional and can be set to the assigned ethernet IP address.
   	   If not provided, it defaults to the loopback address (hostname).
   	   Format:
   	   ./sc 4549  <or> ./sc 4549 192.168.0.4
   This program also uses flags to support:
   ENABLE_SSL    - SSL (openSSL) connections, not working yet. Work in progress.
                 - OFF by default.
   MULTI_CONNECT - Enable unless you want simple single connection (without use of select)
                 - ON by default.  
   ENABLE_CLI    - Link to function to decode commands. Requires another file.
                 - OFF by default.

Also TCP/IP Client.
gcc -std=c99 tm_client.c -o client
./client 127.0.0.1 4549  <or> ./client <HOSTNAME> <Port>
