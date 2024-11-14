#ifndef COMMON_H
#define COMMON_H

#define printF(x) write(1, x, strlen(x))

char *readUntil(int fd, char cEnd);

#endif