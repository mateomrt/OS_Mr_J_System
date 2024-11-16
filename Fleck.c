/*
@Author: Matéo Martin
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdio.h>
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

bool isFileOfType(const char *filename, FileType type);
void listFiles(const char *directory, FileType type);
void sendConnectionRequest(const char *username, const char *ip, int port);
void handleServerResponse();
void sendLogoutRequest(const char *username);
void handleCommands(Fleck *user);

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
    int fileCount = 0;
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
            fileCount++;
        }
    }
    closedir(dir);

    if (fileCount == 0) {
        printf("No %s files found.\n", typeStr);
    }
}

// Worker communication thread function
void *workerCommunication(void *arg) {
    WorkerInfo *workerInfo = (WorkerInfo *)arg;

    int workerSock = socket(AF_INET, SOCK_STREAM, 0);
    if (workerSock < 0) {
        perror("Socket creation failed for worker");
        free(workerInfo);
        workerActive = false;
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
        workerActive = false;
        return NULL;
    }

    printf("Connected to worker at %s:%d. Performing handshake...\n", workerInfo->workerIp, workerInfo->workerPort);

    // Send a simple handshake frame
    Frame handshakeFrame = {0};
    handshakeFrame.type = 0x03; // Distortion request frame
    handshakeFrame.timestamp = time(NULL);
    snprintf(handshakeFrame.data, sizeof(handshakeFrame.data), "HANDSHAKE");
    handshakeFrame.dataLength = strlen(handshakeFrame.data);
    handshakeFrame.checksum = calculateChecksum(&handshakeFrame);

    uint8_t buffer[FRAME_SIZE];
    serializeFrame(&handshakeFrame, buffer);
    if (write(workerSock, buffer, FRAME_SIZE) < 0) {
        perror("Error sending handshake to worker");
        close(workerSock);
        free(workerInfo);
        workerActive = false;
        return NULL;
    }

    // Read response
    ssize_t bytesRead = read(workerSock, buffer, FRAME_SIZE);
    if (bytesRead == FRAME_SIZE) {
        Frame response;
        deserializeFrame(buffer, &response);
        if (response.type == 0x03 && response.dataLength == 0) {
            printf("Worker responded: Handshake successful.\n");
        } else {
            printf("Worker responded with an error.\n");
        }
    } else {
        printf("No valid response from worker.\n");
    }

    close(workerSock);
    free(workerInfo);
    workerActive = false;
    return NULL;
}

// Send a distortion request to Gotham
void sendDistortionRequest(const char *mediaType, const char *fileName) {
    Frame frame = {0};
    frame.type = 0x10; // Distortion request frame type
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s&%s", mediaType, fileName);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    uint8_t buffer[FRAME_SIZE];
    serializeFrame(&frame, buffer);
    if (write(sockfd, buffer, FRAME_SIZE) < 0) {
        perror("Error sending distortion request");
    }
}

void handleDistortionResponse() {
    uint8_t buffer[FRAME_SIZE] = {0}; // Ensure buffer is zeroed
    ssize_t bytesRead = read(sockfd, buffer, FRAME_SIZE);

    if (bytesRead != FRAME_SIZE) {
        fprintf(stderr, "Error: Invalid response frame size received (%ld bytes)\n", bytesRead);
        return;
    }

    Frame response;
    deserializeFrame(buffer, &response);

    if (response.type != 0x10) {
        printf("Unexpected frame type received (%d).\n", response.type);
        return;
    }

    if (response.dataLength == 0) {
        printf("No valid response data from Gotham.\n");
        return;
    }

    if (strcmp(response.data, "DISTORT_KO") == 0) {
        printf("No workers available for this distortion type.\n");
    } else {
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

        if (!workerActive) {
            workerActive = true;
            pthread_create(&workerThread, NULL, workerCommunication, workerInfo);
        } else {
            printf("Worker communication already in progress.\n");
            free(workerInfo);
        }
    }
}


void sendConnectionRequest(const char *username, const char *ip, int port) {
    Frame frame = {0};
    frame.type = 0x01; // Connection request frame type
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s&%s&%d", username, ip, port);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    uint8_t buffer[FRAME_SIZE];
    serializeFrame(&frame, buffer);
    if (write(sockfd, buffer, FRAME_SIZE) < 0) {
        perror("Error sending connection request");
    }
}

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

void sendLogoutRequest(const char *username) {
    Frame frame = {0};
    frame.type = 0x07; // Logout frame type
    frame.timestamp = time(NULL);
    snprintf(frame.data, sizeof(frame.data), "%s", username);
    frame.dataLength = strlen(frame.data);
    frame.checksum = calculateChecksum(&frame);

    uint8_t buffer[FRAME_SIZE];
    serializeFrame(&frame, buffer);
    if (write(sockfd, buffer, FRAME_SIZE) < 0) {
        perror("Error sending logout request");
    }
}


// Handle commands from the user
void handleCommands(Fleck *user) {
    char *command;
    char *message;
    asprintf(&message, "%s user initialized\n", user->name);
    printF(message);
    free(message);

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
            printF("Command OK\n");
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
            }
        } else if (strcasecmp(command, "LOGOUT") == 0) {
            if (sockfd != -1) {
                sendLogoutRequest(user->name);
                close(sockfd);
                sockfd = -1;
            }
        } else if (strcasecmp(command, "LIST MEDIA") == 0) {
            listFiles(user->userFile, FILE_TYPE_MEDIA);
        } else if (strcasecmp(command, "LIST TEXT") == 0) {
            listFiles(user->userFile, FILE_TYPE_TEXT);
        } else if (strncasecmp(command, "DISTORT", 7) == 0) {
            printF("Command OK\n");
            if (sockfd == -1) {
                printf("You must connect to Gotham before making a distortion request.\n");
            } else {
                char mediaType[16], fileName[128];
                printf("Enter media type (Text/Media): ");
                scanf("%15s", mediaType);
                printf("Enter file name: ");
                scanf("%127s", fileName);

                sendDistortionRequest(mediaType, fileName);
                handleDistortionResponse();
            }
        } else if (strcasecmp(command, "CHECK STATUS") == 0) {
            printF("Command OK\n");
        } else if (strcasecmp(command, "CLEAR ALL") == 0) {
            printF("Command OK\n");
        } else {
            printF("Unknown command\n");
        }

        free(command);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Error: You need to provide a configuration file\n");
        return -1;
    }

    printf("Reading configuration file\n");
    Fleck *user = (Fleck *)readConfigFile(argv[1], "Fleck");

    if (user == NULL) {
        printf("Error: Could not load User configuration\n");
        return -2;
    }

    handleCommands(user);

    if (sockfd != -1) {
        close(sockfd);
    }

    free(user);

    if (workerActive) {
        pthread_join(workerThread, NULL);
    }

    return 0;
}
