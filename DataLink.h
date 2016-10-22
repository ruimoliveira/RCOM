#ifndef LINKLAYER_H
#define LINKLAYER_H

int llopen(const char * path, int type);
int sendPacket(int fd, char * buffer, int length);
int receivePacket(int fd);
int llwrite(int fd, char * buffer, int length);
int llclose (int fd, int type);

#endif
