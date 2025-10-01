#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>

#include "pwm.h"
#include "gpio.h"
#include "protocol.h"
#include "servo.h"

#define MAX_CLIENTS 10
#define BACKLOG 5

// Global state
static ServoController controller;
static int listen_fd = -1;
static volatile sig_atomic_t running = 1;

// Client connection tracking
static int client_fds[MAX_CLIENTS];
static char client_buffers[MAX_CLIENTS][MAX_COMMAND_LENGTH];
static size_t client_buffer_lens[MAX_CLIENTS];

static void signal_handler(int signo) {
  (void)signo;
  running = 0;
}

static bool setup_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) < 0) {
    perror("sigaction SIGINT setup failed");
    return false;
  }

  if (sigaction(SIGTERM, &sa, NULL) < 0) {
    perror("sigaction SIGTERM setup failed");
    return false;
  }

  // Ignore SIGPIPE (client disconnect)
  signal(SIGPIPE, SIG_IGN);

  return true;
}

static int create_socket(void) {
  struct sockaddr_un addr;

  // Create socket
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("Failed creating socket");
    return -1;
  }

  // Remove old socket file if it exists
  unlink(SOCKET_PATH);

  // Setup address and create new socket file
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Failed binding to the socket");
    close(fd);

    return -1;
  }

  if (listen(fd, BACKLOG) < 0) {
    perror("Failed listening to socket");
    close(fd);
    unlink(SOCKET_PATH);

    return -1;
  }

  // Make socket accessible to all users
  if (chmod(SOCKET_PATH, 0666) < 0) {
    perror("Warning: Failed to set socket permissions");
  }

  printf("Listening on %s\n", SOCKET_PATH);
  return fd;
}

static void init_clients(void) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    client_fds[i] = -1;
    client_buffer_lens[i] = 0;
  }
}

static bool add_client(int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (client_fds[i] == -1) {
      client_fds[i] = fd;
      client_buffer_lens[i] = 0;
      printf("Client connected (slot %d)\n", i);

      return true;
    }
  }

  return false;
}

static void remove_client(int slot) {
  if (slot >= 0 && slot < MAX_CLIENTS && client_fds[slot] != -1) {
    close(client_fds[slot]);
    client_fds[slot] = -1;
    client_buffer_lens[slot] = 0;

    printf("Client disconnected (slot %d)\n", slot);
  }
}

static void handle_command(int client_fd, const char *buffer) {
  Command cmd;
  Response resp;
  char resp_buffer[MAX_RESPONSE_LENGTH];

  if (!parse_command(buffer, &cmd)) {
    resp.type = RESP_ERROR;
    snprintf(resp.data.error.message, MAX_ERROR_MESSAGE, "Invalid command");
    format_response(&resp, resp_buffer, sizeof(resp_buffer));
    write(client_fd, resp_buffer, strlen(resp_buffer));

    return;
  }

  if (cmd.channel >= MAX_SERVO_CHANNELS) {
    resp.type = RESP_ERROR;
    snprintf(resp.data.error.message, MAX_ERROR_MESSAGE, "Invalid channel");
    format_response(&resp, resp_buffer, sizeof(resp_buffer));
    write(client_fd, resp_buffer, strlen(resp_buffer));

    return;
  }

  ServoChannel *ch = &controller.channels[cmd.channel];

  switch (cmd.type) {
    case CMD_SETUP: {
      if (cmd.data.setup.gpio > MAX_GPIO_PIN) {
        resp.type = RESP_ERROR;
        snprintf(
          resp.data.error.message, MAX_ERROR_MESSAGE,
          "Invalid GPIO pin"
        );

        break;
      }

      ch->gpio = cmd.data.setup.gpio;
      ch->pulse_us = SERVO_NEUTRAL_US;
      ch->min_us = SERVO_MIN_US;
      ch->max_us = SERVO_MAX_US;
      ch->enabled = false;
      resp.type = RESP_OK;

      gpio_set_output(ch->gpio);
    } break;

    case CMD_ENABLE: {
      if (ch->gpio == 0) {
        resp.type = RESP_ERROR;
        snprintf(
          resp.data.error.message, MAX_ERROR_MESSAGE,
          "Channel not configured"
        );

        break;
      }

      ch->enabled = true;
      resp.type = RESP_OK;
    } break;

    case CMD_DISABLE: {
      ch->enabled = false;
      resp.type = RESP_OK;
    } break;

    case CMD_SET_RANGE: {
      if (cmd.data.range.min >= cmd.data.range.max) {
        resp.type = RESP_ERROR;
        snprintf(
          resp.data.error.message, MAX_ERROR_MESSAGE,
          "Invalid range: min must be less than max"
        );

        break;
      }

      ch->min_us = cmd.data.range.min;
      ch->max_us = cmd.data.range.max;
      resp.type = RESP_OK;
    } break;

    case CMD_SET_PULSE: {
      if (ch->gpio == 0) {
        resp.type = RESP_ERROR;
        snprintf(
          resp.data.error.message, MAX_ERROR_MESSAGE,
          "Channel not configured"
        );

        break;
      }

      if (
        cmd.data.pulse.value < ch->min_us ||
        cmd.data.pulse.value > ch->max_us
      ) {
        resp.type = RESP_ERROR;
        snprintf(
          resp.data.error.message, MAX_ERROR_MESSAGE,
          "Pulse value out of range"
        );

        break;
      }

      ch->pulse_us = cmd.data.pulse.value;
      resp.type = RESP_OK;
    } break;

    case CMD_GET_RANGE: {
      resp.type = RESP_RANGE;
      resp.data.range.min = ch->min_us;
      resp.data.range.max = ch->max_us;
    } break;

    case CMD_GET_PULSE: {
      resp.type = RESP_PULSE;
      resp.data.pulse.value = ch->pulse_us;
    } break;

    case CMD_GET_STATE: {
      resp.type = RESP_STATE;
      resp.data.state.gpio = ch->gpio;
      resp.data.state.enabled = ch->enabled;
    } break;

    default: {
      resp.type = RESP_ERROR;
      snprintf(resp.data.error.message, MAX_ERROR_MESSAGE, "Unknown command");
    } break;
  }

  format_response(&resp, resp_buffer, sizeof(resp_buffer));
  write(client_fd, resp_buffer, strlen(resp_buffer));
}

static void handle_client_data(int slot) {
  char temp_buffer[MAX_COMMAND_LENGTH];
  ssize_t bytes_read;

  bytes_read = read(client_fds[slot], temp_buffer, sizeof(temp_buffer) - 1);

  // Client disconnected or error
  if (bytes_read <= 0) {
    remove_client(slot);
    return;
  }

  temp_buffer[bytes_read] = '\0';

  // Append to client's buffer
  size_t available = MAX_COMMAND_LENGTH - client_buffer_lens[slot] - 1;
  size_t to_copy = bytes_read < (ssize_t) available ? (size_t) bytes_read : available;

  memcpy(client_buffers[slot] + client_buffer_lens[slot], temp_buffer, to_copy);
  client_buffer_lens[slot] += to_copy;
  client_buffers[slot][client_buffer_lens[slot]] = '\0';

  // Process complete commands (lines ending with \n)
  char *line_start = client_buffers[slot];
  char *newline;

  while ((newline = strchr(line_start, '\n')) != NULL) {
    *newline = '\0';
    handle_command(client_fds[slot], line_start);
    line_start = newline + 1;
  }

  // Move remaining incomplete data to start of buffer
  size_t remaining = strlen(line_start);
  if (remaining > 0 && line_start != client_buffers[slot]) {
    memmove(client_buffers[slot], line_start, remaining + 1);
  }

  client_buffer_lens[slot] = remaining;
}

int main(void) {
  fd_set read_fds;
  struct timeval tv;
  int max_fd;

  printf("Starting servo daemon...\n");

  memset(&controller, 0, sizeof(controller));
  controller.num_channels = MAX_SERVO_CHANNELS;

  init_clients();

  if (!gpio_init()) {
    fprintf(stderr, "Failed to initialize GPIO\n");
    return 1;
  }

  if (!pwm_init(&controller)) {
    fprintf(stderr, "Failed to initialize PWM\n");
    gpio_cleanup();
    return 1;
  }

  if (!setup_signals()) {
    fprintf(stderr, "Failed to setup signal handlers\n");
    pwm_cleanup();
    gpio_cleanup();
    return 1;
  }

  listen_fd = create_socket();
  if (listen_fd < 0) {
    pwm_cleanup();
    gpio_cleanup();
    return 1;
  }

  printf("Servo daemon running\n");

  while (running) {
    pwm_run_frame(&controller);

    // Setup fd_set for select
    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds);
    max_fd = listen_fd;

    // Add all active client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (client_fds[i] != -1) {
        FD_SET(client_fds[i], &read_fds);
        if (client_fds[i] > max_fd) {
          max_fd = client_fds[i];
        }
      }
    }

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
    if (ready > 0) {
      if (FD_ISSET(listen_fd, &read_fds)) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd >= 0) {
          if (!add_client(client_fd)) {
            fprintf(stderr, "Too many clients, rejecting connection\n");
            close(client_fd);
          }
        }
      }

      // Check for data from existing clients
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] != -1 && FD_ISSET(client_fds[i], &read_fds)) {
          handle_client_data(i);
        }
      }
    }
  }

  printf("\nShutting down...\n");

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (client_fds[i] != -1) {
      close(client_fds[i]);
    }
  }

  if (listen_fd >= 0) {
    close(listen_fd);
    unlink(SOCKET_PATH);
  }

  for (int i = 0; i < controller.num_channels; i++) {
    servo_close(&controller.channels[i]);
  }

  pwm_cleanup();
  gpio_cleanup();

  printf("Shutdown complete\n");
  return 0;
}
