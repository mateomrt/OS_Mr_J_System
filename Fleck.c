/*
@Author: Matéo Martin
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "Protocol.h"
#include "Common.h"

int sockfd = -1; // Socket descriptor for Gotham connection
pthread_t workerThread; // Worker communication thread
bool workerActive = false; // Indicates if a worker thread is active

typedef struct {
    char workerIp[128];
    int workerPort;
} WorkerInfo;

typedef enum {
    FILE_TYPE_TEXT,
    FILE_TYPE_MEDIA
} FileType;

// Function declarations
bool isFileOfType(const char *filename, FileType type);
void listFiles(const char *directory, FileType type);
void sendConnectionRequest(const char *username, const char *ip, int port);
void handleServerResponse();
void sendLogoutRequest(const char *username);
void handleCommands(Fleck *user);
void sendFrame(int socket, const Frame *frame);
ssize_t readAll(int socket, uint8_t *buffer, size_t length);
void *workerCommunication(void *arg);
void sendDistortionRequest(const char *mediaType, const char *fileName);
void handleDistortionResponse();

// Check if a file is of the specified type
bool isFileOfType(const char *filename, FileType type) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }
    switch (type) {
        case FILE_TYPE_TEXT:
            return (strcasecmp(ext, ".txt") == 0);
        case FILE_TYPE_MEDIA:
            return (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mp3") == 0 ||
                    strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 ||
                    strcasecmp(ext, ".png") == 0);
        default:
            return false;
    }
}

// List files of a specific type
void listFiles(const char *directory, FileType type) {
    DIR *dir;
    struct dirent *entry;
    const char *typeStr = (type == FILE_TYPE_TEXT) ? "text" : "media";

    dir = opendir(directory);
    if (dir == NULL) {
        printf("Error: Cannot open directory %s\n", directory);
        return;
    }

    printf("Listing %s files:\n", typeStr);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && isFileOfType(entry->d_name, type)) {
            printf("- %s\n", entry->d_name);
        }
    }
    closedir(dir);
}

// Reliable frame sending
ssize_t sendAll(int socket, const uint8_t *buffer, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t sent = write(socket, buffer + totalSent, length - totalSent);
        if (sent <= 0) {
            perror("Socket write error");
            return sent;
        }
        totalSent += sent;
    }
    return totalSent;
}

// Reliable frame reading
ssize_t readAll(int socket, uint8_t *buffer, size_t length) {
    size_t totalRead = 0;
    while (totalRead < length) {
        ssize_t bytesRead = read(socket, buffer + totalRead, length - totalRead);
        if (bytesRead <= 0) {
            perror("Socket read error");
            return -1; // Return error on read failure
        }
        totalRead += bytesRead;
    }
    return totalRead;
}

// Send a frame to the server
void sendFrame(int socket, const Frame *frame) {
    uint8_t buffer[FRAME_SIZE] = {0};
    serializeFrame(frame, buffer);

    ssize_t bytesWritten = sendAll(socket, buffer, FRAME_SIZE);
    if (bytesWritten != FRAME_SIZE) {
        fprintf(stderr, "Error: Frame not fully sent (sent %ld bytes, expected %d)\n", bytesWritten, FRAME_SIZE);
    } else {
        printf("Frame sent successfully: Type=0x%02x, DataLength=%d, Checksum=0x%04x\n",
               frame->type, frame->dataLength, frame->checksum);
    }
}

// Send connection request to Gotham
void sendConnectionRequest(const char *username, const char *ip, int port) {
    Frame frame = {0};
    frame.type = 0x01; // Connection request frame type
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d", username, ip, port);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    printf("Sending connection request: Type=0x%02x, DataLength=%d, Data=%s, Checksum=0x%04x\n",
           frame.type, frame.dataLength, frame.data, frame.checksum);

    sendFrame(sockfd, &frame);
}

// Handle server response
void handleServerResponse() {
    uint8_t buffer[FRAME_SIZE];
    ssize_t bytesRead = read(sockfd, buffer, FRAME_SIZE);

    if (bytesRead != FRAME_SIZE) {
        fprintf(stderr, "Error: Invalid response frame size received\n");
        return;
    }

    Frame response;
    deserializeFrame(buffer, &response);

    if (response.type == 0x01 && response.dataLength == 0) {
        printf("Connected to Gotham.\n");
    } else if (response.type == 0x01) {
        printf("Connection failed: %s\n", response.data);
    } else {
        printf("Unexpected frame type received during connection.\n");
    }
}


// Send logout request
void sendLogoutRequest(const char *username) {
    Frame frame = {0};
    frame.type = 0x07; // Logout frame type
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s", username);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    printf("Sending logout request: Type=0x%02x, DataLength=%d, Data=%s, Checksum=0x%04x\n",
           frame.type, frame.dataLength, frame.data, frame.checksum);

    sendFrame(sockfd, &frame);
}

// Worker communication thread
void *workerCommunication(void *arg) {
    WorkerInfo *workerInfo = (WorkerInfo *)arg;

    int workerSock = socket(AF_INET, SOCK_STREAM, 0);
    if (workerSock < 0) {
        perror("Socket creation failed for worker");
        free(workerInfo);
        return NULL;
    }

    struct sockaddr_in workerAddr = {0};
    workerAddr.sin_family = AF_INET;
    workerAddr.sin_port = htons(workerInfo->workerPort);
    inet_pton(AF_INET, workerInfo->workerIp, &workerAddr.sin_addr);

    if (connect(workerSock, (struct sockaddr *)&workerAddr, sizeof(workerAddr)) < 0) {
        perror("Connection to worker failed");
        close(workerSock);
        free(workerInfo);
        return NULL;
    }

    printf("Connected to worker at %s:%d.\n", workerInfo->workerIp, workerInfo->workerPort);

    // Prepare TYPE: 0x03 frame with file metadata
    Frame fileRequestFrame = {0};
    fileRequestFrame.type = 0x03; // Worker connection with file metadata
    fileRequestFrame.timestamp = time(NULL);
    snprintf(fileRequestFrame.data, sizeof(fileRequestFrame.data), "%s&hello.txt&1024&<MD5SUM>&<factor>",
             "Arthur"); // Example data
    fileRequestFrame.dataLength = strlen(fileRequestFrame.data);
    fileRequestFrame.checksum = calculateChecksum(&fileRequestFrame);

    uint8_t buffer[FRAME_SIZE];
    serializeFrame(&fileRequestFrame, buffer);

    if (write(workerSock, buffer, FRAME_SIZE) < 0) {
        perror("Error sending file request to worker");
    } else {
        printf("File request sent to worker: Type=0x03, Data=%s\n", fileRequestFrame.data);
    }

    // Wait for worker's response
    if (readAll(workerSock, buffer, FRAME_SIZE) > 0) {
        Frame response;
        deserializeFrame(buffer, &response);

        if (response.type == 0x03 && response.dataLength == 0) {
            printf("Worker accepted the connection. Start file distortion.\n");
        } else if (response.type == 0x03) {
            printf("Worker rejected the connection: %s\n", response.data);
        } else {
            printf("Unexpected response from worker: Type=0x%02x\n", response.type);
        }
    } else {
        printf("Worker did not respond.\n");
    }

    close(workerSock);
    free(workerInfo);
    return NULL;
}


// Send distortion request
void sendDistortionRequest(const char *mediaType, const char *fileName) {
    Frame frame = {0};
    frame.type = 0x10; // Distortion request type
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    printf("Sending distortion request: Type=0x%02x, DataLength=%d, Data=%s, Checksum=0x%04x\n",
           frame.type, frame.dataLength, frame.data, frame.checksum);

    sendFrame(sockfd, &frame);
}

// Handle distortion response
void handleDistortionResponse() {
    uint8_t buffer[FRAME_SIZE] = {0};
    ssize_t bytesRead = readAll(sockfd, buffer, FRAME_SIZE);

    if (bytesRead != FRAME_SIZE) {
        fprintf(stderr, "Error: Invalid response frame size received (%ld bytes)\n", bytesRead);
        return;
    }

    Frame response;
    deserializeFrame(buffer, &response);

    if (response.type != 0x10) {
        printf("Unexpected frame type received.\n");
        return;
    }

    if (response.dataLength == 0 || strcmp(response.data, "DISTORT_KO") == 0) {
        printf("No workers available for this distortion type.\n");
        return;
    } else if (strcmp(response.data, "MEDIA_KO") == 0) {
        printf("Invalid media type for distortion.\n");
        return;
    }

    WorkerInfo *workerInfo = malloc(sizeof(WorkerInfo));
    if (workerInfo == NULL) {
        perror("Memory allocation failed");
        return;
    }

    if (sscanf(response.data, "%127[^&]&%d", workerInfo->workerIp, &workerInfo->workerPort) != 2) {
        printf("Invalid worker redirection data from Gotham.\n");
        free(workerInfo);
        return;
    }

    printf("Redirecting to worker at %s:%d\n", workerInfo->workerIp, workerInfo->workerPort);

    if (pthread_create(&workerThread, NULL, workerCommunication, workerInfo) != 0) {
        perror("Failed to create worker thread");
        free(workerInfo);
    } else {
        pthread_detach(workerThread); // Detach to allow main thread to continue
    }
}


// Handle user commands
void handleCommands(Fleck *user) {
    char *command;

    while (1) {
        printF("$ ");

        command = readUntil(STDIN_FILENO, '\n');
        
        if (command == NULL || strlen(command) == 0) {
            if (command != NULL) {
                free(command);
            }
            continue;
        }
        

        if (strcasecmp(command, "CONNECT") == 0) {
            if (sockfd == -1) {
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0) {
                    perror("Socket creation failed");
                    free(command);
                    continue;
                }
                struct sockaddr_in serverAddr = {0};
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(user->port);
                inet_pton(AF_INET, user->ipAddress, &serverAddr.sin_addr);
                if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
                    perror("Connection to server failed");
                    close(sockfd);
                    sockfd = -1;
                } else {
                    sendConnectionRequest(user->name, user->ipAddress, user->port);
                    handleServerResponse();
                }
            } else {
                printf("Already connected to Gotham.\n");
            }
            
        } else if (strcasecmp(command, "LOGOUT") == 0) {
            if (sockfd != -1) {
                sendLogoutRequest(user->name);
                close(sockfd);
                sockfd = -1;
            }
        } else if (strncasecmp(command, "DISTORT", 7) == 0) {
            if (sockfd != -1) {
                char mediaType[16], fileName[128];
                printf("Enter media type (Text/Media): ");
                scanf("%15s", mediaType);
                printf("Enter file name: ");
                scanf("%127s", fileName);
                sendDistortionRequest(mediaType, fileName);
                handleDistortionResponse();
            } else {
                printf("You must connect to Gotham first.\n");
            }
            
        } else if (strcasecmp(command, "LIST MEDIA") == 0) {
            listFiles(user->userFile, FILE_TYPE_MEDIA);
        } else if (strcasecmp(command, "LIST TEXT") == 0) {
            listFiles(user->userFile, FILE_TYPE_TEXT);
        } else {
            printf("Unknown command.\n");
        }

        free(command);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Error: You need to provide a configuration file\n");
        return -1;
    }

    Fleck *user = (Fleck *)readConfigFile(argv[1], "Fleck");
    if (user != NULL) {
        char *message;
        asprintf(&message, "%s user initialized\n", user->name);
        printF(message);
        free(message);
        handleCommands(user);
        free(user);
    }

    return 0;
}
