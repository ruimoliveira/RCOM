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

const unsigned char msg[] = {'s','i','g','a','!',' ','b','a','m','b','o','r','a','!'};

int main(int argc, char** argv)
{
    
    	if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS4", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS0\n");
      exit(1);
	}

int file = open("./pinguim.gif",O_RDONLY);

	if (file == -1){
		printf("Error opening file.\n");
		return -1;
	}
	const char * path = argv[1];
	sendFile(path, file, "pinguim.gif");
	
	/*const char * path = argv[1];
	int fd = startConnection(path);

    	if(fd == -1){
    		printf("Can't find a connection.\n");
    		return 0;
    	}


    	if (llopen(fd, TRANSMITTER) < 0){
		printf("Error on llopen.\n");
		return -1;
	}

	int file = open("./pinguim.gif",O_RDONLY);

	if (file == -1){
		printf("Error opening file.\n");
		return -1;
	}

	struct stat stat_buf;

	if (fstat(file, &stat_buf) == -1){
		perror("fstat ERROR"); 
		return -1;
	}

	int file_size = stat_buf.st_size;	

	
	printf("*********************************************************\n");


	llwrite(fd, msg, sizeof(msg));


	printf("*********************************************************\n");


	llclose(fd, TRANSMITTER);
*/
	return 0;
}
