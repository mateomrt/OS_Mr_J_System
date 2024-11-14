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


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printF("Error you need to give a configuration file");
        return -1;
    }
    
    Harley* harley = (Harley*)readConfigFile(argv[1], "Harley");
    
    if (harley == NULL)
    {
        printF("HARLEY NULL");
        return -2;
    }

    if (strcmp(harley->workerType, "Media") != 0 && strcmp(harley->workerType, "Text") != 0) {
        printF("Error: Invalid worker type\n");
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
