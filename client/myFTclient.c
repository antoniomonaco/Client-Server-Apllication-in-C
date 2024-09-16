#include "client.h"

int main(int argc, char *argv[]) {
    char *ip = NULL;
    int port = 0;
    char *filePath = NULL;
    char *otherPath = NULL;
    char mode = 0;
    int opt;

    while ((opt = getopt(argc, argv, "wrla:p:f:o:")) != -1) {
        switch (opt) {
            case 'w':
            case 'r':
            case 'l':
                mode = opt;
                break;
            case 'a':
                ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                filePath = optarg;
                break;
            case 'o':
                otherPath = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -<w|r|l> -a server_address -p port -f <local_path|remote_path> [-o <remote_path|local_path>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!ip || port <= 0 || !filePath || mode == 0) {
        fprintf(stderr, "Missing required arguments.\n");
        exit(EXIT_FAILURE);
    }

    if (!otherPath) {
        if(mode == 'w'){
            // Vado ad eliminare il full path nel caso ci fosse
            char *fileName = strrchr(filePath, '/');
            if (fileName) {
                otherPath = fileName + 1; 
            } else {
                otherPath = filePath; // Se non c' è nessun '/' uso il filepath completo
            }
        }
        else{
            otherPath = filePath;
        }
    }


    int socketFD = createIPv4Socket();
    struct sockaddr_in *address = createIPv4Address(ip, port);

    if (connect(socketFD, (struct sockaddr*)address, sizeof(*address)) < 0) {
        perror("Connection failed");
        free(address);
        exit(EXIT_FAILURE);
    }

    if (mode == 'w') {
        handleWrite(socketFD, filePath, otherPath ? otherPath : filePath);
    } else if (mode == 'r') {
        handleRead(socketFD, filePath, otherPath ? otherPath : filePath);
    } else if (mode == 'l') {
        handleList(socketFD, filePath); 
    }

    close(socketFD);
    free(address);

    return 0;
}

int createIPv4Socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sock;
}

struct sockaddr_in* createIPv4Address(char* ip, int port) {
    struct sockaddr_in *address = malloc(sizeof(struct sockaddr_in));
    address->sin_family = AF_INET;
    address->sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &address->sin_addr.s_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        free(address);
        exit(EXIT_FAILURE);
    }

    return address;
}

void handleWrite(int socketFD, char *localPath, char *remotePath) {
    FILE *file = fopen(localPath, "rb");
    if (!file) {
        perror("File open failed");
        return;
    }

    struct stat st;
    if (stat(localPath, &st) != 0) {
        perror("Stat failed");
        fclose(file);
        return;
    }
    size_t fileSize = st.st_size;

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "WRITE %s %ld\n", remotePath, fileSize);
    //Per prima cosa informo il server dell'operazione che voglio svolgere
    if (send(socketFD, buffer, strlen(buffer), 0) < 0) {
        perror("Send failed");
        fclose(file);
        return;
    }

    char response[BUFFER_SIZE];
    ssize_t response_len = recv(socketFD, response, sizeof(response)-1, 0);
    if (response_len > 0) {
        response[response_len] = '\0';
        if (strncmp(response, "ERROR:", 6) == 0) {
            printf("%s", response);
            fclose(file);
            return;
        }
    }

    sleep(1); //attende un secondo per permettere al server di connettersi correttamente

    size_t bytesRead;
    size_t totalBytesRead = 0;
    size_t totalBytesSent = 0;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        size_t bytesSent = 0;
        while (bytesSent < bytesRead) {
            ssize_t result = send(socketFD, buffer + bytesSent, bytesRead - bytesSent, 0);
            /*
             buffer + bytesSent serve per evitare che se l'invio di un pacchetto si sia interrotto per qualche motivo,
             non proceda ad inviare il prossimo pacchetto, ma riprenda ad inviare il pacchetto dove lo aveva lasciato
             per poi passare ai prossimi una volta finito. Quando send restituisce un valore inferiore a quanto richiesto, 
             buffer + bytesSent sposta il puntatore nel buffer dei dati da inviare in modo da riprendere 
             l'invio dal punto in cui si è interrotto
            */
            if (result < 0 | result < bytesRead) {
                perror("Send failed");
                fclose(file);
                return;
            }

            bytesSent += result;
        }
        totalBytesRead += bytesRead;
        totalBytesSent += bytesSent;
    }
    if (totalBytesRead > totalBytesSent){
        perror("Send failed");
        fclose(file);   
        return;
    }

    //DEBUG
    //printf("bytes read: %ld\nbytes sent: %ld\n",totalBytesRead,totalBytesSent); 

    fclose(file);
}

int check_disk_space(const char *path) {
    struct statvfs stat;
    
    if (statvfs(path, &stat) != 0) {
        perror("statvfs failed");
        return 0;
    }
    
    unsigned long long free_space = stat.f_bsize * stat.f_bavail;
    return free_space;
}

void handleRead(int socketFD, char *remotePath, char *localPath) {
    char buffer[BUFFER_SIZE];
    int free_space = check_disk_space("./");
    snprintf(buffer, sizeof(buffer), "READ %s %d\n", remotePath,free_space);
    //Per prima cosa informo il server dell'operazione che voglio svolgere
    send(socketFD, buffer, strlen(buffer), 0);

    FILE *file = fopen(localPath, "wb");
    if (!file) {
        perror("ERROR: File open failed");
        return;
    }

    ssize_t bytesRead;
    while ((bytesRead = recv(socketFD, buffer, sizeof(buffer), 0)) > 0) {
        if (strncmp(buffer, "ERROR:", 6) == 0) {
            printf("%s", buffer);
            fclose(file);
            return;
        }
        fwrite(buffer, 1, bytesRead, file);
        
    }
    printf("File downloaded succesfully!\n");

    if (bytesRead < 0) {
        perror("Receive failed");
    }

    fclose(file);
}

void handleList(int socketFD, char *remotePath) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "LIST %s\n", remotePath);
    send(socketFD, buffer, strlen(buffer), 0);

    ssize_t bytesRead;
    while ((bytesRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
            if (strncmp(buffer, "ERROR:", 6) == 0) {
                printf("%s", buffer);
                return;
        }
        printf("%s", buffer);
    }

    if (bytesRead < 0) {
        perror("Receive failed");
    }
}