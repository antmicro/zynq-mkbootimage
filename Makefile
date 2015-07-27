CC=gcc
FILES=src/{mkbootimage.c,bif.c,bootrom.c}
CFLAGS=-lpcre -lelf -Wall
TARGET=mkbootimage

all:
	${CC} ${FILES} ${CFLAGS} -o ${TARGET}

clean:
	rm ${TARGET}
