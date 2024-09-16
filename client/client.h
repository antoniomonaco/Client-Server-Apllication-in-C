#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#define BUFFER_SIZE 4096

int createIPv4Socket();
struct sockaddr_in* createIPv4Address(char* ip, int port);
void handleWrite(int socketFD, char *localPath, char *remotePath);
void handleRead(int socketFD, char *remotePath, char *localPath);
void handleList(int socketFD, char *remotePath);

#endif // CLIENT_H