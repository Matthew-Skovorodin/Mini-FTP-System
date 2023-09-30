#include "myftp.h"

typedef struct ClientNode {
    char* hostname;
    int* connections;
    struct ClientNode* next;
} ClientInfo;

ClientInfo* head = NULL;

ClientInfo* addClient(char* address) {
    ClientInfo* connectionInfo = (ClientInfo*)malloc(sizeof(ClientInfo));
    connectionInfo->hostname = strdup(address);
    connectionInfo->connections = (int*)malloc(sizeof(int));
    *(connectionInfo->connections) = 1;
    connectionInfo->next = head;
    head = connectionInfo;
    return connectionInfo;
}

ClientInfo* findClient(char* address) {
    ClientInfo* seek = head;

    while (seek != NULL) {
        if (strcmp(address, seek->hostname) == 0)
            return seek;
        else
            seek = seek->next;
    }
    return seek;
}

void sendAck(char ack, int connectfd, char* ackstring) {
    char errorstring[256] = {0};

    if (ack == 'A') {
        write(connectfd, "A\n", 2);
    } else if (ack == 'E') {
        strcpy(errorstring, "E");
        strcat(errorstring, ackstring);
        strcat(errorstring, "\n");
        write(connectfd, errorstring, strlen(errorstring));
    }
}

char* getPath(int connectfd, char* pathString) {
    int numread;
    int i = 0;
    char buf[256] = {0};

    while (1) {
        numread = read(connectfd, buf, 1);
        if (numread == 0 || buf[0] == '\n')
            break;
        pathString[i] = buf[0];
        i++;
    }
    pathString[i] = '\0';
    return pathString;
}


int dataConnectServer(int connectfd) {
    int listenfd, dataconnectfd;
    char host[NI_MAXHOST], service[NI_MAXSERV];
    struct sockaddr_in dataAddr;
    int err;
    int myPort;
    char cmdstring[256] = {0};

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        sendAck('E', connectfd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        sendAck('E', connectfd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&dataAddr, 0, sizeof(dataAddr));
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_port = htons(0);
    dataAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr *)&dataAddr, sizeof(dataAddr)) < 0) {
        sendAck('E', connectfd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    socklen_t sockaddersize = sizeof(struct sockaddr_in);
    if (getsockname(listenfd, (struct sockaddr *)&dataAddr, &sockaddersize) < 0) {
        sendAck('E', connectfd, strerror(errno));
    }
    myPort = ntohs(dataAddr.sin_port);
    char portaddr[6];
    sprintf(portaddr, "%d", myPort);

    if (listen(listenfd, 1) < 0) {
        sendAck('E', connectfd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    socklen_t length = sizeof(struct sockaddr_in);

    strcpy(cmdstring, "A");
    strcat(cmdstring, portaddr);
    strcat(cmdstring, "\n");
    write(connectfd, cmdstring, strlen(cmdstring));

    dataconnectfd = accept(listenfd, (struct sockaddr *)&dataAddr, &length);
    if (dataconnectfd < 0) {
        sendAck('E', connectfd, strerror(errno));
        exit(EXIT_FAILURE);
    }

    err = getnameinfo((struct sockaddr *)&dataAddr, length, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
    if (err != 0) {
        fprintf(stderr, "Error: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    return dataconnectfd;
}

void list(int dataSocketfd, int connectfd) {
    if (fork()) {
        wait(NULL);
        close(dataSocketfd);
    } else {
        close(1);
        dup(dataSocketfd);
        close(dataSocketfd);
        execlp("ls", "ls", "-l", (char *)NULL);
        fprintf(stderr, "%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int changeDirserve(char *path, int connectfd) {
    char buf[510] = {0};
    int n;
    n = chdir(path);
    if (n == -1) {
        sendAck('E', connectfd, strerror(errno));
        return -1;
    }
    getcwd(buf, 510);
    fprintf(stdout, "%s\n", buf);
    return 0;
}

int getFile(int dataSocketfd, char *path, int connectfd) {
    int readnum = 0;
    char buf[512] = {0};
    char delim[] = "/";
    int fd;
    char *token;
    char *file = (char *)malloc(sizeof(char) * 256);
    memset(file, 0, 256);

    token = strtok(path, delim);
    while (token != NULL) {
        strcpy(file, token);
        token = strtok(0, delim);
    }
    struct stat area, *s = &area;

    if (access(file, R_OK | F_OK) < 0) {
        sendAck('E', connectfd, strerror(errno));
        return -1;
    }

    if (lstat(file, s) == 0) {
        if (S_ISLNK(s->st_mode)) {
            sendAck('E', connectfd, strerror(EISDIR));
            return -1;
        }
        if (S_ISDIR(s->st_mode)) {
            sendAck('E', connectfd, strerror(EISDIR));
            return -1;
        }
        if (S_ISREG(s->st_mode)) {
            fd = open(file, O_RDONLY);
            if (fd == -1) {
                sendAck('E', connectfd, strerror(errno));
                return -1;
            }

            while ((readnum = read(fd, buf, 512)) != 0) {
                write(dataSocketfd, buf, readnum);
            }
            close(dataSocketfd);
            free(file);
            close(fd);
            return 0;
        }
    }
    return -1;
}


int putserve(char *path, int datasocketfd, char *hostname, int connectfd) {
    int readnum = 0;
    char buf[512] = {0};
    int fd;
    if (access(path, R_OK | F_OK) == 0) {
        sendAck('E', connectfd, "File exists");
        return -1;
    } else
        sendAck('A', connectfd, NULL);

    fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0744);
    if (fd == -1) {
        sendAck('E', connectfd, strerror(errno));
        return -1;
    } else {
        while ((readnum = read(datasocketfd, buf, 500)) != 0) {
            write(fd, buf, readnum);
        }
    }
    close(fd);
    close(datasocketfd);
    return 0;
}

int main(void) {
    int listenfd, connectfd;
    char host[NI_MAXHOST], service[NI_MAXSERV];
    struct sockaddr_in servAddr, clientAddr;
    SortParams *clientInfo;
    int err;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, BACKLOG) < 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    socklen_t length = sizeof(struct sockaddr_in);

    while (1) {
        connectfd = accept(listenfd, (struct sockaddr *)&clientAddr, &length);
        if (connectfd < 0) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        err = getnameinfo((struct sockaddr *)&clientAddr, length, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
        if (err != 0) {
            fprintf(stderr, "Error: %s\n", gai_strerror(err));
            exit(EXIT_FAILURE);
        }

        clientInfo = findClient(host);
        if (clientInfo == NULL) {
            clientInfo = addClient(host);
        } else
            *clientInfo->connects = *clientInfo->connects + 1;

        fprintf(stdout, "%s %d\n", clientInfo->hostname, *clientInfo->connects);

        if (fork() == 0) {
            int numread;
            char buf[128] = {0};
            char clientString[128] = {0};
            int dataSocketfd;

            while ((numread = read(connectfd, buf, 1)) != 0) {
                if (buf[0] == 'D') {
                    dataSocketfd = dataConnectServer(connectfd);
                } else if (buf[0] == 'C') {
                    char *pathstring = (char *)malloc(256);
                    getPath(connectfd, pathstring);
                    strcpy(clientString, pathstring);
                    if (changeDirserve(clientString, connectfd) < 0) {
                        continue;
                    } else {
                        sendAck('A', connectfd, NULL);
                    }
                    close(dataSocketfd);
                    free(pathstring);
                } else if (buf[0] == 'L') {
                    if (fork()) {
                        wait(NULL);
                    } else {
                        list(dataSocketfd, connectfd);
                        sendAck('A', connectfd, NULL);
                        close(connectfd);
                        exit(EXIT_SUCCESS);
                    }
                    close(dataSocketfd);
                } else if (buf[0] == 'G') {
                    char *pathstring = (char *)malloc(256);
                    getPath(connectfd, pathstring);
                    strcpy(clientString, pathstring);
                    if (fork()) {
                        wait(NULL);
                    } else {
                        if (getFile(dataSocketfd, clientString, connectfd) < 0) {
                            close(connectfd);
                            exit(EXIT_FAILURE);
                        } else {
                            sendAck('A', connectfd, NULL);
                            close(connectfd);
                            exit(EXIT_SUCCESS);
                        }
                    }
                    free(pathstring);
                    close(dataSocketfd);
                } else if (buf[0] == 'P') {
                    char *pathstring = (char *)malloc(256);
                    getPath(connectfd, pathstring);
                    strcpy(clientString, pathstring);
                    if (fork()) {
                        wait(NULL);
                        close(dataSocketfd);
                    } else {
                        if (putserve(clientString, dataSocketfd, NULL, connectfd) < 0) {
                            close(connectfd);
                            exit(EXIT_FAILURE);
                        } else {
                            close(connectfd);
                            exit(EXIT_SUCCESS);
                        }
                    }
                    free(pathstring);
                    close(dataSocketfd);
                } else if (buf[0] == 'Q') {
                    sendAck('A', connectfd, NULL);
                    fprintf(stdout, "Received Exit command, closing child\n");
                    fflush(stdout);
                    exit(EXIT_SUCCESS);
                }
                memset(buf, 0, 128);
            }
            close(connectfd);
            exit(EXIT_SUCCESS);
        }
        close(connectfd);
    }
    return 0;
}