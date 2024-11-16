#include "Protocol.h"

// Calculate checksum for a frame
uint16_t calculateChecksum(const Frame *frame) {
    uint32_t sum = frame->type;
    sum += frame->dataLength & 0xFF;
    sum += (frame->dataLength >> 8) & 0xFF;

    for (int i = 0; i < frame->dataLength; i++) {
        sum += (uint8_t)frame->data[i];
    }

    sum += frame->timestamp & 0xFF;
    sum += (frame->timestamp >> 8) & 0xFF;
    sum += (frame->timestamp >> 16) & 0xFF;
    sum += (frame->timestamp >> 24) & 0xFF;

    return sum % 65536;
}

void serializeFrame(const Frame *frame, uint8_t *buffer) {
    memset(buffer, 0, FRAME_SIZE); // Ensure buffer is zeroed for padding

    // Write frame type
    buffer[0] = frame->type;

    // Write data length (2 bytes, little-endian)
    buffer[1] = frame->dataLength & 0xFF;
    buffer[2] = (frame->dataLength >> 8) & 0xFF;

    // Copy data (up to frame->dataLength)
    if (frame->dataLength > 0 && frame->dataLength <= sizeof(frame->data)) {
        memcpy(buffer + 3, frame->data, frame->dataLength);
    }

    // Write checksum (2 bytes, little-endian)
    buffer[FRAME_SIZE - 6] = frame->checksum & 0xFF;
    buffer[FRAME_SIZE - 5] = (frame->checksum >> 8) & 0xFF;

    // Write timestamp (4 bytes, little-endian)
    memcpy(buffer + FRAME_SIZE - 4, &frame->timestamp, sizeof(int32_t));
}


void deserializeFrame(const uint8_t *buffer, Frame *frame) {
    memset(frame, 0, sizeof(Frame)); // Ensure frame is zeroed

    // Read frame type
    frame->type = buffer[0];

    // Read data length (2 bytes, little-endian)
    frame->dataLength = buffer[1] | (buffer[2] << 8);

    // Validate data length
    if (frame->dataLength > sizeof(frame->data)) {
        fprintf(stderr, "Error: Invalid data length in frame (%d)\n", frame->dataLength);
        frame->dataLength = 0; // Prevent potential overflow
        return;
    }

    // Copy data
    if (frame->dataLength > 0) {
        memcpy(frame->data, buffer + 3, frame->dataLength);
    }

    // Read checksum (2 bytes, little-endian)
    frame->checksum = buffer[FRAME_SIZE - 6] | (buffer[FRAME_SIZE - 5] << 8);

    // Read timestamp (4 bytes, little-endian)
    memcpy(&frame->timestamp, buffer + FRAME_SIZE - 4, sizeof(int32_t));
}

