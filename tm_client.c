//////////////////////////////////////////////////////////////////////////////////////
//  PROGRAM :	tm_client.c
//  AUTHOR  :	Stephen Beko
//	VERSION	:	3
//  DATE    :	24/10/2012
/////////////////////////////////////////////////////////////////////////////////////
/*
 This TCP/IP client connects and sends messages (or commands) to a TCP/IP Server
 BUILD:
       gcc -std=c99 tm_client.c -o client
   Arguments:
       IP Address (or hostname) is mandatory.
   	   Port number is mandatory - must be same as that bound by(tcp) connection server
  	   Format:
   	   ./client 127.0.0.1 4549  <or> ./client <HOSTNAME> <Port>

   MODES (set by #define COMMAND)
     disabled (default. Send same message every 5 seconds
     enabled = Send string set up.

   in command mode - quit    = exit program (otherwise CTRL-C
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

// Enable to send SINGLE MESSAGE (disable to repeat every 5sec)
#define COMMAND

static int cnt;

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    char read_buffer[256];

    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
 	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
    	    		error("ERROR connecting");
#ifndef COMMAND
    	printf("Please enter the message to repeat: ");
    	fgets(read_buffer,255,stdin);
#endif

 	while (1)
    {
#ifdef COMMAND
    	printf("Please enter the message (quit to exit): ");
    	bzero(buffer,256);
    	fgets(buffer,255,stdin);
    	if (strncmp(buffer,"quit", 4) == 0)
    	{
    		break;
    	}
#else
    	printf("NEXT Message ");
    	strcpy(buffer, read_buffer);		//spb
#endif
    	n = write(sockfd,buffer,strlen(buffer));
    	if (n < 0)
    		error("ERROR writing to socket");

// use this to read a response back on the socket
// if server doesn't send one you will wait here
// so advisable to disable this section

/*    	bzero(buffer,256);
    	n = read(sockfd,buffer,255);
    	if (n < 0)
    		{
    		error("ERROR reading from socket");
    		break;
    		}
*/

#ifdef COMMAND
    	sleep (3);
#else
    	cnt++;
    	printf("%s wrote %d count=%d\n",buffer, n,cnt);
    	sleep (5);
#endif
    }
    printf("EXIT program\n");
    close(sockfd);
    return 0;
}
