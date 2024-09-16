#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/statvfs.h>

#define BUFFER_SIZE 4096

typedef struct {
    int clientSocketFD;
    char *rootDirectory;
} client_thread_args;

typedef struct file_mutex {
    char *filename;
    pthread_mutex_t mutex;
    struct file_mutex *next;
} file_mutex;

extern file_mutex *file_mutex_list;
extern pthread_mutex_t list_mutex;

int createIPv4Socket();
struct sockaddr_in* createIPv4Address(char* ip, int port);
void* handleClient(void* args);
void createDirectoryIfNotExists(const char *dir);
void handleWrite(int clientSocketFD, char *rootDirectory, char *buffer);
void handleRead(int clientSocketFD, char *rootDirectory, char *remotePath, char *buffer_client);
void handleList(int clientSocketFD, char *rootDirectory, char *remotePath);
file_mutex* get_file_mutex(const char *filename);
void lock_file(const char *filename);
void unlock_file(const char *filename);

#endif // SERVER_H
