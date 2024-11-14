#ifndef COMMON_H
#define COMMON_H

#define printF(x) write(1, x, strlen(x))

typedef struct{
    char* gothamIpAddress;
    int gothamPort;
    char* fleckIpAddress;
    int fleckPort;
    char* folderName;
    char* workerType;
} Enigma;

typedef struct{
    char* name;
    char* userFile;
    char* ipAddress;
    int port;
} User;

typedef struct{
    char* fleckIpAddress;
    int fleckPort;
    char* harleyEnigmaIpAddress;
    int harleyEnigmaPort;
} Gotham;

typedef struct{
    char* gothamIpAddress;
    int gothamPort;
    char* fleckIpAddress;
    int fleckPort;
    char* folderName;
    char* workerType;
} Harley;

char *readUntil(int fd, char cEnd);

void* readConfigFile(char *file, void *config);

#endif