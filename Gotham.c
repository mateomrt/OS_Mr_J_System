/* Gotham.c */
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
#define FRAME_SIZE 256

typedef struct {
    char ip[128];
    int port;
    char workerType[16];
    int isAvailable; // 1 if active
} Worker;

Worker mediaWorker = {0};
Worker textWorker = {0};

pthread_mutex_t workerMutex = PTHREAD_MUTEX_INITIALIZER;

// Function declarations
void *handleClient(void *arg);
void *serverThread(void *arg);

// Helper struct to pass server socket and thread context
typedef struct {
    int serverSock;
    const char *serverType;
} ServerContext;

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

// Read all bytes from a socket
ssize_t readAll(int socket, uint8_t *buffer, size_t length) {
    size_t totalRead = 0;
    while (totalRead < length) {
        ssize_t bytesRead = read(socket, buffer + totalRead, length - totalRead);
        if (bytesRead <= 0) {
            return bytesRead; // Error or disconnect
        }
        totalRead += bytesRead;
    }
    return totalRead;
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

    pthread_mutex_lock(&workerMutex);

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

    pthread_mutex_unlock(&workerMutex);

    Frame responseFrame = {0};
    responseFrame.type = 0x02; // Worker connection acknowledgment
    responseFrame.dataLength = 0;
    responseFrame.timestamp = time(NULL);
    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t responseBuffer[FRAME_SIZE];
    serializeFrame(&responseFrame, responseBuffer);
    write(clientSock, responseBuffer, FRAME_SIZE);
}

// Handle Fleck distortion request (TYPE: 0x10)
void handleFleckRequest(const Frame *receivedFrame, int clientSock) {
    char mediaType[16] = {0};
    char fileName[128] = {0};

    printf("Handling distortion request: Data=%s\n", receivedFrame->data);

    if (sscanf(receivedFrame->data, "%15[^&]&%127s", mediaType, fileName) != 2) {
        fprintf(stderr, "Failed to parse distortion request data\n");
        Frame responseFrame = {0};
        responseFrame.type = 0x10;
        responseFrame.timestamp = time(NULL);
        snprintf(responseFrame.data, sizeof(responseFrame.data), "MEDIA_KO");
        responseFrame.dataLength = strlen(responseFrame.data);
        responseFrame.checksum = calculateChecksum(&responseFrame);

        uint8_t responseBuffer[FRAME_SIZE];
        serializeFrame(&responseFrame, responseBuffer);
        write(clientSock, responseBuffer, FRAME_SIZE);
        return;
    }

    Frame responseFrame = {0};
    responseFrame.type = 0x10;

    pthread_mutex_lock(&workerMutex);

    // Check for a matching worker
    if (strcmp(mediaType, "Media") == 0) {
        if (mediaWorker.isAvailable) {
            snprintf(responseFrame.data, sizeof(responseFrame.data), "%s&%d", mediaWorker.ip, mediaWorker.port);
            responseFrame.dataLength = strlen(responseFrame.data);
        } else {
            snprintf(responseFrame.data, sizeof(responseFrame.data), "DISTORT_KO");
            responseFrame.dataLength = strlen(responseFrame.data);
        }
    } else if (strcmp(mediaType, "Text") == 0) {
        if (textWorker.isAvailable) {
            snprintf(responseFrame.data, sizeof(responseFrame.data), "%s&%d", textWorker.ip, textWorker.port);
            responseFrame.dataLength = strlen(responseFrame.data);
        } else {
            snprintf(responseFrame.data, sizeof(responseFrame.data), "DISTORT_KO");
            responseFrame.dataLength = strlen(responseFrame.data);
        }
    } else {
        snprintf(responseFrame.data, sizeof(responseFrame.data), "MEDIA_KO");
        responseFrame.dataLength = strlen(responseFrame.data);
    }

    pthread_mutex_unlock(&workerMutex);

    responseFrame.timestamp = time(NULL);
    responseFrame.checksum = calculateChecksum(&responseFrame);

    uint8_t responseBuffer[FRAME_SIZE];
    serializeFrame(&responseFrame, responseBuffer);
    write(clientSock, responseBuffer, FRAME_SIZE);

    printf("Distortion response sent: %s\n", responseFrame.data);
}

// Handle Fleck connection (TYPE: 0x01)
void handleFleckConnection(const Frame *receivedFrame, int clientSock) {
    char username[128], ip[128];
    int port;

    if (sscanf(receivedFrame->data, "%127[^&]&%127[^&]&%d", username, ip, &port) != 3) {
        fprintf(stderr, "Invalid Fleck connection data\n");
        Frame responseFrame = {0};
        responseFrame.type = 0x01;
        responseFrame.timestamp = time(NULL);
        snprintf(responseFrame.data, sizeof(responseFrame.data), "CON_KO");
        responseFrame.dataLength = strlen(responseFrame.data);
        responseFrame.checksum = calculateChecksum(&responseFrame);

        uint8_t responseBuffer[FRAME_SIZE];
        serializeFrame(&responseFrame, responseBuffer);
        write(clientSock, responseBuffer, FRAME_SIZE);
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

// Handle client frames
void handleClientFrame(const Frame *receivedFrame, int clientSock) {
    switch (receivedFrame->type) {
        case 0x01: // Fleck connection
            handleFleckConnection(receivedFrame, clientSock);
            break;
        case 0x10: // Distortion request
            handleFleckRequest(receivedFrame, clientSock);
            break;
        case 0x02: // Worker connection
            handleWorkerConnection(receivedFrame, clientSock);
            break;
        case 0x07: // Disconnection
            printf("Client disconnected: %s\n", receivedFrame->data);
            break;
        default:
            fprintf(stderr, "Unknown frame type received: 0x%02x\n", receivedFrame->type);
            sendErrorFrame(clientSock);
    }
}

// **Restored Function: handleClient**
void *handleClient(void *arg) {
    int clientSock = *(int *)arg;
    free(arg);

    uint8_t buffer[FRAME_SIZE];
    while (1) {
        ssize_t bytesRead = readAll(clientSock, buffer, FRAME_SIZE);
        if (bytesRead <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        Frame receivedFrame;
        deserializeFrame(buffer, &receivedFrame);
        if (calculateChecksum(&receivedFrame) != receivedFrame.checksum) {
            fprintf(stderr, "Checksum mismatch\n");
            sendErrorFrame(clientSock);
            continue;
        }

        handleClientFrame(&receivedFrame, clientSock);
    }

    close(clientSock);
    return NULL;
}

// Worker thread for handling incoming connections
void *serverThread(void *arg) {
    ServerContext *context = (ServerContext *)arg;

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int *clientSock = malloc(sizeof(int));
        *clientSock = accept(context->serverSock, (struct sockaddr *)&clientAddr, &addrLen);
        if (*clientSock < 0) {
            perror("Accept failed");
            free(clientSock);
            continue;
        }

        printf("[%s] New connection accepted from %s:%d\n",
               context->serverType,
               inet_ntoa(clientAddr.sin_addr),
               ntohs(clientAddr.sin_port));

        pthread_t threadId;
        pthread_create(&threadId, NULL, handleClient, clientSock);
        pthread_detach(threadId);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Error: You need to provide a configuration file\n");
        return -1;
    }

    // Read Gotham configuration
    printf("Reading configuration file\n");
    Gotham *gotham = (Gotham *)readConfigFile(argv[1], "Gotham");

    if (gotham == NULL) {
        printf("Error: Could not load Gotham configuration\n");
        return -2;
    }

    printf("Configuration loaded: Fleck Port=%d, Worker Port=%d\n", gotham->fleckPort, gotham->harleyEnigmaPort);

    // Create Fleck server socket
    int fleckSock = socket(AF_INET, SOCK_STREAM, 0);
    if (fleckSock < 0) {
        perror("Socket creation failed for Fleck");
        free(gotham);
        return -3;
    }

    struct sockaddr_in fleckAddr = {0};
    fleckAddr.sin_family = AF_INET;
    fleckAddr.sin_port = htons(gotham->fleckPort);
    fleckAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fleckSock, (struct sockaddr *)&fleckAddr, sizeof(fleckAddr)) < 0) {
        perror("Bind failed for Fleck");
        close(fleckSock);
        free(gotham);
        return -4;
    }

    if (listen(fleckSock, MAX_PENDING_CONNECTIONS) < 0) {
        perror("Listen failed for Fleck");
        close(fleckSock);
        free(gotham);
        return -5;
    }
    printf("Fleck server listening...\n");

    // Create Enigma/Harley server socket
    int workerSock = socket(AF_INET, SOCK_STREAM, 0);
    if (workerSock < 0) {
        perror("Socket creation failed for Worker");
        close(fleckSock);
        free(gotham);
        return -6;
    }

    struct sockaddr_in workerAddr = {0};
    workerAddr.sin_family = AF_INET;
    workerAddr.sin_port = htons(gotham->harleyEnigmaPort);
    workerAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(workerSock, (struct sockaddr *)&workerAddr, sizeof(workerAddr)) < 0) {
        perror("Bind failed for Worker");
        close(fleckSock);
        close(workerSock);
        free(gotham);
        return -7;
    }

    if (listen(workerSock, MAX_PENDING_CONNECTIONS) < 0) {
        perror("Listen failed for Worker");
        close(fleckSock);
        close(workerSock);
        free(gotham);
        return -8;
    }
    printf("Worker server listening...\n");

    // Start threads for Fleck and Enigma/Harley
    ServerContext fleckContext = {fleckSock, "Fleck"};
    ServerContext workerContext = {workerSock, "Worker"};

    pthread_t fleckThread, workerThread;
    if (pthread_create(&fleckThread, NULL, serverThread, &fleckContext) != 0) {
        perror("Failed to create Fleck server thread");
        close(fleckSock);
        close(workerSock);
        free(gotham);
        return -9;
    }

    if (pthread_create(&workerThread, NULL, serverThread, &workerContext) != 0) {
        perror("Failed to create Worker server thread");
        pthread_cancel(fleckThread); // Ensure the other thread is cleaned up
        close(fleckSock);
        close(workerSock);
        free(gotham);
        return -10;
    }

    // Join threads to ensure the program does not exit prematurely
    pthread_join(fleckThread, NULL);
    pthread_join(workerThread, NULL);

    close(fleckSock);
    close(workerSock);
    free(gotham);

    return 0;
}
