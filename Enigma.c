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
#include "Strings.h"


typedef struct{
    char* gothamIpAddress;
    int gothamPort;
    char* fleckIpAddress;
    int fleckPort;
    char* folderName;
    char* workerType;
} Enigma;

Enigma *readConfigFile(char *file) {
    char* buffer;
    Enigma *enigma;

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        printf("Error: File not found\n");
    } else {
        
        enigma = (Enigma *)malloc(sizeof(Enigma));

        enigma->gothamIpAddress = readUntil(fd, '\n');
        
        buffer = readUntil(fd, '\n');
        if (buffer != NULL) {
            enigma->gothamPort = atoi(buffer); 
            free(buffer);
        }
        
        enigma->fleckIpAddress = readUntil(fd, '\n');

        buffer = readUntil(fd, '\n');
        if (buffer != NULL) {
            enigma->fleckPort = atoi(buffer); 
            free(buffer);
        }

        enigma->folderName = readUntil(fd, '\n');
        enigma->workerType = readUntil(fd, '\n');

        close(fd);
    }

    return enigma;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printF("Error you need to give a configuration file");
        return -1;
    }
    Enigma *enigma = NULL;

    enigma = readConfigFile(argv[1]);

    if (enigma == NULL)
    {
        printF("ENIGMA NULL");
        return -2;
    }

    if (strcmp(enigma->workerType, "Media") != 0 && strcmp(enigma->workerType, "Text") != 0) {
        printf("Error: Invalid worker type\n");
        return -3;
    }

    //DISPLAY THE ENIGMA INFORMATIONS
    printF("Enigma Information:\n");
    char *message;
    asprintf(&message, "gotham ip address: %s\n", enigma->gothamIpAddress);
    printF(message);
    
    asprintf(&message, "gotham port: %d\n", enigma->gothamPort);
    printF(message);

    asprintf(&message, "fleck IP Address: %s\n", enigma->fleckIpAddress);
    printF(message);

    asprintf(&message, "fleck port: %d\n", enigma->fleckPort);
    printF(message);

    asprintf(&message, "folder name: %s\n", enigma->folderName);
    printF(message);

    asprintf(&message, "worker type: %s\n", enigma->workerType);
    printF(message);
    
    free(message);

    
    free(enigma);
    return 0;
}
