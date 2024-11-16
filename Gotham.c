/*
@Author: Matéo Martin
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Protocol.h"
#include "Common.h"

#define MAX_PENDING_CONNECTIONS 5

typedef struct {
    char ip[128];
    int port;
    char workerType[16];
    int isAvailable; // 1 if active
} Worker;

Worker mediaWorker = {0};
Worker textWorker = {0};

// Send an error frame (TYPE: 0x09)
void sendErrorFrame(int clientSock) {
    Frame errorFrame = {0};
    errorFrame.type = 0x09;
    errorFrame.dataLength = 0;
    errorFrame.timestamp = time(NULL);
    errorFrame.checksum = calculateChecksum(&errorFrame);

    uint8_t buffer[FRAME_SIZE];
    serializeFrame(&errorFrame, buffer);
    write(clientSock, buffer, FRAME_SIZE);
}

// Handle Fleck connection request (TYPE: 0x01)
void handleFleckConnection(const Frame *receivedFrame, int clientSock) {
    char username[128], ip[128];
    int port;

    if (sscanf(receivedFrame->data, "%127[^&]&%127[^&]&%d", username, ip, &port) != 3) {
        fprintf(stderr, "Invalid Fleck connection data\n");
        sendErrorFrame(clientSock);
        return;
    }

    printf("New user connected: %s (IP: %s, Port: %d)\n", username, ip, port);

    Frame responseFrame = {0};
    responseFrame.type = 0x01; // Connection acknowledgment
    responseFrame.dataLength = 0; // No additional data
    responseFrame.timestamp = time(NULL);
    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t responseBuffer[FRAME_SIZE];
    serializeFrame(&responseFrame, responseBuffer);
    write(clientSock, responseBuffer, FRAME_SIZE);
}

void handleFleckRequest(const Frame *receivedFrame, int clientSock) {
    char mediaType[16] = {0};
    char fileName[128] = {0};

    // Log received frame data for debugging
    printf("Received distortion request: DataLength=%d, Data=%s\n", 
           receivedFrame->dataLength, receivedFrame->data);

    if (sscanf(receivedFrame->data, "%15[^&]&%127s", mediaType, fileName) != 2) {
        fprintf(stderr, "Invalid Fleck request data: %s\n", receivedFrame->data);
        sendErrorFrame(clientSock);
        return;
    }

    printf("Fleck requested distortion: MediaType=%s, FileName=%s\n", mediaType, fileName);

    Frame responseFrame = {0};
    responseFrame.type = 0x10;

    if (strcmp(mediaType, "Media") == 0 && mediaWorker.isAvailable) {
        snprintf(responseFrame.data, sizeof(responseFrame.data), "%s&%d", mediaWorker.ip, mediaWorker.port);
        responseFrame.dataLength = strlen(responseFrame.data);
    } else if (strcmp(mediaType, "Text") == 0 && textWorker.isAvailable) {
        snprintf(responseFrame.data, sizeof(responseFrame.data), "%s&%d", textWorker.ip, textWorker.port);
        responseFrame.dataLength = strlen(responseFrame.data);
    } else {
        snprintf(responseFrame.data, sizeof(responseFrame.data), "DISTORT_KO");
        responseFrame.dataLength = strlen(responseFrame.data);
    }

    responseFrame.timestamp = time(NULL);
    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t responseBuffer[FRAME_SIZE] = {0};
    serializeFrame(&responseFrame, responseBuffer);

    // Log outgoing response for debugging
    printf("Sending distortion response: DataLength=%d, Data=%s\n", 
           responseFrame.dataLength, responseFrame.data);

    write(clientSock, responseBuffer, FRAME_SIZE);
}

// Handle worker connections (TYPE: 0x02)
void handleWorkerConnection(const Frame *receivedFrame, int clientSock) {
    char workerType[16], ip[128];
    int port;

    if (sscanf(receivedFrame->data, "%15[^&]&%127[^&]&%d", workerType, ip, &port) != 3) {
        fprintf(stderr, "Invalid worker connection request data\n");
        sendErrorFrame(clientSock);
        return;
    }

    printf("New %s worker connected – ready to distort!\n", workerType);

    if (strcmp(workerType, "Media") == 0) {
        strcpy(mediaWorker.ip, ip);
        mediaWorker.port = port;
        strcpy(mediaWorker.workerType, workerType);
        mediaWorker.isAvailable = 1;
    } else if (strcmp(workerType, "Text") == 0) {
        strcpy(textWorker.ip, ip);
        textWorker.port = port;
        strcpy(textWorker.workerType, workerType);
        textWorker.isAvailable = 1;
    }

    Frame responseFrame = {0};
    responseFrame.type = 0x02;
    responseFrame.dataLength = 0;
    responseFrame.timestamp = time(NULL);
    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t responseBuffer[FRAME_SIZE];
    serializeFrame(&responseFrame, responseBuffer);
    write(clientSock, responseBuffer, FRAME_SIZE);
}

void *handleClient(void *arg) {
    int clientSock = *(int *)arg;
    free(arg);

    uint8_t buffer[FRAME_SIZE] = {0};
    ssize_t bytesRead = read(clientSock, buffer, FRAME_SIZE);

    // Log raw received frame
    printf("Received frame: BytesRead=%ld\n", bytesRead);

    if (bytesRead != FRAME_SIZE) {
        fprintf(stderr, "Invalid frame size received: %ld bytes (expected %d)\n", bytesRead, FRAME_SIZE);
        sendErrorFrame(clientSock);
        close(clientSock);
        return NULL;
    }

    Frame receivedFrame;
    deserializeFrame(buffer, &receivedFrame);

    // Log frame contents
    printf("Deserialized frame: Type=0x%02x, DataLength=%d, Checksum=0x%04x\n",
           receivedFrame.type, receivedFrame.dataLength, receivedFrame.checksum);

    if (calculateChecksum(&receivedFrame) != receivedFrame.checksum) {
        fprintf(stderr, "Checksum mismatch for frame of Type=0x%02x\n", receivedFrame.type);
        sendErrorFrame(clientSock);
        close(clientSock);
        return NULL;
    }

    // Handle the frame based on its type
    switch (receivedFrame.type) {
        case 0x01: // Fleck connection request
            handleFleckConnection(&receivedFrame, clientSock);
            break;
        case 0x10: // Fleck distortion request
            handleFleckRequest(&receivedFrame, clientSock);
            break;
        case 0x02: // Worker connection
            handleWorkerConnection(&receivedFrame, clientSock);
            break;
        default:
            fprintf(stderr, "Unknown frame type received: 0x%02x\n", receivedFrame.type);
            sendErrorFrame(clientSock);
            break;
    }

    close(clientSock);
    return NULL;
}


// Main Gotham logic
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Error: You need to provide a configuration file\n");
        return -1;
    }

    printf("Reading configuration file\n");
    Gotham *gotham = (Gotham *)readConfigFile(argv[1], "Gotham");

    if (gotham == NULL) {
        printf("Error: Could not load Gotham configuration\n");
        return -2;
    }

    // Initialize workers
    memset(&mediaWorker, 0, sizeof(Worker));
    memset(&textWorker, 0, sizeof(Worker));

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("Socket creation failed");
        free(gotham);
        return -3;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(gotham->fleckPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSock);
        free(gotham);
        return -4;
    }

    if (listen(serverSock, MAX_PENDING_CONNECTIONS) < 0) {
        perror("Listen failed");
        close(serverSock);
        free(gotham);
        return -5;
    }

    printf("Gotham server initialized\nWaiting for connections...\n");

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int *clientSock = malloc(sizeof(int)); // Allocate memory for the client socket
        *clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
        if (*clientSock < 0) {
            perror("Accept failed");
            free(clientSock);
            continue;
        }

        printf("New connection accepted from %s:%d\n",
               inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        pthread_t threadId;
        pthread_create(&threadId, NULL, handleClient, clientSock);
        pthread_detach(threadId); // Detach thread to automatically reclaim resources
    }

    close(serverSock);
    free(gotham);
    return 0;
}
