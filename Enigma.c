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

// Send a response to Fleck (ACK or NACK)
void sendResponseToFleck(int clientSock, bool isSuccess) {
    Frame responseFrame = {0};
    responseFrame.type = 0x03; // Response to distortion request
    responseFrame.timestamp = time(NULL);

    if (isSuccess) {
        responseFrame.dataLength = 0; // ACK has no data
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
    printf("Processing distortion request from Fleck...\n");

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
        printf("Distortion process simulated. Sent ACK to Fleck.\n");
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

    printf("Enigma is listening for distortion requests on port %d...\n", enigmaPort);

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

        uint8_t buffer[FRAME_SIZE] = {0};
        ssize_t bytesRead = read(clientSock, buffer, FRAME_SIZE);
        if (bytesRead != FRAME_SIZE) {
            fprintf(stderr, "Invalid frame size received\n");
            close(clientSock);
            continue;
        }

        Frame receivedFrame = {0};
        deserializeFrame(buffer, &receivedFrame);

        if (calculateChecksum(&receivedFrame) != receivedFrame.checksum) {
            fprintf(stderr, "Checksum mismatch\n");
            close(clientSock);
            continue;
        }

        if (receivedFrame.type == 0x03) { // Distortion request
            handleDistortionRequest(&receivedFrame, clientSock);
        } else {
            fprintf(stderr, "Unexpected frame type received\n");
        }

        close(clientSock);
    }

    close(serverSock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Error: You need to provide a configuration file\n");
        return -1;
    }

    printf("Reading configuration file\n");
    Enigma *enigma = (Enigma *)readConfigFile(argv[1], "Enigma");

    if (enigma == NULL) {
        printf("Error: Could not load Enigma configuration\n");
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
    printf("Connected to Gotham as Enigma worker, ready to distort text.\n");

    pthread_t workerThread;
    pthread_create(&workerThread, NULL, enigmaWorkerLoop, &enigma->fleckPort);

    pthread_join(workerThread, NULL);

    sendDisconnectionRequest(enigma->workerType);
    printf("Disconnecting from Gotham.\n");

    close(sockfd);
    free(enigma);
    return 0;
}
