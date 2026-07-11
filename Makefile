# vmctl Makefile
#
# Supported platforms:
#   - Windows with MinGW/GNU make
#   - Linux
#   - macOS
#
# Common targets:
#   make
#   make test
#   make install
#   make uninstall
#   make clean
#   make rebuild
#
# Default install locations:
#   Windows: C:/Program Files/libvm/bin
#   Linux:   ~/.local/bin
#   macOS:   ~/.local/bin
#
# Override example:
#   make PREFIX=/usr/local install

PROJECT_NAME := vmctl

SRC_DIR     := src
INCLUDE_DIR := include
BUILD_DIR   := build

SOURCES := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/cli.c \
	$(SRC_DIR)/session.c \
	$(SRC_DIR)/config.c

OBJECTS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/cli.o \
	$(BUILD_DIR)/session.o \
	$(BUILD_DIR)/config.o

CPPFLAGS ?= -I$(INCLUDE_DIR)
CFLAGS   ?= -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)

	CC := gcc
	EXEEXT := .exe

	GNU_BIN ?= C:/Program Files/GNU/bin

	MKDIR_P ?= "$(GNU_BIN)/mkdir.exe" -p
	RM_RF   ?= "$(GNU_BIN)/rm.exe" -rf
	CP      ?= "$(GNU_BIN)/cp.exe" -f

	PREFIX ?= C:/Program Files/libvm

else

	UNAME_S := $(shell uname -s)

	CC ?= cc
	EXEEXT :=

	MKDIR_P ?= mkdir -p
	RM_RF   ?= rm -rf
	CP      ?= cp -f

	# Developer-friendly per-user install location for Linux and macOS.
	PREFIX ?= $(HOME)/.local

	ifeq ($(UNAME_S),Darwin)
		PLATFORM := macOS
	else ifeq ($(UNAME_S),Linux)
		PLATFORM := Linux
	else
		PLATFORM := Unix
	endif

endif

TARGET := $(PROJECT_NAME)$(EXEEXT)
BINDIR := $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR_P) "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET) version

install: $(TARGET)
	@$(MKDIR_P) "$(BINDIR)"
	@$(CP) "$(TARGET)" "$(BINDIR)/$(TARGET)"
	@echo "Installed $(TARGET) to $(BINDIR)"

uninstall:
	@$(RM_RF) "$(BINDIR)/$(TARGET)"
	@echo "Removed $(BINDIR)/$(TARGET)"

clean:
	@$(RM_RF) "$(BUILD_DIR)"
	@$(RM_RF) "$(TARGET)"

rebuild: clean all

show-config:
	@echo "Project:     $(PROJECT_NAME)"
	@echo "Compiler:    $(CC)"
	@echo "Target:      $(TARGET)"
	@echo "Prefix:      $(PREFIX)"
	@echo "Binary dir:  $(BINDIR)"
ifndef OS
	@echo "Platform:    $(PLATFORM)"
endif

.PHONY: all test install uninstall clean rebuild show-config
