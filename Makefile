CC=gcc
FILES=src/mkbootimage.c src/bif.c src/bootrom.c
PROGRAM_NAME=mkbootimage
VERSION_MAJOR=1.0
VERSION_MINOR=${shell git rev-parse --short HEAD}
CFLAGS=-lpcre -lelf -Wall \
	-DMKBOOTIMAGE_VER=\"${PROGRAM_NAME}\ ${VERSION_MAJOR}-${VERSION_MINOR}\"
TARGET=mkbootimage

all:
	${CC} ${FILES} ${CFLAGS} -o ${TARGET}

clean:
	rm ${TARGET}
