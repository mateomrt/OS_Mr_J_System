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
#include <stdbool.h>
#include <dirent.h>
#include "Common.h"

typedef enum {
    FILE_TYPE_TEXT,
    FILE_TYPE_MEDIA
} FileType;


bool isFileOfType(const char* filename, FileType type) {
    const char* ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }

    switch (type) {
        case FILE_TYPE_TEXT:
            return (strcasecmp(ext, ".txt") == 0);
            
        case FILE_TYPE_MEDIA:
            return (strcasecmp(ext, ".wav") == 0 ||
                    strcasecmp(ext, ".mp3") == 0 ||
                    strcasecmp(ext, ".jpg") == 0 ||
                    strcasecmp(ext, ".jpeg") == 0 ||
                    strcasecmp(ext, ".png") == 0);
            
        default:
            return false;
    }
}

void listFiles(const char* directory, FileType type) {
    DIR *dir;
    struct dirent *entry;
    int fileCount = 0;
    char *message;
    const char* typeStr = (type == FILE_TYPE_TEXT) ? "text" : "media";
    
    dir = opendir(directory);
    if (dir == NULL) {
        asprintf(&message, "Error: Cannot open directory %s\n", directory);
        printF(message);
        free(message);
        return;
    }
    
    // Count files
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && isFileOfType(entry->d_name, type)) {
            fileCount++;
        }
    }
    closedir(dir);
    
    if (fileCount == 0) {
        asprintf(&message, "There are no %s files available.\n", typeStr);
        printF(message);
        free(message);
        return;
    }
    
    asprintf(&message, "There are %d %s files available:\n", fileCount, typeStr);
    printF(message);
    free(message);
    
    // List files
    dir = opendir(directory);
    int currentFile = 1;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && isFileOfType(entry->d_name, type)) {
            asprintf(&message, "%d. %s\n", currentFile, entry->d_name);
            printF(message);
            free(message);
            currentFile++;
        }
    }
    
    closedir(dir);
}

void handleCommands(User* user) {
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
        } else if (strcasecmp(command, "LOGOUT") == 0) {
            printF("Command OK\n");
        } else if (strcasecmp(command, "LIST MEDIA") == 0) {
            listFiles(user->userFile, FILE_TYPE_MEDIA);
        } else if (strcasecmp(command, "LIST TEXT") == 0) {
            listFiles(user->userFile, FILE_TYPE_TEXT);
        } else if (strncasecmp(command, "DISTORT", 7) == 0) {
            printF("Command OK\n");
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

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printF("Error you need to give a configuration file");
        return -1;
    }
    
    User* user = (User*)readConfigFile(argv[1], "User");

    if (user == NULL)
    {
        printF("USER NULL");
        return -2;
    }

    handleCommands(user);

    free(user);
    return 0;
}
