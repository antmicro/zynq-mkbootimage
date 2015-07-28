CC=gcc
FILES=src/mkbootimage.c src/bif.c src/bootrom.c
CFLAGS=-lpcre -lelf -Wall
TARGET=mkbootimage

all:
	${CC} ${FILES} ${CFLAGS} -o ${TARGET}

clean:
	rm ${TARGET}
