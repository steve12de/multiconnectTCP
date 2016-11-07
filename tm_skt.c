//////////////////////////////////////////////////////////////////////////////////////
//  PROGRAM :	tm_skt.c
//  AUTHOR  :	Stephen Beko
//	VERSION	:	3
//  DATE    :	24/10/2012
//////////////////////////////////////////////////////////////////////////////////////
/*
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
*/
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>    //for bzero
#include <string.h>
#include <unistd.h>     // for close
#include <sys/types.h>
#include <fcntl.h>
#include <sys/time.h> 
#include <sys/socket.h>	//not really needed because added through incs below
#include <netinet/in.h>
#include <arpa/inet.h>  //for inet_addr - pulls in other includes
#include <errno.h>
#include <stdbool.h>    // for bool

// IF YOU WANT SSL CONNECTIONS
// #define ENABLE_SSL		1
// IF YOU WANT MULTI CONNECT using Select, otherwise single CONNECT (not available with SSL)
#define MULTI_CONNECT		1
// IF YOU WANT CLI PROCESSING OF MESSAGE
//#define ENABLE_CLI

#ifdef ENABLE_CLI
#include "tm_process_cmd.h"
#endif

#ifdef ENABLE_SSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#endif

#define TRUE 1
#define FALSE 0

// Function Prototypes
void error(const char *msg);

///////////////////////
// Data Declarations //
///////////////////////
const int backlog = 64;

// Added for SSL
#ifdef ENABLE_SSL
#define MAX_SOCKETS	128
// Define SSL socket structure.
// We use an internal list to keep track, for the user just the fd is used.
// This is only needed for secure data, the plain ones have no extra data.
struct socket {
	int used;	// A seperate flag as sock can be 0
	int sock;	// The sockfd
	BIO *bio;	// Ref for the BIO system (SSL uses it)
	SSL *ssl;	// SSL layer data
};
static struct socket sockets[MAX_SOCKETS];
#endif

////////////////////////////////////
//////////// FUNCTIONS  ////////////
////////////////////////////////////
void error(const char *msg)
{
    perror(msg);
    //exit(1);
}

///////////////////////////////////
// Added for SSL
///////////////////////////////////
#ifdef ENABLE_SSL
/*
 * *slist_find()
 * Find the SSL structure for a socket
 */
static struct socket *slist_find(int sock)
{
	// Search
	if (sock >= 0)
	{
		int i;
		for (i=0; i<MAX_SOCKETS; i++)
		{
			if (sockets[i].used && (sockets[i].sock == sock))
				return &(sockets[i]);
		}
	}
	return NULL;
}
/*
 * *slist_new()
 * Find a new SSL structure
 */
static struct socket *slist_new(int sock)
{
	if (sock >= 0)
	{
		int i;
		for (i=0; i<MAX_SOCKETS; i++)
		{
			if (!sockets[i].used)
			{
				// Fill out and return
				struct socket *sl = &(sockets[i]);
				memset(sl, 0, sizeof(struct socket)); // Clear
				sl->used = TRUE;
				sl->sock = sock;
				return sl;
			}
		}
	}
	return NULL;
}
/*
 * slist_free()
 * Free SSL structure
 */
static void slist_free(int sock)
{
	struct socket *sl = slist_find(sock);
	if (sl) sl->used = FALSE;
}

/*
 * ssl_initialise()
 */
void ssl_initialise()
{
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
}
#endif // ENABLE_SSL

/*
 *	socket_listen()
 *	Open socket to listen for connections
 */
bool socket_listen(int sockfd)
{
	bool ret = FALSE;
	// The backlog argument defines the maximum length to which the queue of pending connections for sockfd may grow.
	if (listen(sockfd, backlog) != -1)
    {
		ret =  TRUE;
    }
    else
    {
		error("\nListener error:");
		close(sockfd);
    }
	return ret;
}

/*
 *	socket_block()
 *	Set blocking option.TRUE = block, FALSE = no block.
 *	Non block for select().
 */
void socket_block(int sock, int block)
{
	if (sock >= 0)
	{
		// Non blocking so select() can work it
		fcntl(sock, F_SETFL, (block) ? 0 : O_NONBLOCK);
	}
}

/*
 *  socket_accept()
 *  Accept a new socket connection to a listener
 *  Supports secure connections.
 */
#ifdef ENABLE_SSL
int socket_accept(int sock, SSL_CTX *ctx)
#else
int socket_accept(int sock)
#endif
{
	struct sockaddr addr;
	socklen_t len = sizeof(addr);
	sock = accept(sock, (struct sockaddr *)&addr, &len);

	if (sock >= 0)
	{
		// Non blocking so select() can work it
		socket_block(sock, FALSE);
#ifdef ENABLE_SSL
		// Add the security stuff?
		if (ctx)
		{
			int ok = FALSE;
			struct socket *sl = slist_new(sock);
			sl->bio = BIO_new_socket(sock, 0); // Let the BIO close the socket when it's done
			if (sl->bio)
			{
				sl->ssl = SSL_new(ctx);
				if (sl->ssl)
				{
					SSL_set_bio(sl->ssl, sl->bio, sl->bio);
					if (SSL_accept(sl->ssl) > 0)
					{
						// Set the SSL mode
						SSL_set_mode(sl->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
						ok = TRUE;
					}
				}
			}
			if (!ok) // Error?
			{
				if (sl->ssl) SSL_free(sl->ssl); // This frees the bio etc too
				slist_free(sock);
				close(sock);
				sock = -1;
			}
		}
#endif
	}
	return sock;
}

/*
 * read_sock()
 * Read text on connection from client 
 * Pass command to CLI (if CLI enabled). 
 */
bool read_sock(int sockfd, void *buf, bool block)
{
    int n = 0;
    const int maxsize = 255;
    bzero(buf, 256);
    socket_block(sockfd, block);
   	
	if (sockfd >= 0)
	{
#ifndef ENABLE_SSL
		n = (int) recv(sockfd, buf, maxsize, 0);
#else
		struct socket *sl = slist_find(sockfd);
   		if (sl)
   		{
   			int code = SSL_read(sl->ssl, buf, maxsize);
   			if (code > 0)
   				n = code;
   		}
#endif

   		if ((n < 0) && block ) // BLOCKING set so genuine error, otherwise didn't read anything
   		{
   			error("tm_skt: ERROR reading from socket");
   			return FALSE;
   		}
   		if ((n == 0) && (block))
   		{
   			printf("tm_skt: ERROR, read on invalid socket\n");
   			return FALSE;
   		}
   		else
   		{
   			printf("tm_skt: Nothing to Read\n");
   		}
   		printf("tm_skt: Message from fd%d len=%d:%s\n", sockfd, n, (char*)buf);
#ifdef ENABLE_CLI
   		read_command(buf);
#endif
	}
	else
	{
		printf("tm_skt: ERROR, invalid socket\n");
	}
	return TRUE;
}

/*
 * write_sock()
 * Write response to client 
 */
void write_sock(int sockfd, void *buf)
{
    unsigned int len;
    int sent = 0;
	printf("tm_skt Write to fd%d\n",sockfd);
    len = strlen(buf);
    if (sockfd >= 0)
    {
#ifndef SSL
    	sent = (int) send(sockfd, buf, len, 0);
#else
    	struct socket *sl = slist_find(sockfd);
    	if (sl)
    	{
    		int code = SSL_write(sl->ssl, buf, len);
    		if (code > 0)
    			sent = len; // TBD
    	}
#endif
    }
    if (sent  < 0)
       error("tm_skt: ERROR writing to socket");
}

/*
 * socket_close()
 * Closes and frees the socket
*/
void socket_close(int sock)
{
	if (sock >= 0)
	{
#ifdef ENABLE_SSL
			struct socket *sl = slist_find(sock);
			if (sl->ssl) // Close down SSL (also does BIO)
			{
			 	if (SSL_get_shutdown(sl->ssl) & SSL_RECEIVED_SHUTDOWN)
		 		{
					// Need to call this a second time if the RC != 0
					if (!SSL_shutdown(sl->ssl))
						SSL_shutdown(sl->ssl);
		 		}
		 		else
					SSL_clear(sl->ssl);
				SSL_free(sl->ssl);
			}
			slist_free(sock); // Free up secure slot
#endif
		close(sock);
	}
}

/*
 * fdset_initialise()
 */
void fdset_initialise(fd_set *set, struct timeval *tv)
{
   /* clear the sets */
   FD_ZERO(set);
   /* Wait up to five seconds. */
   tv->tv_sec = 5;
   tv->tv_usec = 0;
}

/*
 * main()
 * Initialisation and loop to handle connections
 */
int main(int argc, char *argv[])
{
	 struct sockaddr_in serv_addr;
	 int sockfd; 		// listen socket
     int newsockfd; 	// accept this and other connections
     int portno;		// port listening on
     char buffer[256];
     static int connectionCount = 0;
     int rc;
#ifdef MULTI_CONNECT
     bool use_of_select = TRUE;
#else
     bool use_of_select = FALSE;
#undef ENABLE_SSL
#endif
     bool set_ok;

#ifdef ENABLE_SSL
     ssl_initialise();
#else
     bool accepted = FALSE;  // used in single connections
#endif
     
     const char *address = NULL;
     int i;
     if (argc < 2) 
     {
    	 error("tm_skt: ERROR, NO PORT provided\n");
         exit(1);
     }
     // do we have an address
     if (argc >= 3)
     {
    	 address = argv[2];
    	 int len = strlen(address);
    	 if ((len < 8) || (len > 16))
    	 {
    	     printf("tm_skt: ERROR Invalid address - Bind to addr 0\n");
    		 address = NULL;
    	 }
    	 else
      	      printf("tm_skt: address=%s...len=%d\n",argv[2], len);
     }
     //////////////////////////////////////////////////////
     // select() for incoming connections to accept() on //
     //////////////////////////////////////////////////////
     // In order to be notified of incoming connections on a socket, you can use
     // select(2) or poll(2). A readable event will be delivered when a new connection is
     // attempted and you may then call accept to get a socket for that connection.

     // master file descriptor list
     fd_set master;
     // temp file descriptor list for select()
     fd_set read_fds;
     struct timeval tv;
     int fd_max;
     ////////////////////////////////////////////
     // create, bind, listen and on tcp socket //
     ////////////////////////////////////////////
 	// Open a socket
 	sockfd = socket(AF_INET, SOCK_STREAM, 0);  //PF_INET
 	if (sockfd != -1)
 	{
 		// Bind it
	    bzero((char *) &serv_addr, sizeof(serv_addr));
	    portno = atoi(argv[1]);
	    serv_addr.sin_family = AF_INET;
 		serv_addr.sin_addr.s_addr = (address) ? inet_addr(address) : INADDR_ANY;
 		serv_addr.sin_port = htons(portno);

  		if (bind(sockfd, (struct sockaddr *) &serv_addr,
 				 sizeof(serv_addr)) < 0)
 		{
 	        error("tm_skt: ERROR binding socket");
 	        exit(1);
 		}
 		if (!socket_listen(sockfd))
		{
 	        error("ERROR on Listen");
 	        exit(1);
		}
 	    printf ("Network Server running on Listner: fd%d\n",sockfd);
 	}
 	else
 	{
        error("tm_skt: ERROR opening socket");
	    exit(1);
 	}
     // initialise set
     fdset_initialise(&read_fds, &tv);
     FD_SET(sockfd, &read_fds);
     fd_max = sockfd;

     //////////////////
     ////// loop //////
     //////////////////
     while(1)
     {
    	 if (use_of_select)
    	 {
    		 // Accept multiple connections (SSL or non SSL)
    		 set_ok = TRUE;
    		 read_fds = master;	// IMPORTANT set to add connections
    		 FD_SET(sockfd, &read_fds);	 // IMPORTANT to add listner to read set
    		 // params - int nfds, fd_set *readfds, fd_set *writefds,
    		 //		   - fd_set *exceptfds, struct timeval *timeout);
    		 socket_block(sockfd, FALSE);
    		 rc = select(fd_max + 1, &read_fds, NULL, NULL, &tv);
    		 if (rc == -1)
    		 {
    			 error("tm_skt: Error Server-select() failed");
    			 exit(1);
    		 }
			 else if (rc == 0)
			 {
				 // constantly through here until new connection
				 //printf("tm_skt: Server-select timeout\n");
				 set_ok = FALSE;
			 }
			 else
			 {
				 printf("tm_skt: Select OK on Listener\n");
    		     set_ok = TRUE;
    	     }

    		 ////////////////////////////////////////////////////////////////
    		 // run through the existing connections looking for data to read
    		 ////////////////////////////////////////////////////////////////
    		 if (set_ok)
    		 {
    		 for(i = 0; i <= fd_max; i++)
    		 {
    			 // Check if data is available to read
    			 if(FD_ISSET(i, &read_fds))
    			 {
    				 FD_CLR(i, &read_fds);
    				 // every time we get a new connection, we have to add it to the master set and also every time 
    				 // a connection closes, we have to remove it from the master set.	 
    				 if(i == sockfd)  // Listner   
    				 {
    					 socket_block(sockfd, TRUE);  // if not present - causes error: inv arg
#ifdef ENABLE_SSL
    					 if((newsockfd = (socket_accept(sockfd, NULL))) == -1)
#else
       					 if((newsockfd = (socket_accept(sockfd))) < 0 )
#endif
       					 {
    						 error("tm_skt: ERROR: accept");
       					 }
    					 else
    					 {
    						 printf("tm_skt: Listner-accept new connection:fd%d\n",newsockfd);
    						 //printf("New connection from %s on socket %d\n", inet_ntoa(clientaddr.sin_addr), newsockfd);
    						 FD_SET(newsockfd, &master); /* add to master set */
    						 connectionCount++;
    						 if(newsockfd > fd_max)
    						 { /* keep track of the maximum */
    							 fd_max = newsockfd;
    						 }
    					 }
    					 printf("tm_skt: Read on New Skt Connection, fd%d\n",newsockfd);
    					 sleep(1);
    					 // Read data set up with connection - TRUE for first connection
    					 if (!read_sock(newsockfd, buffer, FALSE))
    					 {
    					 	 printf("tm_skt: Lost Connection, fd%d\n", newsockfd);
    					 }
    					 else
    					 {
                             bzero(buffer,256);
	  					     strcpy(buffer,"Connect OK\n");
    					     write_sock(newsockfd, buffer);
    					 }
    				 }
    				 else // other non Listener fds
    				 {
    					printf("tm_skt: Read on Existing Skt Connection, fd%d\n",i);
    					// read data from a client over an existing socket connection
						if (!read_sock(i, buffer, TRUE))
						{
							printf("tm_skt: Lost Existing Connection on fd%d\n", i);
							FD_CLR(i, &master);
							connectionCount--;
							printf("tm_skt: Connections=%d\n",connectionCount);
						}
						else
						{
						    bzero(buffer,256);
						    strcpy(buffer,"ACK\n");
						    write_sock(i, buffer);
						}
    				 }
    			 }	// if ISSET
    		 } // for
    		 }  // set_ok
    	 }	// if use_of_select
    	 else   // SINGLE CONNECTION MODE
    	 {
#ifndef ENABLE_SSL
    		 // Accept a simple single connection (non SSL connection)
    		 if (!accepted)
    		 {
    			 socket_block(sockfd, TRUE);
    			 newsockfd = socket_accept(sockfd);
    			 if (newsockfd < 0) 
    			 {
    				 error("tm_skt:ERROR on accept");
    				exit(1);
    			 }
    			 accepted = TRUE;
    		 }
    		 read_sock(newsockfd, buffer, TRUE);
    		 // write_sock(newsockfd, buffer);
    		 break;
#else
    		error("tm_skt:Single Connection mode disabled for SSL");
        	exit(1);
#endif
    	 }
     }	//while
     /////////////////////////////
     //////// end program ////////
     /////////////////////////////
     if (use_of_select)
     {
    	 for(i = 0; i <= fd_max; i++)
    	 {
    		 //if ()
    		 socket_close(i);
    	 }
     }
     else
     {
    	 socket_close(newsockfd);
         socket_close(sockfd);
     }
     return 0; 
}

/////////////////////////////////////////////////////////////////////////////////
// useful conversions														   //
// struct sockaddr_in network;												   //
// char *address							  	// eg "10.0.0.1"		   //
// inet_aton(address, &network.sin_addr); 		// store IP in network		   //
// nework.sin_addr.s_addr = inet_addr(address); // as above					   //
// address = inet_ntoa(network.sin_addr); 		// return the IP in ascii	   //
//////////////////////////////////////////////////////////////////////////////////
