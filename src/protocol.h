#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_COMMAND_LENGTH 256
#define MAX_RESPONSE_LENGTH 256
#define MAX_ERROR_MESSAGE 128

typedef enum {
  CMD_SETUP,
  CMD_ENABLE,
  CMD_DISABLE,
  CMD_SET_RANGE,
  CMD_SET_PULSE,
  CMD_GET_RANGE,
  CMD_GET_PULSE,
  CMD_GET_STATE,
  CMD_INVALID
} CommandType;

typedef enum {
  RESP_OK,
  RESP_ERROR,
  RESP_RANGE,
  RESP_PULSE,
  RESP_STATE
} ResponseType;

typedef struct {
  CommandType type;
  uint8_t channel;
  union {
    struct {
      uint8_t gpio;
    } setup;

    struct {
      uint16_t min;
      uint16_t max;
    } range;

    struct {
      uint16_t value;
    } pulse;
  } data;
} Command;

typedef struct {
  ResponseType type;
  union {
    struct {
      char message[MAX_ERROR_MESSAGE];
    } error;

    struct {
      uint16_t min;
      uint16_t max;
    } range;

    struct {
      uint16_t value;
    } pulse;

    struct {
      uint8_t gpio;
      bool enabled;
    } state;
  } data;
} Response;

/**
 * Parse a command string into a Command structure
 *
 * @param buffer Input string (newline-terminated)
 * @param cmd Output command structure
 *
 * @return true on success, false on parse error
 */
bool parse_command(const char *buffer, Command *cmd);

/**
 * Format a response structure into a string
 *
 * @param resp Input response structure
 * @param buffer Output string buffer
 * @param buffer_size Size of output buffer
 *
 * @return Number of bytes written, or -1 on error
 */
int format_response(const Response *resp, char *buffer, size_t buffer_size);

#endif // PROTOCOL_H
