#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FRAME_SIZE 256
#define CHECKSUM_MODULO 65536

// Frame structure
typedef struct {
    uint8_t type;              // 1 byte
    uint16_t dataLength;       // 2 bytes
    char data[FRAME_SIZE - 9]; // Variable length
    uint16_t checksum;         // 2 bytes
    int32_t timestamp;         // 4 bytes
} Frame;

// Utility functions
uint16_t calculateChecksum(const Frame *frame);
void serializeFrame(const Frame *frame, uint8_t *buffer);
void deserializeFrame(const uint8_t *buffer, Frame *frame);

#endif