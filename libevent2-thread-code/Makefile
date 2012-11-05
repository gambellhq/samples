CFLAGS=-Wall -g
LDFLAGS=-levent -lpthread
LIBS=workqueue.o
HEADERS=workqueue.h
BINS=echoserver_threaded

all: ${BINS}

echoserver_threaded: echoserver_threaded.o ${LIBS} ${HEADERS}
	${CC} echoserver_threaded.o ${LIBS} ${LDFLAGS} -o $@

clean:
	/bin/rm -rf ${BINS} *.o core *.core
