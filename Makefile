# vmctl Makefile

ifeq ($(OS),Windows_NT)
    CC := gcc
    EXEEXT := .exe
    GNU_BIN ?= C:/Program Files/GNU/bin
    MKDIR_P ?= "$(GNU_BIN)/mkdir.exe" -p
    RM_RF   ?= "$(GNU_BIN)/rm.exe" -rf
    CP      ?= "$(GNU_BIN)/cp.exe" -f
    PREFIX  ?= C:/Program Files/libvm
else
    CC ?= cc
    EXEEXT :=
    MKDIR_P ?= mkdir -p
    RM_RF   ?= rm -rf
    CP      ?= cp -f
    PREFIX  ?= /usr/local
endif

TARGET      := vmctl$(EXEEXT)
SRC_DIR     := src
INCLUDE_DIR := include
BUILD_DIR   := build

SOURCES := \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/cli.c \
    $(SRC_DIR)/session.c \
    $(SRC_DIR)/config.c

OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

CPPFLAGS ?= -I$(INCLUDE_DIR)
CFLAGS   ?= -Wall -Wextra -O2

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR_P) "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	@$(MKDIR_P) "$(PREFIX)/bin"
	@$(CP) "$(TARGET)" "$(PREFIX)/bin/$(TARGET)"

test: $(TARGET)
	./$(TARGET) version

clean:
	@$(RM_RF) "$(BUILD_DIR)"
	@$(RM_RF) "$(TARGET)"

rebuild: clean all

.PHONY: all install test clean rebuild
