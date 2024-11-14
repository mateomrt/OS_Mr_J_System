#include "Strings.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


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