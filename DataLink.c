#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include "DataLink.h"
#include "Alarm.h"

struct termios oldtio, newtio;

#define RECEIVER 0
#define TRANSMITTER 1

#define BAUDRATE B9600
#define NUM_TRIES 3

#define FLAG   0x7E
#define A      0x03
#define C1     0x40
#define C0     0x00
#define C_SET  0x03
#define C_UA   0x03
#define C_DISC 0x0B
#define C_RR0  0x05
#define C_RR1  0x85
#define C_REJ0 0x01
#define C_REJ1 0x81

#define ESCAPE 0x7d

#define CONTROL_PACKET_SIZE	5
#define DATA_PACKET_SIZE 	6*sizeof(char)

#define TIMEOUT 5

const unsigned int MESSAGE_DATA_MAX_SIZE = 512;

const unsigned char SET[] = {FLAG, A, C_SET, A^C_SET,FLAG};
const unsigned char UA[] = {FLAG, A, C_UA, A^C_UA, FLAG};
const unsigned char DISC[] = {FLAG, A, C_DISC, A^C_DISC, FLAG};
const unsigned char RR0[] = {FLAG, A, C_RR0, A^C_RR0, FLAG};
const unsigned char RR1[] = {FLAG, A, C_RR1, A^C_RR1, FLAG};
const unsigned char REJ0[] = {FLAG, A, C_REJ0, A^C_REJ0, FLAG};
const unsigned char REJ1[] = {FLAG, A, C_REJ1, A^C_REJ1, FLAG};

int control_field = -1;

typedef enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA_RCV, STOP } ConnectionState;

volatile static int control = 0;

int stateMachine(ConnectionState * state, unsigned char ch){

	switch(*state) {
		case START:
			if(ch == FLAG){
				*state = FLAG_RCV;
			}
			break;

		case FLAG_RCV:
			if(ch == A){
				*state = A_RCV;
			}
			else if(ch == FLAG) ;
			else *state = START;
			break;

		case A_RCV:
			if(ch == C_UA || ch == C_SET || ch == C_DISC || ch == C_RR0 ||
					ch == C_RR1 || ch == C_REJ0 || ch == C_REJ1){
				*state = C_RCV;
				control_field = ch;
			}
			else if(ch == FLAG)
				*state = FLAG_RCV;
			else *state = START;
			break;

		case C_RCV:
			if(ch == (A^C_UA) || ch == (A^C_SET) || ch == (A^C_DISC) ||ch == (A^C_RR0)
					||ch == (A^C_RR1) || ch == (A^C_REJ0) || ch == (A^C_REJ1)){
				*state = BCC_OK;
			}
			else if(ch == FLAG)
				*state = FLAG_RCV;
			else *state = START;
			break;

		case BCC_OK:
			if(ch == FLAG){		// received command
				*state = STOP;
			}
			else if(ch != FLAG){ 	// received data
				*state = DATA_RCV;
			}
			break;
		default:
			break;

	}

	return control_field;
}

int dataStateMachine(ConnectionState * state, unsigned char ch){
	
	switch(*state) {
		case START:
			if(ch == FLAG){
				control_field = ch;
				*state = FLAG_RCV;
			}
			break;
			
		case FLAG_RCV:
			if(ch == A){
				control_field = ch;
				*state = A_RCV;
			}
			else if(ch == FLAG) ;
			else *state = START;
			break;
			
		case A_RCV:
			if(ch == C0 || ch == C1){
				control_field = ch;
				*state = C_RCV;
			}
			else if(ch == FLAG)
				*state = FLAG_RCV;
			else *state = START;
			break;
			
		case C_RCV:
			if(ch == (A^C0) || ch == (A^C1) ){
				control_field = ch;
				*state = BCC_OK;
			}
			else if(ch == FLAG){
				*state = FLAG_RCV;
			} else *state = START;
			break;
			
		case BCC_OK:
			if(ch == FLAG){			//received command
				control_field = ch;
				*state = STOP;
			}
			else if(ch != FLAG){ 	//received data
				control_field = ch;
				*state = DATA_RCV;
			}
			break;
		default:
			break;

	}

	return control_field;
}

int startConnection(const char * path){

	int fd = open(path, O_RDWR | O_NOCTTY);
	
	if (fd <0) { perror(path); exit(-1); }
	
	if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      		perror("tcgetattr");
      		return -1;
    	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    	newtio.c_iflag = IGNPAR;
    	newtio.c_oflag = 0;

    	/* set input mode (non-canonical, no echo,...) */
    	newtio.c_lflag = 0;

    	newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    	newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

   	/* 
    	VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    	leitura do(s) próximo(s) caracter(es)
   	*/

    	tcflush(fd, TCIOFLUSH);

    	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      		perror("tcsetattr");
      		return -1;
    	}

    	printf("New termios structure set\n");

    	return fd;
}


int llopen(int fd, int type){

	ConnectionState state;
	int i = 0;
	unsigned char ch;

    	if(type == TRANSMITTER){
		printf("LLOPEN: Transmitter start.\n");
   		int try = 0;
   		int ret = -1;
   		state = START;


    		while(try < NUM_TRIES) {
			if(try == 0 || flag){
				flag = 0;

	    			if(write(fd,SET,sizeof(SET)) <= 0){			// sends SET frame
	    				printf("LLOPEN: Error writing SET. Another try.\n");
	    				try++;
	    				continue;
	    			} else printf("LLOPEN: SET sent successfully.\n");
				
				if(++try == 1)			
					setAlarm();
			}

    			for(i=0; i < sizeof(UA); i++){				// receives UA frame sent by the receiver
    				read(fd, &ch, 1);
    				ret = stateMachine(&state, ch);
    			}

    			if(state == STOP && ret == C_UA){
    				printf("LLOPEN: UA received successfully.\n");	// reading was successful and connection was established
				stopAlarm();
    				break;
    			} else{
    				printf("LLOPEN: Error receiving UA. Another try.\n");
    				try++;
    			}
    		}
    	
    		if(try == NUM_TRIES){						// exceeded max number of tries
    			printf("LLOPEN: Exceeded max number of tries.\n");
			return -1;
		}

    } else if(type == RECEIVER){
		printf("LLOPEN: Receiver start.\n");
    		int try = 0;
    		int ret = -1;
    		state = START;

   		while(try < NUM_TRIES) {
	     		for(i=0; i < sizeof(SET); i++){				// reads SET frame sent by the transmitter
	    			read(fd, &ch, 1);
	    			ret = stateMachine(&state, ch);
	    		}
	    	
	    		if(state == STOP && ret == C_SET){
	    			printf("LLOPEN: SET received succesfully\n");
	    		} else{
	    			printf("LLOPEN: Error receiving SET. Another try.\n");
	    			try++;
				continue;
	    		}

			if(write(fd,UA,sizeof(UA)) < 0){
	    			printf("LLOPEN: Error writing UA. Another try.\n");
	    			try++;
	    		} else{
				printf("LLOPEN: UA sent successfully.\n");
				break;
			}
	    	}
	    	
    		if(try == NUM_TRIES){						// exceeded max number of tries
    			printf("LLOPEN: Exceeded max number of tries.\n");
			return -1;
		}

   	 }

    
    	return fd;
}

unsigned char getBCC(const unsigned char* buffer, int length) {
	unsigned char BCC = 0;
	unsigned int i;

	for(i = 0; i < length; i++)
		BCC ^= buffer[i];

	return BCC;
}

int encapsulatePacket(unsigned char ** pckt, const unsigned char * buffer, int length){
	(*pckt)[0] = FLAG;
	(*pckt)[1] = A;

	unsigned char BCC1;
	if(control){
		(*pckt)[2] = C1;
		BCC1 = A ^ C1;
	} else {
		(*pckt)[2] = C0;
		BCC1 = A ^ C0;
	}

	(*pckt)[3] = BCC1;

	memcpy(&(*pckt)[4], buffer, length);

	unsigned char BCC2 = getBCC(buffer, length);

	(*pckt)[4 + length] = BCC2;
	(*pckt)[5 + length] = FLAG;

	return 0;
}

unsigned int stuffPacket(unsigned char** packet, int length){
	unsigned int newPacketSize = length;

	int i;
	for (i = 1; i < length - 1; i++){
		if ((*packet)[i] == FLAG || (*packet)[i] == ESCAPE)
			newPacketSize++;
		}

	*packet = (unsigned char*) realloc(*packet, newPacketSize);

	for (i = 1; i < length - 1; i++) {
		if ((*packet)[i] == FLAG || (*packet)[i] == ESCAPE) {
			memmove(*packet + i + 1, *packet + i, length - i);

			length++;

			(*packet)[i] = ESCAPE;
			(*packet)[i + 1] ^= 0x20;
		}
	}

	return newPacketSize;
}

int receivePacket(int fd, unsigned char ** buffer, unsigned int * buffSize){
	ConnectionState state = START;
	volatile int received = 0;
	int cReceived;
	int length = 0;

    	while(!received){
        	unsigned char ch;

        	if(state != STOP){
        		cReceived = read(fd, &ch, 1);
        		if(!cReceived)
        			return cReceived;
        	}

        	ch = dataStateMachine(&state,ch);

        	if(state == DATA_RCV){
        		if (length % (*buffSize) == 0) {
				int factor = length / (*buffSize) + 1;
				(*buffer) = (unsigned char*) realloc((*buffer), factor * (*buffSize));
			}
        	state = BCC_OK;
        	}

        	(*buffer)[length++] = ch;

        	if(state == STOP){
        		*buffSize = length;
        		received = 1;
        		break;
        	}
    	}

    	return received;
}

unsigned int destuffPacket(unsigned char** packet, int length){

	int i;
	for (i = 1; i < length - 1; ++i) {
		if ((*packet)[i] == ESCAPE) {
			memmove(*packet + i, *packet + i + 1, length - i - 1);

			length--;

			(*packet)[i] ^= 0x20;
		}
	}

	*packet = (unsigned char*) realloc(*packet, length);

	return length;
}

int llwrite(int fd, const unsigned char* buffer, int length){
	int transfer = 1;
	int try = 0;

	ConnectionState state = START;
	int i;
	unsigned char ch;

	unsigned char* packet = malloc(DATA_PACKET_SIZE + length);
	unsigned int packetSize;

	while(transfer){
        	if(try == 0 || flag){
			flag = 0;
           		if(try >= NUM_TRIES){
                		printf("LLWRITE: Message not sent.\n");
                		return 0;
            		}
            
            		encapsulatePacket(&packet, buffer, length);
           		packetSize = stuffPacket(&packet, DATA_PACKET_SIZE + length);
            		write(fd, packet, packetSize); // send Packet

            		if(++try == 1){
                		setAlarm();
            			printf("LLWRITE: packet sent\n");
            		}
        	}
        

		int ret = -1;
		control_field = -1;
        	for(i=0; i < sizeof(RR0); i++){
        		read(fd, &ch, 1);
        		ret = stateMachine(&state, ch);
        	}

        	if(state == STOP && ( (ret == C_RR0 && control) || (ret == C_RR1 && !control) ) ){
        		printf("LLWRITE: received RR command\n");

        		control = !control;
			stopAlarm();
        		transfer = 0;
        	} else if(state == STOP && ( (ret == C_REJ0 && control) || (ret == C_REJ1 && !control) ) ){
        		printf("LLWRITE: received REJ command\n");

			stopAlarm();
			try = 0;
       		}
    	}

    	stopAlarm();
		
    	free(packet);

    	printf("LLWRITE: packet sent & received confirmation packet\n");

	return 1;
}

int verifyDataPacketReceived(unsigned char * buffer, int size){
	unsigned char a = buffer[1];
	unsigned char c = buffer[2];
	unsigned char bcc1 = buffer[3];

	if(bcc1 == (a^c) && (c == C0 || c == C1)){

			unsigned char BCC2rcvd = getBCC(&buffer[4], size-6); //entre buffer[4] e buffer[size-(4+2)], so os dados
			unsigned char bcc2 = buffer[size - 2];

			if(bcc2 != BCC2rcvd){
				printf("LLREAD: Bad bcc2\n");
				return 0;
			}
	} else if(c != C0 && c != C1){
		printf("LLREAD: Bad control field: %d\n", c);
		return 0;
	}

	return 1;
}

int llread(int fd, unsigned char* buffer){
    unsigned int packetSize = DATA_PACKET_SIZE;
    unsigned char * packet = malloc(packetSize);
    unsigned int size;
	int received = 0;
	while(!received){
		if(receivePacket(fd, &packet, &packetSize)){
			printf("LLREAD: received a packet\n");

			size = destuffPacket(&packet, packetSize);
			printf("LLREAD: destuffed packet\n");

			if(!verifyDataPacketReceived(packet, size)){
				printf("LLREAD: Packet is not data or has header errors\n");
				return 0;
			} else {
				sleep(1);
				if((packet[2] == C0 && !control) || (packet[2] == C1 && control)){
					control = !control;

					//guardar no buffer (com decapsulation incluida)
					memcpy(buffer, &packet[4], size-DATA_PACKET_SIZE);

					if(control)
						write(fd,RR1,sizeof(RR1));
					else
						write(fd,RR0,sizeof(RR0));

					received = 1;
				} else if(packet[2] == C0 && control){
					//nao guarda
					write(fd,REJ1,sizeof(REJ1));

					printf("LLREAD: recebeu e enviou REJ1\n");
					return 0;
				} else if(packet[2] == C1 && !control){
					//nao guarda
					write(fd,REJ0,sizeof(REJ0));

					printf("LLREAD: recebeu e enviou REJ0\n");
					return 0;
				} else
					return 0;
			}
		}
	}

	free(packet);

	stopAlarm();

	return 1;
}

int llclose (int fd, int type){
	ConnectionState state;
	int i = 0;
	unsigned char ch;

	if(type == TRANSMITTER){
		printf("LLCLOSE: Transmitter start.\n");
		int try = 0;
		int control_field = -1;
		state = START;

		while(try < NUM_TRIES){
			if(write(fd, DISC, sizeof(DISC)) <= 0){
				printf("LLCLOSE: Error writing DISC. Another try.\n");
				try++;
				continue;
			} else printf("LLCLOSE: DISC sent successfully.\n");
			
		

			for(i=0; i < sizeof(DISC); i++){
    				read(fd, &ch, 1);
    				int ret = stateMachine(&state, ch);

    				if(ret != -1)
    					control_field = ret;
    			}

    			if(state == STOP && control_field == C_DISC){
    				printf("LLCLOSE: DISC received successfully.\n");
    			} else {
				printf("LLCLOSE: Error reading DISC. Another try.\n");
				try++;
				continue;
			}

    			if(write(fd,UA, sizeof(UA)) <= 0){
    				printf("LLCLOSE: Error writing UA. Another try.\n");
    				try++;
    			} else{
				printf("LLCLOSE: UA sent successfully.\n");
				break;
			}
		}

		if(try == NUM_TRIES){					// exceeded max number of tries
    			printf("LLOPEN: Exceeded max number of tries.\n");
			return -1;
		}

	} else if(type == RECEIVER){
		printf("LLCLOSE: receiver start.\n");
		int try = 0;
    		int control_field = -1;
    		state = START;

   		while(try < NUM_TRIES) {
     			for(i=0; i < sizeof(DISC); i++){		// reads DISC frame sent by the transmitter
    				read(fd, &ch, 1);
    				int ret = stateMachine(&state, ch);

    				if(ret != -1)
    					control_field = ret;
    			}

    			if(state == STOP && control_field == C_DISC){
    				printf("LLCLOSE: DISC received successfully.\n");
    			} else{
				printf("LLCLOSE: Error reading DISC. Another try.\n");
				try++;
				continue;
			}

	    		if(write(fd,DISC,sizeof(DISC)) < 0){			// writes frame DISC and checks if it was successful
	    			printf("LLCLOSE: Error writing DISC. Another try\n");
	    			try++;
				continue;
	    		} else printf("LLCLOSE: Disc sent successfully\n");

			state = START;

			for(i=0; i < sizeof(UA); i++){
				read(fd, &ch, 1);

				int ret = stateMachine(&state, ch);

				if(ret != -1)
					control_field = ret;
			}

			if(state == STOP && control_field == C_UA){
				printf("LLCLOSE: UA received succesfully.\n");
				break;
			} else{
				printf("LLCLOSE: Error receiving UA. Another try.\n");
				try++;
			}
		}

		if(try == NUM_TRIES){						// exceeded max number of tries
    			printf("LLCLOSE: Exceeded max number of tries.\n");
			return -1;
		}

	}

	if (tcsetattr(fd,TCSANOW, &oldtio)== -1)
	{
		perror("tcsetattr");
		exit(-1);
	}
	    
	close(fd);
	return(0);
}
