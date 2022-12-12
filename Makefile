CC     := gcc
CFLAGS := -Wall -Werror 

SRCS   := client.c \
	server.c \

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

.PHONY: all
all: ${PROGS}

${PROGS} : % : %.o Makefile
	${CC} $< -o $@ udp.c mfs.c

lib:
	gcc -fPIC -c mfs.c -o mfs.o
	gcc -shared mfs.o udp.h udp.c -o libmfs.so

clean:
	rm -f ${PROGS} ${OBJS}

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<
