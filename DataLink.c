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

struct termios oldtio, newtio;

#define RECEIVER 0
#define TRANSMITTER 1

#define BAUDRATE B9600
#define NUM_TRIES 3

#define FLAG   0x7E
#define A      0x03
#define C1	   0x40
#define C0	   0x00
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

const unsigned char SET[] = {FLAG, A, C_SET, A^C_SET,FLAG};
const unsigned char UA[] = {FLAG, A, C_UA, A^C_UA, FLAG};
const unsigned char DISC[] = {FLAG, A, C_DISC, A^C_DISC, FLAG};
const unsigned char RR0[] = {FLAG, A, C_RR0, A^C_RR0, FLAG};
const unsigned char RR1[] = {FLAG, A, C_RR1, A^C_RR1, FLAG};
const unsigned char REJ0[] = {FLAG, A, C_REJ0, A^C_REJ0, FLAG};
const unsigned char REJ1[] = {FLAG, A, C_REJ1, A^C_REJ1, FLAG};

typedef enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP } ConnectionState;

int flag=0, conta=0;

volatile static int control = 0;

void atende()                   // atende alarme
{
	printf("alarme # %d\n", conta);
	flag=1;
	conta++;
}

int stateMachine(ConnectionState * state, char ch){

	int control_field = -1;
	
	switch(*state) {
		case START:
			if(ch == FLAG)
				*state = FLAG_RCV;
			break;
			
		case FLAG_RCV:
			if(ch == A)								
				*state = A_RCV;
			else if(ch == FLAG) ;					
			else *state = START;					
			break;
			
		case A_RCV:
			if(ch == C_UA){
				*state = C_RCV;
				control_field = C_UA;
			}
			else if (ch == C_SET){
				*state = C_RCV;
				control_field = C_SET;
			} else if(ch == C_DISC){
				*state = C_RCV;
				control_field = C_DISC;
			} else if(ch == FLAG)
				*state = FLAG_RCV;
			else *state = START;
			break;
			
		case C_RCV:
			if(ch == A^C_UA || ch == A^C_SET || ch == A^C_DISC)
				*state = BCC_OK;
			else if(ch == FLAG)
				*state = FLAG_RCV;
			else *state = START;
			break;
			
		case BCC_OK:
			if(ch == FLAG)
				*state = FLAG_RCV;
			else *state = START;
			break;
		default:
			break;

	}
	
	return control_field;
}

int llopen(const char * path, int type){
	ConnectionState state;
	int i = 0;
	unsigned char ch;
	
	int fd = open(path, O_RDWR | O_NOCTTY);
	
	if (fd <0) { perror(path); exit(-1); }
	
	if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
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
      exit(-1);
    }

    printf("New termios structure set\n");
   
    if(type == TRANSMITTER){ 
   		int try = 0;
   		int control_field = -1;
   		state = START;
   		
    	while(try < NUM_TRIES) {
    		if(write(fd,SET,sizeof(SET)) <= 0){		// sends SET frame
    			sleep(1);
    			try++;
    			continue;
    		}
    		else break; 		   		
    	
    		for(i=0; i < sizeof(UA); i++){				// receives UA frame sent by the receiver
    			read(fd, &ch, 1);
    			int ret = stateMachine(&state, ch);	
    			
    			if(ret != -1)
    				control_field = ret;
    		}
    		
    	
    		if(state == STOP && control_field == C_UA){
    			printf("UA received successfully.\n");	// reading was successful and connection was established
    			break;
    		} else{
    			printf("Error receiving UA.\n");
    			return -1;
    		}
    	}
    	
    	if(try == NUM_TRIES)						// exceeded max number of tries
    		return -1; 
    	    		
    } else if(type == RECEIVER){
    	int try = 0;
    	int control_field = -1;
    	state = START;
    	
   		while(try < NUM_TRIES) {
     		for(i=0; i < sizeof(SET); i++){			// reads SET frame sent by the transmitter
    			read(fd, &ch, 1);
    			int ret = stateMachine(&state, ch);
    			
    			if(ret != -1)
    				control_field = ret;
    		}
    	
    		if(state == STOP && control_field == C_SET){
    			printf("SET received succesfully\n");
    			break;
    		} else{
    			printf("Error receiving SET");
    			return -1;
    		}
    	}
    	
    	if(try == NUM_TRIES)
    		return -1;
    		
    	
    	if(write(fd,UA,sizeof(UA)) < 0){
    		printf("Error writing UA.\n");
    		return -1;
    	} else printf("UA sent successfully.\n");
    }
    
    return fd;
}

unsigned char getBCC(const unsigned char* buffer, int length) {
	unsigned char BCC = 0;

	for(unsigned int i = 0; i < length; i++)
		BCC ^= buffer[i];

	return BCC;
}

unsigned char * encapsulatePacket(const unsigned char * buffer, int length){
	unsigned char* pckt = malloc(DATA_PACKET_SIZE + length);

	pckt[0] = FLAG;
	pckt[1] = A;

	unsigned char BCC1;
	if(control){
		pckt[2] = C1;
		BCC1 = A ^ C1;

	} else {
		pckt[2] = C0;
		BCC1 = A ^ C0;
	}

	pckt[3] = BCC1;

	memcpy(&pckt[4], buffer, length);

	unsigned char BCC2 = getBCC(buffer, length);

	pckt[4 + length] = BCC2;
	pckt[5 + length] = FLAG;

	return pckt;
}

unsigned int stuffPacket(unsigned char** packet, int length){
	unsigned int newPacketSize = length;

	int i;
	for (i = 1; i < length - 1; i++)
		if ((*packet)[i] == FLAG || (*packet)[i] == ESCAPE)
			newPacketSize++;

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

int receiveCommandPacket(int fd, const unsigned char * cmd){
	ConnectionState state = START;
    unsigned char ch;
	int control_field = -1;

    unsigned int i;
	for(i=0; i < CONTROL_PACKET_SIZE; i++){				// receives UA frame sent by the receiver
		read(fd, &ch, 1);
		int ret = stateMachine(&state, ch);

		if(ret != -1)
			control_field = ret;
	}


	if(state == STOP && control_field == cmd[2]){
		printf("Successfully received command.\n");	// reading was successful and connection was established
		return 1;
	} else{
		printf("Error receiving command.\n");
		return 0;
	}
}

int llwrite(int fd, const unsigned char * buffer, int length){
    int transfer = 1;
    int try = 0;

    unsigned char* packet;
    unsigned int packetSize;

    while(transfer){
        if(try == 0){
            if(try >= NUM_TRIES){
                printf("Message not sent.\n");
                return 0;
            }
            
            packet = encapsulatePacket(buffer, length);
            packetSize = stuffPacket(&packet, DATA_PACKET_SIZE + length);
            write(fd, packet, packetSize); //send Packet
            
            if(try++ == 1){
                //setalarm
            }
        }



        if(control && receiveCommandPacket(fd, RR0)){
        	//stopalarm
        	control--;
            transfer = 0;
        } else if(!control && receiveCommandPacket(fd, RR1)){
        	//stopalarm
        	control++;
            transfer = 0;
        } else {
			//stopAlarm();
			try = 0;
		}
    }

    //stopalarm

    return 1;
}

int llclose (int fd, int type){
	ConnectionState state;
	int i = 0;
	unsigned char ch;
	
	if(type == TRANSMITTER){
		int try = 0;
		int control_field = -1;
		state = START;
		
		while(try < NUM_TRIES){
			if(write(fd, DISC, sizeof(DISC)) <= 0){
				try++;
				continue;
			} else{
				printf("DISC sent successfully.\n");
				break;
			}
					
			for(i=0; i < sizeof(DISC); i++){
    			read(fd, &ch, 1);
    			int ret = stateMachine(&state, ch);
    			
    			if(ret != -1)
    				control_field = ret;
    		}
    		
    		if(state == STOP && control_field == C_DISC){
    			printf("DISC received successfully.\n");
    			break;
    		} else return -1;
    		
    		if(write(fd,UA, sizeof(UA)) <= 0){
    			printf("Error writing UA.\n");
    			return -1;
    		}
    		
    	}
	} else if(type == RECEIVER){
		int try = 0;
    	int control_field = -1;
    	state = START;
    	
   		while(try < NUM_TRIES) {
     		for(i=0; i < sizeof(DISC); i++){			// reads DISC frame sent by the transmitter
    			read(fd, &ch, 1);
    			int ret = stateMachine(&state, ch);
    			
    			if(ret != -1)
    				control_field = ret;
    		}
    	
    		if(state == STOP && control_field == C_DISC){
    			printf("DISC receiverd successfully.\n");
    			break;
    		} else try++;
    	}
    	
    	if(try == NUM_TRIES)						// exceeded max num of tries
    		return -1;
    		
    	
    	if(write(fd,DISC,sizeof(DISC)) < 0){		// writes frame DISC and checks if it was successful
    		printf("Error writing DISC.\n");
    		return -1;
    	}
    	
		for(i=0; i < sizeof(UA); i++){
			read(fd, &ch, 1);
			int ret = stateMachine(&state, ch);
			
			if(ret != -1)
				control_field = ret;
		}
		
		if(state == STOP && control_field == C_UA){
			printf("UA received succesfully.\n");
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
