#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AppLayer.h"
#include "DataLink.h"

int printUsage(){
    printf("Usage:<port> <send = 0 |receive = 1>  <filename>\n");
    printf("     \tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");

	return 1;
}

int main(int argc, char** argv) {

	//**TODO arg1 ttyS0; arg2 filename**//

    if ( (argc != 4) ||
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    	printUsage();
    	exit(1);
    }

	if(atoi(argv[2]) == 1)

		return sendFile(argv[1], argv[2]);

	else if (atoi(argv[2]) == 2)

		return receiveFile(argv[1]);

	else{
    	printUsage();
    	exit(1);
	}
}
