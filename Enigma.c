/*
@Author: Matéo Martin
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "Protocol.h"
#include "Common.h"

int sockfd = -1; // Socket for Gotham connection

// Send a connection request to Gotham
void sendConnectionRequest(const char *workerType, const char *ip, int port) {
    Frame frame = {0};
    frame.type = 0x02; // Worker connection frame
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d", workerType, ip, port);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    uint8_t buffer[FRAME_SIZE] = {0};
    serializeFrame(&frame, buffer);
    if (write(sockfd, buffer, FRAME_SIZE) < 0) {
        perror("Error sending connection request to Gotham");
    }
}

// Send a disconnection request to Gotham
void sendDisconnectionRequest(const char *workerType) {
    Frame frame = {0};
    frame.type = 0x07; // Disconnection frame
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s", workerType);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    uint8_t buffer[FRAME_SIZE] = {0};
    serializeFrame(&frame, buffer);
    if (write(sockfd, buffer, FRAME_SIZE) < 0) {
        perror("Error sending disconnection request to Gotham");
    }
}

void sendResponseToFleck(int clientSock, bool isSuccess) {
    Frame responseFrame = {0};
    responseFrame.type = 0x03; // Response to distortion request
    responseFrame.timestamp = time(NULL);

    if (isSuccess) {
        responseFrame.dataLength = 0;
    } else {
        snprintf(responseFrame.data, sizeof(responseFrame.data), "CON_KO");
        responseFrame.dataLength = strlen(responseFrame.data);
    }

    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t buffer[FRAME_SIZE] = {0};
    serializeFrame(&responseFrame, buffer);
    if (write(clientSock, buffer, FRAME_SIZE) < 0) {
        perror("Error sending response to Fleck");
    }
}

// Handle distortion requests from Fleck
void handleDistortionRequest(const Frame *receivedFrame, int clientSock) {
    char username[128] = {0};
    char fileName[128] = {0};
    char fileSize[32] = {0};
    char md5sum[33] = {0};
    char factor[32] = {0};

    // Parse the distortion request data
    if (sscanf(receivedFrame->data, "%127[^&]&%127[^&]&%31[^&]&%32[^&]&%31s",
               username, fileName, fileSize, md5sum, factor) != 5) {
        perror("Invalid distortion request data\n");
        return;
    }


    // Simulate a response to Fleck
    Frame responseFrame = {0};
    responseFrame.type = 0x03; // Distortion acknowledgment
    responseFrame.timestamp = time(NULL);
    responseFrame.dataLength = 0; // No additional data
    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t buffer[FRAME_SIZE] = {0};
    serializeFrame(&responseFrame, buffer);

    if (write(clientSock, buffer, FRAME_SIZE) < 0) {
        perror("Error sending distortion response to Fleck");
    } else {
        printf("Distortion process simulated. Sent ACK to %s.\n", username);
        
    }
}


// Handle incoming connections for distortion requests
void *enigmaWorkerLoop(void *arg) {
    int enigmaPort = *(int *)arg;

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("Socket creation failed for Enigma");
        return NULL;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(enigmaPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed for Enigma");
        close(serverSock);
        return NULL;
    }

    if (listen(serverSock, 5) < 0) {
        perror("Listen failed for Enigma");
        close(serverSock);
        return NULL;
    }

    printF("Waiting for connections...\n");

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);

        if (clientSock < 0) {
            perror("Accept failed");
            continue;
        }

        char *message;
        asprintf(&message, "Accepted connection from %s:%d\n",
               inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        printF(message);
        free(message);

        uint8_t buffer[FRAME_SIZE] = {0};
        ssize_t bytesRead = read(clientSock, buffer, FRAME_SIZE);
        if (bytesRead != FRAME_SIZE) {
            perror("Invalid frame size received\n");
            close(clientSock);
            continue;
        }

        Frame receivedFrame = {0};
        deserializeFrame(buffer, &receivedFrame);

        if (calculateChecksum(&receivedFrame) != receivedFrame.checksum) {
            perror("Checksum mismatch\n");
            close(clientSock);
            continue;
        }

        if (receivedFrame.type == 0x03) { // Distortion request
            handleDistortionRequest(&receivedFrame, clientSock);
        } else {
            perror("Unexpected frame type received\n");
        }

        close(clientSock);
    }

    close(serverSock);
    return NULL;
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF("Error: You need to provide a configuration file\n");
        return -1;
    }

    printF("Reading configuration file\n");
    Enigma *enigma = (Enigma *)readConfigFile(argv[1], "Enigma");

    if (enigma == NULL) {
        printF("Error: Could not load Enigma configuration\n");
        return -2;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        free(enigma);
        return -3;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(enigma->gothamPort);
    inet_pton(AF_INET, enigma->gothamIpAddress, &serverAddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection to Gotham failed");
        close(sockfd);
        free(enigma);
        return -4;
    }

    sendConnectionRequest(enigma->workerType, enigma->fleckIpAddress, enigma->fleckPort);
    printF("Connected to Gotham as Enigma worker, ready to distort text.\n");

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, enigmaWorkerLoop, &enigma->fleckPort);

    pthread_join(workerThread, NULL);

    sendDisconnectionRequest(enigma->workerType);
    printF("Disconnecting from Gotham.\n");

    close(sockfd);
    free(enigma);
    return 0;
}
