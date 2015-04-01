/* Author : Aditya Ambre
   Description : Cold storage daemon which listens for the requests from hot storage server.
                 Store : Request for storing cold data blocks 
                 Fetch : To fetch cold data block stored at the cold storage server.
*/

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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PACKET_LENGTH 512
#define SEED 50 
#define FILE_DIR "/tmp/store"
#define BLOCK_HASH_SIZE 128       

struct sockaddr_in sinaddr, incoming;
int soc;
int hotStorageServer;
int *playerServerSocs;

void startServerSocket(int port)
{
  char host[64];
  struct hostent *hp;
  int rc;

  gethostname(host, sizeof host);
  hp = gethostbyname(host);
  if ( hp == NULL ) 
  {
    fprintf(stderr, "host not found (%s)\n", host);
    exit(1);
  }

  // Define server socket for the hot storage server
  soc = socket(AF_INET, SOCK_STREAM, 0);
  if ( soc < 0 ) {
    perror("socket:");
    exit(1);
  }

  // set up the address and port 
  sinaddr.sin_family = AF_INET;
  sinaddr.sin_port = htons(port);
  memcpy(&sinaddr.sin_addr, hp->h_addr_list[0], hp->h_length);
  
  int yes=1;
  setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  
  // bind socket soc to address sin 
  rc = bind(soc, (struct sockaddr *)&sinaddr, sizeof(sinaddr));
  if ( rc < 0 ) {
    perror("bind:");
    exit(rc);
  }

  // Server socket listening for connections
  rc = listen(soc, 5);
  if ( rc < 0 ) {
    perror("listen:");
    exit(rc);
  }

}

void acceptRequest(int hotStorageServer)
{
  int len,recvLen,endOfBuf;
  int cblock_num;
  char buf[50];
  char blk_file_name[BLOCK_HASH_SIZE];
  memset(blk_file_name,'\0',BLOCK_HASH_SIZE);
  // Get player socket address information
  len = recv(hotStorageServer, buf, 50, 0);
  if ( len < 0 ) 
  {  
    perror("recv");exit(1);
  }

  buf[len] = '\0';

  if(buf[0]=='S') // Store request
  {  
    // Expecting input format is S:<Block Number>:<Lengh of the block>
    printf("control info got is <%s>\n", buf);
    char* splitpotato=strtok(buf, ":");

    cblock_num=atoi(strtok(NULL, ":"));

    recvLen=atoi(strtok(NULL, ":"));
    endOfBuf=recvLen;
    // Send Ack to player
    len = send(hotStorageServer,"ACK", strlen("ACK"), 0); 
    if(len < 0)
    { perror("send:"); exit(len);}

    char* inputBuffer=malloc((recvLen)*sizeof(char));
    char *readBuffer=inputBuffer;
    while(recvLen >0)
    {         
      // Receive potato from player in loop, as server can accpet limited data at a time
      len=recv(hotStorageServer, readBuffer, PACKET_LENGTH, 0);
      if((recvLen-len) >0)
        readBuffer=readBuffer+len;
      recvLen=recvLen - len;
    }
    inputBuffer[endOfBuf] = '\0';
    printf("Block to be stored:\n%s\n", inputBuffer);

    // Writing file block in FILE_DIR
    sprintf(blk_file_name,"%s/%d",FILE_DIR,cblock_num);
    printf("just checking Block file name <%s>\n", blk_file_name);

    FILE *fptr;
    fptr=fopen(blk_file_name,"w");
    if(fptr==NULL)
    {
      printf("Error!");
      exit(1);
    }

    fprintf(fptr,"%s",inputBuffer);
    fclose(fptr);

  }
  else if(buf[0]=='F') // Fetch request
  {
    //printf("control info got is <%s>\n", buf);
    char* splitpotato=strtok(buf, ":");

    cblock_num=atoi(strtok(NULL, ":"));
    
    sprintf(blk_file_name,"%s/%d",FILE_DIR,cblock_num);
    printf("just checking Block file name <%s>\n", blk_file_name);

    FILE *f;
    f=fopen(blk_file_name,"r");
    if(f==NULL)
    {
      printf("Error!");
      exit(1);
    }
    int fd = fileno(f);
    struct stat buf1;
    fstat(fd, &buf1);
    int size = buf1.st_size;

    char *buffer=malloc(sizeof(char)*size);
    fread(buffer,1,size,f);
    printf("Block to be send :<%s>\n",buffer );
    // Send block size to player
    char blk_size[10];
    memset(blk_size,'\0',10);
    sprintf(blk_size,"%d",size);

    len = send(hotStorageServer,blk_size, strlen(blk_size), 0); 
    if(len < 0)
    { perror("send:"); exit(len);}


    // Receive ack for the request send
    len = recv(hotStorageServer, buf, 32, 0);
    buf[len] = '\0';
    printf("ACK for block size information sent<%s> \n",buf);

    // Send Ack to player
    len = send(hotStorageServer,buffer, size, 0); 
    if(len < 0)
    { perror("send:"); exit(len);}
     
    printf("File sended successfully \n"); 
  }  
}


void acceptConnection()
{
  int len,activity;
  len = sizeof(sinaddr);
  fd_set nextReadfds;
  // max_sd=soc;

  while(1)
  { 
    //activity = select( max_sd + 1 , &nextReadfds , NULL , NULL , NULL);

    hotStorageServer= accept(soc, (struct sockaddr *)&incoming, &len);
    //printf("%d %d\n",activity,errno);
    if ( hotStorageServer < 0 ) 
    {
      perror("bind:"); exit(1);
    }

    len = send(hotStorageServer,"ACK", strlen("ACK"), 0);
    if ( len < 0 ) 
    {
      perror("acceptConnection : Send"); exit(1); 
    }

    acceptRequest(hotStorageServer);
  }  

}

void sendExitCall()
{
   int len;
   // Send exit call to all players to end the game
     len = send(hotStorageServer,"Exit", strlen("Exit"), 0);
     if ( len < 0 ) 
     { perror("send");exit(1);}

}

main (int argc, char *argv[])
{

  int len, port,i;
  // Check number of command line arguments 
  if ( argc < 2 ) 
  {
    fprintf(stderr, "Usage: %s <port-number> \n", argv[0]);
    exit(1);
  }

  //read port number from command line
  port = atoi(argv[1]); 

  // Start Server socket
  startServerSocket(port);

  // Listen for the TCP connection from Hot storage server
  acceptConnection();

  sendExitCall();

  close(soc);
  exit(0);
}
