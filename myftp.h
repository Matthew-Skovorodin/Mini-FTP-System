#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>
#include <math.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MY_PORT_NUMBER 49999
#define MY_PORT_NUMBER_STRING "49999"
#define BACKLOG 3

typedef struct _ClientNode {
    char* hostname;
    int* connects;
    struct _ClientNode* next;
} SortParams;

void getClientInput(int socketfd, char* hostname); // Client side function that gets user input

void exitFTP(int connectionFD); // Exit the program and kill server and client

void changeDirectory(char* path);  // Return zero if successful or negative if error

int changeDirectoryServer(char* path, int connectfd); // Server side cd

int serverChangeDirectory(char* path, int connectionFD); // Change directory in the server

void listFiles(); // Build a pipe and pipe ls into more to print 20 items at a time

int serverList(int socketfd, char* hostname); // Same as ls but performed on directory server is running in and output to client

int getFileFromServer(char* path, int comSocketFD, char* hostname); // Gets a file from the server

int showFileContents(char* path, int comSocketFD, char* hostname); // Almost the same as get except the file's contents from the server side are shown to the client using more

int sendFileToServer(char* path, int comSocketFD, char* hostname); // Sends a file from the client to the server

int receiveFileOnServer(char* path, int dataSocketFD, char* hostname, int connectFD); // Receives a file from the client on the server

int acknowledge(char* ackString, int socketFD); // Client-side function that acknowledges stuff from the server

int sendFile(int dataSocketFD, char* path, int connectFD); // Server-side get file (sends a file to the client)

int establishDataConnection(int comSocketFD, char* hostname); // Data connection client side

int setupDataConnectionServer(int connectFD); // Setup a data connection for sending and receiving files

int connectToServer(char* hostname, char* portNumber); // Establish a connection with the server from the client for data or commands/controls

char* getClientPath(int connectFD, char* pathString); // Server-side gets the path from the client and removes all slashes to leave just the name

void sendAcknowledgement(char ack, int connectFD, char* ackString); // Server-side sends an acknowledgement