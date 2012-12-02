CFLAGS=-Wall -g
LDFLAGS=-levent -lpthread
LIBS=workqueue.o
HEADERS=workqueue.h
BINS=getsample_threaded

all: ${BINS}

getsample_threaded: getsample_threaded.o ${LIBS} ${HEADERS}
	${CC} getsample_threaded.o ${LIBS} ${LDFLAGS} -o $@

clean:
	/bin/rm -rf ${BINS} *.o core *.core
