# Compiler and flags
CC = clang
CFLAGS = -Wall -Werror -Wall -Wextra -pedantic -g
LIBS = -lm -lpthread

# Directories
SRC_DIR = src
LIB_DIR = lib

# Source files and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(LIB_DIR)/%.o,$(SRCS))

# Targets
TARGET = redis

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(LIB_DIR)/%.o: $(SRC_DIR)/%.c | $(LIB_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

clean:
	rm -rf $(LIB_DIR) $(TARGET)

