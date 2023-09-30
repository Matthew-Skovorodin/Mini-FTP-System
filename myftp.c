/*
Project: Miniature FTP System
Author: Matthew Skovorodin
Description: This project comprises two C programs, myftpserve (server) and myftp (client), utilizing Unix TCP/IP sockets for seamless communication.
The code implements FTP commands like cd, ls, get, put, and more
*/

#include "myftp.h"

// Function to read acknowledgements from the server and parse them for information
int receiveAcknowledge(char* ackString, int socketfd) {
    char serverAck;
    char serverAckString[128] = {0};
    char BUF[128] = {0};
    int i = 0;

    while (1) {
        if (read(socketfd, BUF, 1) == 0 || BUF[0] == '\n')
            break;
        serverAckString[i] = BUF[0];
        i++;
    }

    serverAckString[i] = '\0';

    // Parsing the server acknowledgment
    sscanf(serverAckString, "%c %[^\n]", &serverAck, ackString);

    if (serverAck == 'E') {
        fprintf(stderr, "%s\n", ackString);
        fflush(stderr);
        return -1;
    }
    return 0;
}

// Function to exit the FTP session
void exitFTP(int socketfd) {
    char* ackStringExit = (char*)malloc(sizeof(char) * 256);
    write(socketfd, "Q\n", 2);

    if (receiveAcknowledge(ackStringExit, socketfd) < 0) {
        free(ackStringExit);
        exit(EXIT_FAILURE);
    }

    free(ackStringExit);
    exit(EXIT_SUCCESS);
}

// Function to establish connections with the server
int connectToServer(char* hostname, char* portnumber) {
    int socketfd;
    struct addrinfo hints, *actualData;
    memset(&hints, 0, sizeof(hints));
    int err;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    err = getaddrinfo(hostname, portnumber, &hints, &actualData);
    if (err != 0) {
        fprintf(stderr, "Error: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    socketfd = socket(actualData->ai_family, actualData->ai_socktype, 0);
    if (socketfd < 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (connect(socketfd, actualData->ai_addr, actualData->ai_addrlen) < 0) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (receiveAcknowledge(NULL, socketfd) < 0) {
        fprintf(stderr, "Acknowledge error\n");
    }

    return socketfd;
}

// Function to establish a data connection with the server
int dataConnect(int comsocketfd, char* hostname) {
    int dataSocketfd;
    char* ackString = (char*)malloc(sizeof(char) * 256);
    memset(ackString, 0, 256);

    // Send D and wait for the server response using acknlg
    write(comsocketfd, "D\n", 2);
    if (acknlg(ackString, comsocketfd) == 0) {
        dataSocketfd = serverconnect(hostname, ackString);
        free(ackString);
        return dataSocketfd;
    } else {
        free(ackString);
        return -1;
    }
}

// Function to change directories on the server side
int servercd(char* path, int socketfd) {
    char cmdbuf[256];
    char* acknlgString = (char*)malloc(sizeof(char) * 256);

    strcpy(cmdbuf, "C");
    strcat(cmdbuf, path);
    strcat(cmdbuf, "\n");
    write(socketfd, cmdbuf, strlen(cmdbuf));
    if (acknlg(acknlgString, socketfd) < 0) {
        free(acknlgString);
        return -1;
    }
    free(acknlgString);
    return 0;
}

// Function to change directories locally on the client side
void changeDir(char* path) {
    char buf[510];
    int n;
    n = chdir(path);
    if (n == -1)
        fprintf(stdout, "%s\n", strerror(errno));
    getcwd(buf, 510);
    fprintf(stdout, "%s\n", buf);
}

// Function to run 'ls -l' on the server and display 20 lines at a time using 'more'
int serverls(int comsocketfd, char* hostname) {
    char* acknlgStringRls = (char*)malloc(sizeof(char) * 256);
    int dataSocketfd;
    dataSocketfd = dataConnect(comsocketfd, hostname);
    if (dataSocketfd < 0) {
        return -1;
    }

    write(comsocketfd, "L\n", 2);
    if (acknlg(acknlgStringRls, comsocketfd) < 0) {
        close(dataSocketfd);
        free(acknlgStringRls);
        return -1;
    }

    if (fork()) {
        wait(NULL);
        close(dataSocketfd);
    } else {
        close(0);
        dup(dataSocketfd);
        close(dataSocketfd);
        execlp("more", "more", "-20", (char*)NULL);
        fprintf(stderr, "%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    free(acknlgStringRls);
    close(dataSocketfd);
    return 0;
}

// Function to run 'ls -l' client-side in the current directory, piping it to 'more' to show only 20 lines at a time
void list() {
    if (fork()) {
        wait(NULL);
    } else {
        int fd[2];
        int rdr, wtr;

        if (pipe(fd) < 0)
            perror("Pipe error");
        else
            perror("Pipe success");

        rdr = fd[0];
        wtr = fd[1];

        if (fork()) {
            close(wtr);
            wait(NULL);
            close(0);
            dup(rdr);
            close(rdr);
            execlp("more", "more", "-20", (char*)NULL);
            fprintf(stderr, "%s\n", strerror(errno));
        } else {
            close(rdr);
            close(1);
            dup(wtr);
            execlp("ls", "ls", "-l", (char*)NULL);
            fprintf(stderr, "%s\n", strerror(errno));
            close(wtr);
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_SUCCESS);
    }
}

// Function to get a file from the server
int get(char* path, int comsocketfd, char* hostname) {
    int dataSocketfd;
    int readnum = 0;
    char buf[512];
    char cmdbuf[256];
    char* ackStringGet = (char*)malloc(sizeof(char) * 256);

    dataSocketfd = dataConnect(comsocketfd, hostname);
    if (dataSocketfd < 0) {
        free(ackStringGet);
        return -1;
    }

    strcpy(cmdbuf, "G");
    strcat(cmdbuf, path);
    strcat(cmdbuf, "\n");
    write(comsocketfd, cmdbuf, strlen(cmdbuf));

    if (acknlg(ackStringGet, comsocketfd) < 0) {
        free(ackStringGet);
        return -1;
    }

    char delim[] = "/";
    int fd;
    char* token;
    char* file = (char*)malloc(sizeof(char) * 256);
    token = strtok(path, delim);

    while (token != NULL) {
        strcpy(file, token);
        token = strtok(0, delim);
    }

    if (access(file, R_OK | F_OK) == 0) {
        fprintf(stderr, "Error: File exists\n");
        free(ackStringGet);
        free(file);
        return -1;
    }

    fd = open(file, O_RDWR | O_CREAT | O_EXCL, 0744);
    if (fd == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        free(ackStringGet);
        free(file);
        return -1;
    } else {
        while ((readnum = read(dataSocketfd, buf, 512)) != 0) {
            write(fd, buf, readnum);
        }
    }

    close(dataSocketfd);
    free(ackStringGet);
    free(file);
    return 0;
}

// Function to send a file from the client and put it on the server
int put(char* path, int comsocketfd, char* hostname) {
    int dataSocketfd;
    int readnum = 0;
    char buf[512] = {0};
    char cmdbuf[256] = {0};
    char delim[] = "/";
    char* ackStringPut = (char*)malloc(sizeof(char) * 256);
    memset(ackStringPut, 0, 256);

    int fd;
    char* token;
    char* file = (char*)malloc(sizeof(char) * 256);
    memset(file, 0, 256);

    token = strtok(path, delim);
    while (token != NULL) {
        strcpy(file, token);
        token = strtok(0, delim);
    }

    struct stat area, *s = &area;
    if (access(file, R_OK | F_OK) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        free(ackStringPut);
        free(file);
        return -1;
    }

    if (lstat(file, s) == 0) {
        if (S_ISLNK(s->st_mode))
            return -1;
        if (S_ISDIR(s->st_mode)) {
            fprintf(stderr, "Error: %s\n", strerror(EISDIR));
            free(ackStringPut);
            free(file);
            return -1;
        }
        if (S_ISREG(s->st_mode)) {
            fd = open(file, O_RDONLY);
            if (fd == -1) {
                fprintf(stderr, "%s\n", strerror(errno));
                free(ackStringPut);
                free(file);
                return -1;
            }

            dataSocketfd = dataConnect(comsocketfd, hostname);
            if (dataSocketfd < 0) {
                free(ackStringPut);
                free(file);
                return -1;
            }

            strcpy(cmdbuf, "P");
            strcat(cmdbuf, file);
            strcat(cmdbuf, "\n");
            write(comsocketfd, cmdbuf, strlen(cmdbuf));
            if (acknlg(ackStringPut, comsocketfd) < 0) {
                free(ackStringPut);
                close(dataSocketfd);
                free(file);
                return -1;
            }

            while ((readnum = read(fd, buf, 512)) != 0) {
                write(dataSocketfd, buf, readnum);
            }
            close(dataSocketfd);
            free(ackStringPut);
            free(file);
            return 0;
        } else
            return -1;
    }
    return -1;
}

// Function to establish a data connection and display the contents with 'more' (20 lines at a time)
int show(char* path, int comsocketfd, char* hostname) {
    int dataSocketfd;
    char cmdbuf[256];
    char* ackStringShow = (char*)malloc(sizeof(char) * 256);

    dataSocketfd = dataConnect(comsocketfd, hostname);
    if (dataSocketfd < 0) {
        free(ackStringShow);
        return -1;
    }

    strcpy(cmdbuf, "G");
    strcat(cmdbuf, path);
    strcat(cmdbuf, "\n");
    write(comsocketfd, cmdbuf, strlen(cmdbuf));
    if (acknlg(ackStringShow, comsocketfd) < 0) {
        free(ackStringShow);
        close(dataSocketfd);
        return -1;
    }

    if (fork()) {
        wait(NULL);
        close(dataSocketfd);
    } else {
        close(0);
        dup(dataSocketfd);
        close(dataSocketfd);
        execlp("more", "more", "-20", (char*)NULL);
        fprintf(stderr, "%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    free(ackStringShow);
    return 0;
}

void getinput(int socketfd, char* hostname) {
    char buf[256];
    char* stringToken1;
    char* stringToken2;
    char delim[] = " \t\r\n\v\f";

    fprintf(stdout, "MFTP>  ");
    while (fgets(buf, 256, stdin)) {
        stringToken1 = strtok(buf, delim);
        if (stringToken1 != NULL) {
            if (strcmp(stringToken1, "exit") == 0) {
                exitftp(socketfd);
            } else if (strcmp(stringToken1, "cd") == 0) {
                stringToken2 = strtok(NULL, delim);
                changeDir(stringToken2);
            } else if (strcmp(stringToken1, "rcd") == 0) {
                stringToken2 = strtok(NULL, delim);
                if (fork()) {
                    wait(NULL);
                } else {
                    if (servercd(stringToken2, socketfd) < 0) {
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            } else if (strcmp(stringToken1, "ls") == 0) {
                list();
            } else if (strcmp(stringToken1, "rls") == 0) {
                stringToken2 = strtok(NULL, delim);
                if (fork()) {
                    wait(NULL);
                } else {
                    if (serverls(socketfd, hostname) < 0) {
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            } else if (strcmp(stringToken1, "get") == 0) {
                stringToken2 = strtok(NULL, delim);
                if (fork()) {
                    wait(NULL);
                } else {
                    if (get(stringToken2, socketfd, hostname) < 0) {
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            } else if (strcmp(stringToken1, "show") == 0) {
                stringToken2 = strtok(NULL, delim);
                if (fork()) {
                    wait(NULL);
                } else {
                    if (show(stringToken2, socketfd, hostname) < 0) {
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            } else if (strcmp(stringToken1, "put") == 0) {
                stringToken2 = strtok(NULL, delim);
                if (fork()) {
                    wait(NULL);
                } else {
                    if (put(stringToken2, socketfd, hostname) < 0) {
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            } else {
                fprintf(stdout, "Error: command %s is incorrect\n", stringToken1);
            }
        }
        fprintf(stdout, "MFTP>  ");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <hostname>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int comsocketfd = connectToServer(argv[1], MY_PORT_NUMBER_STRING);
    if (comsocketfd < 0) {
        fprintf(stderr, "Error: Could not connect to server\n");
        return EXIT_FAILURE;
    }

    getinput(comsocketfd, argv[1]);
    printf("All done!\n");
    close(comsocketfd); // Close the socket when done

    return EXIT_SUCCESS;
}