#include <stdio.h>
#include <stdlib.h>
#include <calcLib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <cstring>
#include <bitset>

#include "protocol.h"

int main(int argc, char *argv[])
{
  if (argc != 2) 
	{
	  fprintf(stderr,"usage: %s hostname (%d)\n",argv[0], argc);
	  exit(1);
	}

  char delim[] = ":";
  char *Desthost = strtok(argv[1],delim);
  char *Destport = strtok(NULL,delim);

  if (Desthost == NULL)
  {
    perror("Host missing");
    exit(1);
  }

  if (Destport == NULL)
  {
    perror("Port missing");
    exit(1);
  }

  #ifdef DEBUG 
  printf("Host %s, and port %d.\n", Desthost, port);
  #endif

	int sockfd;
	struct addrinfo hints, *servinfo, *p;

    //send and recieve with these
  struct calcMessage calc_message;
  struct calcProtocol calc_protocol;
  int sent_bytes;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM ;
  hints.ai_protocol = IPPROTO_UDP;

    
  int rv;
	if ((rv = (getaddrinfo(Desthost, Destport, &hints, &servinfo))) == -1) 
	{
        fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

  for(p = servinfo; p != NULL; p = p->ai_next) 
  {
    if ((sockfd = socket(p->ai_family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP) == -1) 
    {
      perror("failed to bind to socket");
      continue;
    }
    break;
  }

    calc_message.type = htons(22);
    calc_message.message = htonl(0);
    calc_message.protocol = htons(17);
    calc_message.major_version = htons(1);
    calc_message.minor_version = htons(0);

    bool communication_complete = false;
    struct timeval timer;
    timer.tv_usec = 0;
    timer.tv_sec = 2;

    //Ask for assignment
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    for (int i = 0; i < 3; i++)
    {
      if (!sendto(sockfd, &calc_message, sizeof(calc_message), 0, servinfo->ai_addr, servinfo->ai_addrlen))
      {
        perror("Invalid message or reciever");
        exit(1);
      }

      if ((sent_bytes = recvfrom(sockfd, &calc_protocol, sizeof(calc_protocol), 0, servinfo->ai_addr, &servinfo->ai_addrlen)) == -1)
      {    
        printf("Nothing recieved \n");
      }
      else
      {
        if (sent_bytes == sizeof(calc_protocol))
        {
          communication_complete = true;
          i = 5;
          break;
        }
        else 
        {
          printf("Incorect protocol, exiting..\n");
          communication_complete = true;
          break;
        }
      }
    }

    if (!communication_complete)
    {
      printf("connection timed out.. \n");
      exit(1);
    }

  struct sockaddr_in client_addr;
  socklen_t addr_length;

  addr_length = sizeof(client_addr);

  getsockname(sockfd, (struct sockaddr *) &client_addr, &addr_length);
  printf("Host %s, and port %d\n", Desthost, Desthost);
  #ifdef DEBUG
    printf("Communicating with: %s:%s | local: %s:%d\n", Desthost, Destport, inet_ntoa(client_addr.sin_addr), (int)ntohs(client_addr.sin_port));
  #endif

  //Operators for incoming data stream
  bool is_float = false;
  double f1,f2,fresult;
  int i1,i2,iresult;
  {
    // Initialize the library, this is needed for this library. 
    initCalcLib();
    char command[10];
    std::string message;
    switch (ntohl(calc_protocol.arith))
    {
      case 1:
      message = "add";
      break;

      case 2:
      message = "sub";
      break;

      case 3:
      message = "mul";
      break;

      case 4:
      message = "div";
      break;

      case 5:
      message = "fadd";
      break;

      case 6:
      message = "fsub";
      break;

      case 7:
      message = "fmul";
      break;

      case 8:
      message = "fdiv";
      break;
    
      default:
      printf("no operator available, exiting..");
      exit(1);
      break;
    }

    strcpy(command, message.c_str());
  
    if(command[0]=='f')
    {
      printf("Assignment: %8.8g %s %8.8g\n", calc_protocol.flValue1, command, calc_protocol.flValue2);

      is_float = true;
      f1 = calc_protocol.flValue1;
      f2 = calc_protocol.flValue2;

      if(strcmp(command,"fadd")==0)         fresult=f1+f2;
      else if (strcmp(command, "fsub")==0)  fresult=f1-f2;
      else if (strcmp(command, "fmul")==0)  fresult=f1*f2;
      else if (strcmp(command, "fdiv")==0)  fresult=f1/f2;

  #ifdef DEBUG
    printf("Calculated result: %8.8g\n", fresult);
  #endif

      calc_protocol.flResult = fresult;
    } 
    else 
    {
      printf("Assignment: %d %s %d\n", ntohl(calc_protocol.inValue1), command, ntohl(calc_protocol.inValue2));

      i1 = ntohl(calc_protocol.inValue1);
      i2 = ntohl(calc_protocol.inValue2);

      if(strcmp(command,"add")==0)        iresult=i1+i2;
      else if (strcmp(command, "sub")==0) iresult=i1-i2;
      else if (strcmp(command, "mul")==0) iresult=i1*i2;
      else if (strcmp(command, "div")==0) iresult=i1/i2;

  #ifdef DEBUG
    printf("Calculated result: %d\n", iresult);
  #endif

      calc_protocol.inResult = htonl(iresult);
    }
  }

  //send result
  communication_complete = false;
  for (int i = 0; i < 3; i++)
  {
    if (!sendto(sockfd, &calc_protocol, sizeof(calc_protocol), 0, servinfo->ai_addr, servinfo->ai_addrlen))
    {
      perror("Invalid message or reciever");
      exit(1);
    }
    else
    {
      if (is_float) printf("Result sent: %8.8g \n", calc_protocol.flResult); 
      else printf("My result: %d \n", ntohl(calc_protocol.inResult)); 
    }
      
    if ((sent_bytes = recvfrom(sockfd, &calc_message, sizeof(calc_message), 0, servinfo->ai_addr, &servinfo->ai_addrlen)) == -1)
    {
      printf("Nothing recieved \n");
    }
    else
    {
      if (sent_bytes == sizeof(calc_message))
      {
        if(ntohl(calc_message.message) == 1)
        {
          printf("Server: OK\n");
          communication_complete = true; 
        }
        else printf("Server: NOT OK\n");  
        i = 5;
        break;
      }
      else 
      {
        printf("Incorrect info, exiting..\n");
        break;
      }
    }
  }

  if (!communication_complete)
  {
    printf("connection timed out.. \n");
    exit(1);
  }

  return 1;
}
