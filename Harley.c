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
#include "Common.h"


typedef struct{
    char* gothamIpAddress;
    int gothamPort;
    char* fleckIpAddress;
    int fleckPort;
    char* folderName;
    char* workerType;
} Harley;

Harley *readConfigFile(char *file) {
    char* buffer;
    Harley *harley;

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        printf("Error: File not found\n");
    } else {
        
        harley = (Harley *)malloc(sizeof(Harley));

        
        harley->gothamIpAddress = readUntil(fd, '\n');
        
        buffer = readUntil(fd, '\n');
        if (buffer != NULL) {
            harley->gothamPort = atoi(buffer); 
            free(buffer);
        }
        
        harley->fleckIpAddress = readUntil(fd, '\n');

        buffer = readUntil(fd, '\n');
        if (buffer != NULL) {
            harley->fleckPort = atoi(buffer);
            free(buffer);
        }

        harley->folderName = readUntil(fd, '\n');
        harley->workerType = readUntil(fd, '\n');

        close(fd);
    }

    return harley;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printF("Error you need to give a configuration file");
        return -1;
    }
    Harley *harley = NULL;

    harley = readConfigFile(argv[1]);

    if (harley == NULL)
    {
        printF("HARLEY NULL");
        return -2;
    }

    if (strcmp(harley->workerType, "Media") != 0 && strcmp(harley->workerType, "Text") != 0) {
        printf("Error: Invalid worker type\n");
        return -3;
    }

    //DISPLAY THE HARLEY INFORMATIONS
    printF("Harley Information:\n");
    char *message;
    asprintf(&message, "gotham ip address: %s\n", harley->gothamIpAddress);
    printF(message);
    
    asprintf(&message, "gotham port: %d\n", harley->gothamPort);
    printF(message);

    asprintf(&message, "fleck IP Address: %s\n", harley->fleckIpAddress);
    printF(message);

    asprintf(&message, "fleck port: %d\n", harley->fleckPort);
    printF(message);

    asprintf(&message, "folder name: %s\n", harley->folderName);
    printF(message);

    asprintf(&message, "worker type: %s\n", harley->workerType);
    printF(message);
    
    free(message);

    
    free(harley);
    return 0;
}
