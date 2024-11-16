#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "Common.h"

char *readUntil(int fd, char cEnd) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = NULL;
    
    // Initialize buffer with size 1 for the null terminator
    buffer = (char *)malloc(1);
    if (buffer == NULL) {
        return NULL;
    }
    
    while (1) {
        chars_read = read(fd, &c, sizeof(char));
        if (chars_read == 0) {
            if (i == 0) {
                free(buffer);  // Free buffer if no chars were read
                return NULL;
            }
            break;
        } else if (chars_read < 0) {
            free(buffer);
            return NULL;
        }
        
        if (c == cEnd) {
            break;
        }
        
        buffer = (char *)realloc(buffer, i + 2);
        if (buffer == NULL) {
            return NULL;
        }
        buffer[i++] = c;
    }
    
    buffer[i] = '\0';  // Null-terminate the string
    return buffer;
}

void* readConfigFile(char *file, void *config) {
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        printF("Error: File not found\n");
        return NULL;
    }

    if (config == NULL) {
        printF("Error: Config struct pointer is NULL\n");
        close(fd);
        return NULL;
    }

    if (strcmp(config, "Gotham") == 0) {
        Gotham *gotham = (Gotham*)malloc(sizeof(Gotham));
        gotham->fleckIpAddress = readUntil(fd, '\n');
        gotham->fleckPort = atoi(readUntil(fd, '\n'));
        gotham->harleyEnigmaIpAddress = readUntil(fd, '\n');
        gotham->harleyEnigmaPort = atoi(readUntil(fd, '\n'));
        close(fd);
        return gotham;
    } else if (strcmp(config, "Enigma") == 0) {
        Enigma *enigma = (Enigma*)malloc(sizeof(Enigma));
        enigma->gothamIpAddress = readUntil(fd, '\n');
        enigma->gothamPort = atoi(readUntil(fd, '\n'));
        enigma->fleckIpAddress = readUntil(fd, '\n');
        enigma->fleckPort = atoi(readUntil(fd, '\n'));
        enigma->folderName = readUntil(fd, '\n');
        enigma->workerType = readUntil(fd, '\n');
        close(fd);
        return enigma;
    } else if (strcmp(config, "Fleck") == 0) {
        Fleck *user = (Fleck*)malloc(sizeof(Fleck));
        user->name = readUntil(fd, '\n');
        user->userFile = readUntil(fd, '\n');
        user->ipAddress = readUntil(fd, '\n');
        user->port = atoi(readUntil(fd, '\n'));
        close(fd);
        return user;
    } else if (strcmp(config, "Harley") == 0) {
        Harley *harley = (Harley*)malloc(sizeof(Harley));
        harley->gothamIpAddress = readUntil(fd, '\n');
        harley->gothamPort = atoi(readUntil(fd, '\n'));
        harley->fleckIpAddress = readUntil(fd, '\n');
        harley->fleckPort = atoi(readUntil(fd, '\n'));
        harley->folderName = readUntil(fd, '\n');
        harley->workerType = readUntil(fd, '\n');
        close(fd);
        return harley;
    } else {
        printF("Error: Unknown config struct type\n");
        close(fd);
        return NULL;
    }

    close(fd);
    return NULL;
}