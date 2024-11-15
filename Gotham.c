/*
@author: Matéo Martin
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "Common.h"
#include "Protocol.h"

#include <arpa/inet.h>

void handleClient(int clientSock, const char *FleckIp, int FleckPort) {
    uint8_t buffer[FRAME_SIZE];
    ssize_t bytesRead = read(clientSock, buffer, FRAME_SIZE);
    if (bytesRead != FRAME_SIZE) {
        fprintf(stderr, "Invalid frame size received\n");
        close(clientSock);
        return;
    }

    Frame receivedFrame;
    deserializeFrame(buffer, &receivedFrame);

    // Validate checksum
    if (calculateChecksum(&receivedFrame) != receivedFrame.checksum) {
        fprintf(stderr, "Checksum mismatch\n");
        close(clientSock);
        return;
    }

    if (receivedFrame.type == 0x01) { // Connection Request
        char username[128], ip[128];
        int port;

        // Parse DATA field
        if (sscanf(receivedFrame.data, "%127[^&]&%127[^&]&%d", username, ip, &port) != 3) {
            fprintf(stderr, "Invalid connection request data\n");
            close(clientSock);
            return;
        }

        printf("Connection request from %s (IP: %s, Port: %d)\n", username, ip, port);

        // Create response frame
        Frame responseFrame = {0};
        responseFrame.type = 0x01;

        if (strcmp(ip, FleckIp) == 0 && port == FleckPort) { // Example validation
            responseFrame.dataLength = 0; // No data for OK response
        } else {
            snprintf(responseFrame.data, sizeof(responseFrame.data), "CON_KO");
            responseFrame.dataLength = strlen(responseFrame.data);
        }

        responseFrame.timestamp = time(NULL);
        responseFrame.checksum = calculateChecksum(&responseFrame);

        uint8_t responseBuffer[FRAME_SIZE];
        serializeFrame(&responseFrame, responseBuffer);
        write(clientSock, responseBuffer, FRAME_SIZE);

        printf("Response sent: %s\n", (responseFrame.dataLength == 0) ? "CON_OK" : "CON_KO");
    }

    close(clientSock);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printF("Error you need to give a configuration file");
        return -1;
    }
    
    Gotham* gotham = (Gotham*)readConfigFile(argv[1], "Gotham");
    
    if (gotham == NULL)
    {
        printF("GOTHAM NULL");
        return -2;
    }

    //DEBUG DISPLAY THE GOTHAM INFORMATIONS
    printF("Gotham Information:\n");
    char *message;
    asprintf(&message, "fleck ip address: %s\n", gotham->fleckIpAddress);
    printF(message);
    
    asprintf(&message, "fleck port: %d\n", gotham->fleckPort);
    printF(message);

    asprintf(&message, "harley/enigma ip address: %s\n", gotham->harleyEnigmaIpAddress);
    printF(message);

    asprintf(&message, "harley/enigma port: %d\n", gotham->harleyEnigmaPort);
    printF(message);
    
    free(message);


    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(gotham->fleckPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSock, 5) < 0) {
        perror("Listen failed");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    printf("Gotham server listening on port %d\n", gotham->fleckPort);

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
        if (clientSock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Accepted connection from %s:%d\n",
               inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        handleClient(clientSock, gotham->fleckIpAddress, gotham->fleckPort);
    }

    close(serverSock);

    
    free(gotham);



    return 0;
}
