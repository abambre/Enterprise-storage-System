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

struct sockaddr_in sinaddr, incoming;
int soc;
int coldServer;
int *playerServerSocs;


main (int argc, char *argv[])
{
  int connectSoc, rc,port;
  char host[LEN];
  struct hostent *hp;
  struct sockaddr_in sin;
  int block_number;
  char buf[512];
  /* read host and port number from command line */
  if ( argc != 3 ) {
    fprintf(stderr, "Usage: %s <host-name> <port-number>\n", argv[0]);
    exit(1);
  }
  
  /* fill in hostent struct */
  hp = gethostbyname(argv[1]); 
  if ( hp == NULL ) {
    fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
    exit(1);
  }
  port = atoi(argv[2]);

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

  // Request 1 for sending 2 for fetching the block
  int req=2;
  char playerStr[10];
  if(req==1)
  { 
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

  }else if(req==2)
  {
    block_number=0;
    sprintf(playerStr,"F:%d",block_number);

    len1 = send(connectSoc,playerStr, strlen(playerStr), 0);
    if(len1 < 0)
    { perror("send:"); exit(len1);}

    // Receive ack for the request send
    memset(buf,'\0',sizeof(buf));
    len1 = recv(connectSoc, buf, 32, 0);
    buf[len1] = '\0';
    printf("Connected as player and buf<%s> \n",buf);

    char *block_fetch=malloc(sizeof(char)*atoi(buf));

    len1 = send(connectSoc,"ACK", strlen("ACK"), 0);
    if ( len1 < 0 ) 
    {
      perror("Fetch Block : Send ACK"); exit(1); 
    }

    len1 = recv(connectSoc, block_fetch, atoi(buf), 0);
    buf[len1] = '\0';
    printf("Block receive<%s> \n",block_fetch);

  }

  close(soc);
  exit(0);
}
