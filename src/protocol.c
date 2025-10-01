#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "protocol.h"

static void str_toupper(char *str) {
  for (int i = 0; str[i]; i++) {
    str[i] = toupper((unsigned char)str[i]);
  }
}

bool parse_command(const char *buffer, Command *cmd) {
  if (!buffer || !cmd) {
    return false;
  }

  // Make a working copy and convert to uppercase
  char work[MAX_COMMAND_LENGTH];
  strncpy(work, buffer, MAX_COMMAND_LENGTH - 1);
  work[MAX_COMMAND_LENGTH - 1] = '\0';

  // Remove trailing newline if present
  size_t len = strlen(work);
  if (len > 0 && work[len - 1] == '\n') {
    work[len - 1] = '\0';
  }

  str_toupper(work);

  // Tokenize the command
  char *token = strtok(work, " ");
  if (!token) {
    cmd->type = CMD_INVALID;
    return false;
  }

  if (strcmp(token, "SETUP") == 0) {
    cmd->type = CMD_SETUP;

    // Expect channel number
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }
    cmd->channel = atoi(token);

    // Expect "GPIO"
    token = strtok(NULL, " ");
    if (!token || strcmp(token, "GPIO") != 0) {
      cmd->type = CMD_INVALID;
      return false;
    }

    // Expect GPIO number
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }
    cmd->data.setup.gpio = atoi(token);

    return true;
  }

  if (strcmp(token, "ENABLE") == 0) {
    cmd->type = CMD_ENABLE;

    // Expect channel number
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }
    cmd->channel = atoi(token);

    return true;
  }

  if (strcmp(token, "DISABLE") == 0) {
    cmd->type = CMD_DISABLE;

    // Expect channel number
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }
    cmd->channel = atoi(token);

    return true;
  }

  if (strcmp(token, "SET") == 0) {
    // Expect channel number
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }
    cmd->channel = atoi(token);

    // Expect sub-command
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }

    if (strcmp(token, "RANGE") == 0) {
      cmd->type = CMD_SET_RANGE;

      // Expect min value
      token = strtok(NULL, " ");
      if (!token) {
        cmd->type = CMD_INVALID;
        return false;
      }
      cmd->data.range.min = atoi(token);

      // Expect max value
      token = strtok(NULL, " ");
      if (!token) {
        cmd->type = CMD_INVALID;
        return false;
      }
      cmd->data.range.max = atoi(token);

      return true;
    }

    if (strcmp(token, "PULSE") == 0) {
      cmd->type = CMD_SET_PULSE;

      // Expect value
      token = strtok(NULL, " ");
      if (!token) {
        cmd->type = CMD_INVALID;
        return false;
      }
      cmd->data.pulse.value = atoi(token);

      return true;
    }

    // Unknown sub-command
    cmd->type = CMD_INVALID;
    return false;
  }

  if (strcmp(token, "GET") == 0) {
    // Expect channel
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }
    cmd->channel = atoi(token);

    // Expect sub-command
    token = strtok(NULL, " ");
    if (!token) {
      cmd->type = CMD_INVALID;
      return false;
    }

    if (strcmp(token, "RANGE") == 0) {
      cmd->type = CMD_GET_RANGE;
      return true;
    }

    if (strcmp(token, "PULSE") == 0) {
      cmd->type = CMD_GET_PULSE;
      return true;
    }

    if (strcmp(token, "STATE") == 0) {
      cmd->type = CMD_GET_STATE;
      return true;
    }

    // Unknown sub-command
    cmd->type = CMD_INVALID;
    return false;
  }

  // Unknown command
  cmd->type = CMD_INVALID;
  return false;
}

int format_response(const Response *resp, char *buffer, size_t buffer_size) {
  if (!resp || !buffer || buffer_size == 0) {
    return -1;
  }

  int written = 0;

  switch (resp->type) {
    case RESP_OK: {
      written = snprintf(buffer, buffer_size, "OK\n");
    } break;

    case RESP_ERROR: {
      written = snprintf(
        buffer, buffer_size,
        "ERROR %s\n", resp->data.error.message
      );
    } break;

    case RESP_RANGE: {
      written = snprintf(
        buffer, buffer_size,
        "RANGE %u %u\n", resp->data.range.min, resp->data.range.max
      );
    } break;

    case RESP_PULSE: {
      written = snprintf(
        buffer, buffer_size,
        "PULSE %u\n", resp->data.pulse.value
      );
    } break;

    case RESP_STATE: {
      written = snprintf(
        buffer, buffer_size,
        "GPIO %u ENABLE %d\n",
        resp->data.state.gpio,
        resp->data.state.enabled ? 1 : 0
      );
    } break;

    default: {
      return -1;
    }
  }

  if (written < 0 || (size_t) written >= buffer_size) {
    return -1;
  }

  return written;
}
