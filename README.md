# multiconnectTCP
Steve's Multi Connection TCP/IP Server(with openSSL) and TCP/IP Client

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
./client 127.0.0.1 4549  <or> ./client <HOSTNAME> <Port>.

To Install openSSL under Ubuntu and BUILD the server using eclipse
http://www.openssl.org
https://help.ubuntu.com/community/OpenSSL
$sudo apt-get install openssl
$apt-cache search libssl | grep SSL  #to see version to install, in this case 0.9.8
$apt-get install libssl0.9.8
$apt-get install libssl-dev
  CONFIRM following sym Links in /usr/lib/ssl
          openssl.conf -> /etc/ssl/openssl.cnf
          certs -> /etc/ssl/certs
          private -> /etc/ssl/private
  CONFIRM /usr/include/openssl/<headers and importantly ssl.h>.
  SET UP ECLIPSE:
  In Eclipse IDE select Your Project property --> c/c++ Build --> 
		Settings gcc c linker(from tools settings)--> add to Library Search Path (-L)
		Libraries -> add Path(-L) /usr/lib /usr/lib/i386-linux-gnu
                -> add libs -> ssl and crypto
Now Build it.
