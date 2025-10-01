CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lrt

# Directories
SRC_DIR = src
BUILD_DIR = build
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# Source files
SOURCES = $(SRC_DIR)/piservod.c \
          $(SRC_DIR)/pwm.c \
          $(SRC_DIR)/gpio.c \
          $(SRC_DIR)/protocol.c \
          $(SRC_DIR)/servo.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Target binary
TARGET = piservod

.PHONY: all clean install uninstall

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Uninstalled $(TARGET)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Cleaned build artifacts"
