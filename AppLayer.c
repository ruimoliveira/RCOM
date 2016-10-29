#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AppLayer.h"
#include "DataLink.h"

#define DATA	1
#define START   2
#define END     3
#define T_FILE_SIZE 0
#define T_FILE_NAME 1

#define TRANSMITTER 1
#define RECEIVER 0

#define MAX_SIZE 256

int getFileSize(FILE* file) {
	// saving current position
	long int currentPosition = ftell(file);

	// seeking end of file
	if (fseek(file, 0, SEEK_END) == -1) {
		printf("ERROR: Could not get file size.\n");
		return -1;
	}

	// saving file size
	long int size = ftell(file);

	// seeking to the previously saved position
	fseek(file, 0, currentPosition);

	// returning size
	return size;
}

int sendFile(const char * path, char *filename){
    //abrir antes de enviar
    FILE *file = fopen (filename, "rb");

    //teste para ver se o arquivo foi criado
    if (file == NULL){
        printf("Erro na abertura do arquivo.\n");
        return 0;
    }
    else
    {
        printf("Sucesso na abertura do arquivo.\n");
    }

    //Abrir conex�o
    int fd = startConnection(path);
    if(fd == -1){
    	printf("Cant find a connection.");
    	return 0;
    }
    int a = llopen(fd, TRANSMITTER);
    if (a<=0)//verifica conex�o
    {
        return(0);
    }
    //tamanho do arquivo a ser enviado
    int fileSize = getFileSize(file);
    char fileSizeBuf[sizeof(int)*3+2];//tamanho da trama
    snprintf(fileSizeBuf, sizeof fileSizeBuf, "%d", fileSize); //snprintf redireciona a sa�da de um printf numa string

    //enviar pacote de controle de partida
    if(!sendControlPackage(fd, START, fileSizeBuf, filename))
    {
    	printf("Couldnt send Control Packet.\n");
        return 0;
    }

    //alocar espaco para buffer de arquivo
    char* filebuf = malloc(MAX_SIZE);

	// le pedacos de arquivo
	unsigned int readBytes = 0, writtenBytes = 0, i = 0;

	while ((readBytes = fread(filebuf, sizeof(char), MAX_SIZE, file)) > 0) {
        //createControlPacket(fd, (i++) % 255, fileBuf, readBytes)

		// envia os pedacos dentro do pacote de dados
		if (!sendDataPackage(fd, (i++) % 255, filebuf, readBytes)) {
			free(filebuf);
			return 0;
		}

		// reseta arquivo buffer
		filebuf = memset(filebuf, 0, MAX_SIZE);

		//incrementa numero de bytes escritos
		writtenBytes += readBytes;
	}
	printf("\n");

	free(filebuf);

	if (fclose(file) != 0) {
		printf("ERRO:impossivel fechar o arquivo.");
		printf("\n\n");
		return 0;
	}

	/* END */

	if (!sendControlPackage(fd, END, "0", ""))
    {
        return 0;
    }

	if (!llclose(fd, TRANSMITTER))
    {
        return 0;
	}

	printf("\n");
	printf("Arquivo transferido com sucesso.");
	printf("\n\n");

	return 1;
}


int sendControlPackage(int fd, const int C, char* fileSize, char* fileName){

	// calculate control package size
	int packageSize = 5 + strlen(fileSize) + strlen(fileName);
	unsigned int i = 0, pos = 0;

	// create control package
	unsigned char controlPackage[packageSize];
	controlPackage[pos++] = C;
	controlPackage[pos++] = T_FILE_SIZE;
	controlPackage[pos++] = strlen(fileSize);
	for (i = 0; i < strlen(fileSize); i++)
		controlPackage[pos++] = fileSize[i];
	controlPackage[pos++] = T_FILE_NAME;
	controlPackage[pos++] = strlen(fileName);
	for (i = 0; i < strlen(fileName); i++)
		controlPackage[pos++] = fileName[i];


    // Envio do pacote de controle
	if (!llwrite(fd,controlPackage, packageSize))
        {
            printf("Nao foi possivel gravar a camada de ligacao ao enviar pacote de controle. \n\n");
            free(controlPackage);
		return 0;
	}

	return 1;
}

int sendDataPackage(int fd, int N, const char * filebuf, int length){
	unsigned char C = DATA;
	unsigned char L2 = length / 256;
	unsigned char L1 = length % 256;

    //tamanho do pacote de dados
	unsigned int dataPackageSize = 4 + length;

    //criar pacote de dados
	unsigned char* dataPackage = (unsigned char*) malloc(dataPackageSize);
	dataPackage[0] = C;
	dataPackage[1] = N;
	dataPackage[2] = L2;
	dataPackage[3] = L1;

	memcpy(&dataPackage[4], filebuf, length);


	// write package
	if (!llwrite(fd, dataPackage, dataPackageSize)) {
		printf("Nao foi possivel enviar o pacote.\n");
		free(dataPackage);

		return 0;
	}

	free(dataPackage);

	return 1;
}

int receiveFile(const char * path){

	//Abrir conexao
	int fd = startConnection(path);
	if(fd == -1){
		printf("Cant find a connection.");
		return 0;
	}
	int a = llopen(fd, TRANSMITTER);
	if (a<=0)//verifica conexao
	{
		printf("llopen error");
		return(0);
	}


    char *filename;
    int controlStart, fileSize;

    if (!receiveControlPackage(fd, &controlStart, &fileSize, &filename))
    {
    	printf("APPLAYER: pacote de controlo nao recebido");
        return 0;
    }

    if (controlStart != START)
    {
        printf ("Erro: Pacote de controle recebido, mas seu campo de controle - %d - nao o C_PKG_START\n", controlStart);
        return 0;
    }

    //criar arquivo de sa�da
    FILE* outputFile = fopen("receivedfile", "wb");
    if(outputFile == NULL)
    {
        printf("Erro na criacao do arquivo de saida.");
        printf ("\n\n");
        return 0;
    }

    printf("Criado arquivo de saida: receivedfile\n");
	printf("Tamanho do aquivo: %d (bytes)\n", fileSize);

	int fileSizeRead = 0, x = -1;
	while (fileSizeRead != fileSize)
    {
        int lastx = x;
        char* filebuf = NULL;
        int length = 0;


		//	Recebem o pacote de datos e coloca os peda�os em buffer
		if(!receiveDataPackage(fd,&x, &filebuf, &length))
		{
			printf("Nao pode receber o pacote. \n");
			free(filebuf);
			return 0;
		}

		if (x != 0 && lastx+1 != x)
		{
			printf("Erro: Sequencia de numeros recebido foi %d em vez de %d", x, lastx+1);
			printf("\n\n");
			return 0;
		}
    }

    return 1;
}

int receiveControlPackage(int fd, int* controlPackageType, int* fileLength, char** fileName){
	// receive control package
	//TODO alterar esta cena
	unsigned char * package = malloc(MAX_SIZE);
	unsigned int totalSize = llread(fd, package);
	if (totalSize < 0) {
		printf("ERROR: Could not read from link layer while receiving control package.\n");
		return 0;
	}

	// process control package type
	*controlPackageType = package[0];

	if (*controlPackageType == END) {
		return 1;
	}

	unsigned int i = 0, numParams = 2, pos = 1, numOcts = 0;
	for (i = 0; i < numParams; i++) {
		int paramType = package[pos++];

		switch (paramType) {
		case T_FILE_SIZE: {
			numOcts = (unsigned int) package[pos++];

			char* length = malloc(numOcts);
			memcpy(length, &package[pos], numOcts);

			*fileLength = atoi(length);
			free(length);

			break;
		}
		case T_FILE_NAME:
			numOcts = (unsigned char) package[pos++];
			memcpy(*fileName, &package[pos], numOcts);

			break;
		}
	}

	return 1;
}

int receiveDataPackage(int fd, int* N, char** buf, int* length){
	unsigned char* package = malloc(MAX_SIZE * 2);

	// read package from link layer
	unsigned int size = llread(fd, package);
	if (size < 0) {
		printf(
				"ERROR: Could not read from link layer while receiving data package.\n");
		return 0;
	}

	int C = package[0];
	*N = (unsigned char) package[1];
	int L2 = package[2];
	int L1 = package[3];

	// assert package is a data package
	if (C != DATA) {
		printf("ERROR: Received package is not a data package (C = %d).\n", C);
		return 0;
	}

	// calculate size of the file chunk contained in the read package
	*length = 256 * L2 + L1;

	// allocate space for that file chunk
	*buf = malloc(*length);

	// copy file chunk to the buffer
	memcpy(*buf, &package[4], *length);

	// destroy the received package
	free(package);

	return 1;
}
