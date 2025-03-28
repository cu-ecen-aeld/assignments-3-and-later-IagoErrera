CC ?= ${CROSS_COMPILE}gcc
CFLAGS ?= -Wall
CFLAGS += -I./includes  

TARGET ?= aesdsocket
BUILD_DIR = build
SRC_DIR = src
SRC = aesdsocket.c
OBJ = aesdsocket.o

.PHONY: all
all: ${TARGET}

.PHONY: default
default: ${TARGET}

${TARGET}: ${BUILD_DIR}/${OBJ}
	${CC} ${LDFLAGS} $^ -o $@ 

${BUILD_DIR}/${OBJ}: ${SRC_DIR}/${SRC}
	mkdir -p build
	${CC} ${CFLAGS} -c $^ -o $@

client: src/client.c
	${CC} ${CFLAGS} $^ -o $@

clean: 
	rm -rf ${BUILD_DIR}
	rm -f ${TARGET}
	rm -f client
