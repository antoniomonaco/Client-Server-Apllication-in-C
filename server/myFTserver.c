#include "server.h"

file_mutex *file_mutex_list = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s -a server_address -p server_port -d ft_root_directory\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *serverAddress = NULL;
    int serverPort = 0;
    char *rootDirectory = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "a:p:d:")) != -1) {
        switch (opt) {
            case 'a':
                serverAddress = optarg;
                break;
            case 'p':
                serverPort = atoi(optarg);
                break;
            case 'd':
                rootDirectory = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -a server_address -p server_port -d ft_root_directory\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!serverAddress || !serverPort || !rootDirectory) {
        fprintf(stderr, "Invalid arguments.\n");
        exit(EXIT_FAILURE);
    }

    createDirectoryIfNotExists(rootDirectory);

    int serverSocketFD = createIPv4Socket();
    struct sockaddr_in *serverAddr = createIPv4Address(serverAddress, serverPort);

    if (bind(serverSocketFD, (struct sockaddr *)serverAddr, sizeof(*serverAddr)) != 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket bound successfully!\n");

    if (listen(serverSocketFD, 10) != 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Started listening!\n");

    while (1) {
        struct sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);
        int clientSocketFD = accept(serverSocketFD, (struct sockaddr *)&clientAddress, &clientAddressSize);

        if (clientSocketFD < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t clientThread;
        client_thread_args *args = malloc(sizeof(client_thread_args));
        args->clientSocketFD = clientSocketFD;
        args->rootDirectory = rootDirectory;
        pthread_create(&clientThread, NULL, handleClient, (void*)args);
        //printf("New thread created: %lu\n", clientThread); // DEBUG
        pthread_detach(clientThread);
    }

    close(serverSocketFD);
    free(serverAddr);

    return 0;
}

int createIPv4Socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

struct sockaddr_in* createIPv4Address(char* ip, int port) {
    struct sockaddr_in *address = malloc(sizeof(struct sockaddr_in));
    address->sin_family = AF_INET;
    address->sin_port = htons(port);

    if (strlen(ip) == 0) {
        address->sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip, &address->sin_addr);
    }
    return address;
}
/*
Recupera il mutex associato a un file specifico. Se il mutex per il file non esiste, viene creato
*/
file_mutex* get_file_mutex(const char *filename) {
    pthread_mutex_lock(&list_mutex);
    file_mutex *fm = file_mutex_list;

    while (fm != NULL) {
        if (strcmp(fm->filename, filename) == 0) {
            pthread_mutex_unlock(&list_mutex);
            return fm;
        }
        fm = fm->next; //scorro tutti gli elementi della lista
    }

    fm = (file_mutex *)malloc(sizeof(file_mutex));
    fm->filename = strdup(filename);
    pthread_mutex_init(&fm->mutex, NULL);
    fm->next = file_mutex_list;
    file_mutex_list = fm;

    pthread_mutex_unlock(&list_mutex);
    return fm;
}

void lock_file(const char *filename) {
    file_mutex *fm = get_file_mutex(filename);
    pthread_mutex_lock(&fm->mutex);
}

void unlock_file(const char *filename) {
    file_mutex *fm = get_file_mutex(filename);
    pthread_mutex_unlock(&fm->mutex);
}
/*
Sceglie quale funzione utilizzare, in base alle informazioni ricevute dal client
*/
void* handleClient(void* args) {
    client_thread_args *clientArgs = (client_thread_args*)args;
    int clientSocketFD = clientArgs->clientSocketFD;
    char *rootDirectory = clientArgs->rootDirectory;

    char buffer[BUFFER_SIZE];
    int recv_result = recv(clientSocketFD, buffer, sizeof(buffer) - 1, 0);
    if (recv_result > 0) {
        buffer[recv_result] = '\0';

        char command[BUFFER_SIZE];
        char filePath[BUFFER_SIZE];
        sscanf(buffer, "%s %s", command, filePath);

        if (strcmp(command, "WRITE") == 0) {
            handleWrite(clientSocketFD, rootDirectory, buffer);
        } else if (strcmp(command, "READ") == 0) {
            handleRead(clientSocketFD, rootDirectory, filePath, buffer);
        } else if (strcmp(command, "LIST") == 0) {
            handleList(clientSocketFD, rootDirectory, filePath);
        } else {
            fprintf(stderr, "Invalid command.\n");
        }
    } else {
        perror("Receive failed");
    }

    close(clientSocketFD);
    free(clientArgs);
    return NULL;
}

/*
Si occupa di creare la root directory del server nel caso non esistesse
*/
void createDirectoryIfNotExists(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0700) != 0) {
            perror("Directory creation failed");
            exit(EXIT_FAILURE);
        }
    }
}

int check_disk_space(const char *path, size_t file_size) {
    struct statvfs stat;
    
    if (statvfs(path, &stat) != 0) {
        perror("statvfs failed");
        return 0;
    }
    
    unsigned long long free_space = stat.f_bsize * stat.f_bavail;
    return free_space >= file_size;
}


void handleWrite(int clientSocketFD, char *rootDirectory, char *buffer) {
    char command[BUFFER_SIZE];
    char filePath[BUFFER_SIZE];
    size_t file_size;
    sscanf(buffer, "%s %s %ld", command, filePath, &file_size);

    char fullPath[BUFFER_SIZE];
    // Calcola la lunghezza richiesta
    size_t rootDirLength = strlen(rootDirectory);
    size_t filePathLength = strlen(filePath);
    size_t requiredLength = rootDirLength + filePathLength + 2; // +2 per '/' e terminatore nullo '\0'

    // Verifica se la lunghezza richiesta è maggiore della dimensione del buffer
    if (requiredLength > BUFFER_SIZE) {
        char *msg = "ERROR: Path is too long\n";
        perror(msg);
        send(clientSocketFD, msg, strlen(msg), 0);
        return;
    }

    snprintf(fullPath, sizeof(fullPath), "%s/%s", rootDirectory, filePath);

    if (!check_disk_space(rootDirectory, file_size)) {
        char *msg = "ERROR: Not enough disk space\n";
        send(clientSocketFD, msg, strlen(msg), 0);
        return;
    }

    lock_file(fullPath);
    printf("Thread %ld: starting write operation\n", pthread_self());

    FILE *file = fopen(fullPath, "wb");
    if (!file) {
        perror("File open failed");
        char *error_msg = "ERROR: No such directory\n";
        send(clientSocketFD, error_msg, strlen(error_msg), 0);
        unlock_file(fullPath);
        return;
    }
    else{
        char *msg = "Directory found\n";
        send(clientSocketFD, msg, strlen(msg), 0);
    }

    ssize_t bytesReceived;
    size_t totalBytesReceived = 0;
    size_t totalBytesWritten = 0;
    while ((bytesReceived = recv(clientSocketFD, buffer, sizeof(buffer), 0)) > 0) {
        size_t bytesWritten = fwrite(buffer, 1, bytesReceived, file);
        if (bytesWritten < bytesReceived) {
            perror("Write to file failed");
            break; // Se non riesce a scrivere completamente, interrompi il ciclo
        }
        totalBytesReceived += bytesReceived;
        totalBytesWritten += bytesWritten;
    }

    if (bytesReceived < 0) {
        perror("Receive failed");
    }

    fclose(file);
    //DEBUG
    //printf("bytes received: %ld\nbytes written: %ld\n",totalBytesReceived,totalBytesWritten);

    printf("Thread %ld: finished write operation\n", pthread_self());

    unlock_file(fullPath);
}

void handleRead(int clientSocketFD, char *rootDirectory, char *remotePath, char *buffer_client) {
    char command[BUFFER_SIZE];
    char filePath[BUFFER_SIZE];
    int free_space; //spazio libero sul disco del client
    sscanf(buffer_client, "%s %s %d", command, filePath, &free_space);

    char fullPath[BUFFER_SIZE];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", rootDirectory, remotePath);

    lock_file(fullPath);
    printf("Thread %ld: starting read operation\n", pthread_self());

    FILE *file = fopen(fullPath, "rb");

    if (!file) {
        perror("File open failed");
        char *error_msg = "ERROR: File open failed: No such file or directory\n";
        send(clientSocketFD, error_msg, strlen(error_msg), 0);
        unlock_file(fullPath);
        return;
    }

    struct stat st;
    if (stat(fullPath, &st) != 0) {
        perror("Stat failed");
        fclose(file);
        return;
    }
    int fileSize = (int)st.st_size;

    if(fileSize > free_space){
        char *msg = "ERROR: Not enough disk space\n";
        send(clientSocketFD, msg, strlen(msg), 0);
        printf("%s",msg);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        size_t bytesSent = 0;
        while (bytesSent < bytesRead) {
            ssize_t result = send(clientSocketFD, buffer + bytesSent, bytesRead - bytesSent, 0);
            if (result < 0) {
                perror("Send failed");
                fclose(file);
                unlock_file(fullPath);
                return;
            }
            bytesSent += result;
        }
    }

    fclose(file);
    printf("Thread %ld: finished read operation\n", pthread_self());
    unlock_file(fullPath);
}

void handleList(int clientSocketFD, char *rootDirectory, char *remotePath) {
    char fullPath[BUFFER_SIZE];
    snprintf(fullPath, sizeof(fullPath), "%s%s", rootDirectory, remotePath);

    pthread_mutex_lock(&list_mutex); // Per inviare la lista dei file faccio il lock globalmente sulla directory
    printf("Thread %ld: starting list operation\n", pthread_self());

    DIR *dir = opendir(fullPath);
    if (!dir) {
        char *msg = "ERROR: Directory open failed\n";
        perror(msg);
        send(clientSocketFD, msg, strlen(msg), 0);
        pthread_mutex_unlock(&list_mutex);
        return;
    }

    struct dirent *entry;
    char buffer[BUFFER_SIZE];
    while ((entry = readdir(dir)) != NULL) {
        snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
        send(clientSocketFD, buffer, strlen(buffer), 0);
    }

    closedir(dir);
    printf("Thread %ld: finished list operation\n", pthread_self());
    pthread_mutex_unlock(&list_mutex); 
}
