#ifndef PROTOCOL_H
#define PROTOCOL_H

#define _GNU_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "Common.h"


#define FRAME_SIZE 256

// Frame structure
typedef struct {
    uint8_t type;              // Frame type
    uint16_t dataLength;       // Length of the data
    char data[FRAME_SIZE - 9]; // Data field (variable length)
    uint16_t checksum;         // Checksum
    int32_t timestamp;         // Timestamp
} Frame;

// Utility functions
uint16_t calculateChecksum(const Frame *frame);
void serializeFrame(const Frame *frame, uint8_t *buffer);
void deserializeFrame(const uint8_t *buffer, Frame *frame);

#endif
