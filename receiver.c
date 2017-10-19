#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "DataLink.h"
#include "AppLayer.h"

#define RECEIVER 0
#define TRANSMITTER 1

int main(int argc, char** argv)
{
    
    	if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS4", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS0\n");
      exit(1);
	}
	
	const char * path = argv[1];

	if (receiveFile(path) < 0)
		printf("File not received :(\n");
	else printf("File received successfully!\n");

/*	int fd = startConnection(path);

    	if(fd == -1){
    		printf("Cant find a connection.");
    		return 0;
    	}


    	llopen(fd, RECEIVER);

	
	printf("*********************************************************\n");

	unsigned char * msg = malloc(20);
	llread(fd, msg);


	printf("*********************************************************\n");
	printf("%s\n", msg);
	
	
	llclose(fd, RECEIVER);


	printf("*********************************************************\n");
*/
	return 0;
}
