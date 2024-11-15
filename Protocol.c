#include "Protocol.h"

// Calculate checksum for a frame
uint16_t calculateChecksum(const Frame *frame) {
    uint32_t sum = 0;
    sum += frame->type;
    sum += (frame->dataLength & 0xFF) + ((frame->dataLength >> 8) & 0xFF);
    for (int i = 0; i < frame->dataLength; i++) {
        sum += (uint8_t)frame->data[i];
    }
    sum += (frame->timestamp & 0xFF) + ((frame->timestamp >> 8) & 0xFF) +
           ((frame->timestamp >> 16) & 0xFF) + ((frame->timestamp >> 24) & 0xFF);
    return sum % CHECKSUM_MODULO;
}

// Serialize a Frame into a buffer
void serializeFrame(const Frame *frame, uint8_t *buffer) {
    memset(buffer, 0, FRAME_SIZE);
    buffer[0] = frame->type;
    buffer[1] = frame->dataLength & 0xFF;
    buffer[2] = (frame->dataLength >> 8) & 0xFF;
    memcpy(buffer + 3, frame->data, frame->dataLength);
    uint16_t checksum = frame->checksum;
    buffer[FRAME_SIZE - 6] = checksum & 0xFF;
    buffer[FRAME_SIZE - 5] = (checksum >> 8) & 0xFF;
    int32_t timestamp = frame->timestamp;
    memcpy(buffer + FRAME_SIZE - 4, &timestamp, sizeof(int32_t));
}

// Deserialize a buffer into a Frame
void deserializeFrame(const uint8_t *buffer, Frame *frame) {
    frame->type = buffer[0];
    frame->dataLength = buffer[1] | (buffer[2] << 8);
    memcpy(frame->data, buffer + 3, frame->dataLength);
    frame->checksum = buffer[FRAME_SIZE - 6] | (buffer[FRAME_SIZE - 5] << 8);
    memcpy(&frame->timestamp, buffer + FRAME_SIZE - 4, sizeof(int32_t));
}