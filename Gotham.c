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
    char* fleckIpAddress;
    int fleckPort;
    char* harleyEnigmaIpAddress;
    int harleyEnigmaPort;
} Gotham;

Gotham *readConfigFile(char *file) {
    char* buffer;
    Gotham *gotham;

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        printF("Error: File not found\n");
    } else {
        
        gotham = (Gotham *)malloc(sizeof(Gotham));

        gotham->fleckIpAddress = readUntil(fd, '\n');
        
        buffer = readUntil(fd, '\n');
        if (buffer != NULL) {
            gotham->fleckPort = atoi(buffer); 
            free(buffer);
        }
        
        gotham->harleyEnigmaIpAddress = readUntil(fd, '\n');

        buffer = readUntil(fd, '\n');
        if (buffer != NULL) {
            gotham->harleyEnigmaPort = atoi(buffer);
            free(buffer);
        }

        close(fd);
    }

    return gotham;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printF("Error you need to give a configuration file");
        return -1;
    }
    Gotham *gotham = NULL;

    gotham = readConfigFile(argv[1]);
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

    free(gotham);
    return 0;
}
