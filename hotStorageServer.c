#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <netinet/in.h>
#include <time.h> 

#define PACKET_LENGTH 512
#define LEN 64

int coldServer;
struct hostent *hp;
int connectSoc,port;
char buf[512];

void connectToColdServer(char *hostname,int port)
{
  char host[LEN];
  struct sockaddr_in sin;
  int rc;

  /* fill in hostent struct */
  hp = gethostbyname(hostname); 
  if ( hp == NULL ) {
    fprintf(stderr, "%s: host not found (%s)\n", "connectToColdServer", hostname);
    exit(1);
  }

  /* use address family INET and STREAMing sockets (TCP) */
  connectSoc = socket(AF_INET, SOCK_STREAM, 0);
  if ( connectSoc < 0 ) {
    perror("socket:");
    exit(connectSoc);
  }

  /* set up the address and port */
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
  /* connect to socket at above addr and port */
  rc = connect(connectSoc, (struct sockaddr *)&sin, sizeof(sin));
  if ( rc < 0 ) {
    perror("connect_:");
    exit(rc);
  }
  // Receive ack for the request send
  int len1 = recv(connectSoc, buf, 32, 0);
  buf[len1] = '\0';
  printf("Connected as player and ack<%s> \n",buf);

}

void sendDataBlock(int block_number)
{
  int len1;
  char playerStr[10];
  char block_buff[]="SEnding you first block";
  sprintf(playerStr,"S:%d:%d",block_number,strlen(block_buff));
  
  len1 = send(connectSoc,playerStr, strlen(playerStr), 0);
  if(len1 < 0)
  { perror("send:"); exit(len1);}

  // Receive ack for the request send
  len1 = recv(connectSoc, buf, 32, 0);
  buf[len1] = '\0';
  printf("Connected as player and buf<%s> \n",buf);

  // Send the block to hot server
  
  len1 = send(connectSoc,block_buff, strlen(block_buff), 0);
  if(len1 < 0)
  { perror("send:"); exit(len1);}

}

void fetchDataBlock(int block_number)
{
  int len1;
  char playerStr[10];

  sprintf(playerStr,"F:%d",block_number);

  len1 = send(connectSoc,playerStr, strlen(playerStr), 0);
  if(len1 < 0)
  { perror("send:"); exit(len1);}

  // Receive ack for the request send
  memset(buf,'\0',sizeof(buf));
  len1 = recv(connectSoc, buf, 32, 0);
  buf[len1] = '\0';
  printf("Connected as player and buf<%s> \n",buf);

  char *block_fetch=(char *)malloc(sizeof(char)*atoi(buf));

  len1 = send(connectSoc,"ACK", strlen("ACK"), 0);
  if ( len1 < 0 ) 
  {
    perror("Fetch Block : Send ACK"); exit(1); 
  }

  len1 = recv(connectSoc, block_fetch, atoi(buf), 0);
  buf[len1] = '\0';
  printf("Block receive<%s> \n",block_fetch);

}

main (int argc, char *argv[])
{
  int block_number;
  /* read host and port number from command line */
  if ( argc != 3 ) {
    fprintf(stderr, "Usage: %s <host-name> <port-number>\n", argv[0]);
    exit(1);
  }

  connectToColdServer(argv[1],atoi(argv[2]));
  
  // Request 1 for sending 2 for fetching the block
  int req=2;

  if(req==1)
  { 
    sendDataBlock(1);

  }else if(req==2)
  {
    fetchDataBlock(0);
  }

  close(connectSoc);
  exit(0);
}
