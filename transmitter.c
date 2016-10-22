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
	
	int fd = llopen(path, TRANSMITTER);
	
	if(fd < 0){
		printf("error on llopen.\n");
		return -1;
	}
	
	
	
}
