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
    
    Gotham* gotham = (Gotham*)readConfigFile(argv[1], "Gotham");
    
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
