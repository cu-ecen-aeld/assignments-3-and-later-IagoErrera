CC = $(CROSS_COMPILE)gcc
CFLAGS = 

SRC_DIR = ./src
BUILD_DIR = ./build
TARGET = writer

FILES = writer

.PHONY: all
all: $(TARGET) 
	echo "Compiler: ${CC}"	

$(TARGET): $(BUILD_DIR)/$(FILES).o
	$(CC) $^ -o $@ 

$(BUILD_DIR)/$(FILES).o: $(SRC_DIR)/$(FILES).c
	mkdir -p build
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -rf build
	rm -f $(TARGET)
