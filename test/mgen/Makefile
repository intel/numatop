PROG = mgen
CC = gcc
LD = gcc
CFLAGS = -g -Wall
LDLIBS = -lpthread

OBJS = mgen.o

all: $(PROG)

$(PROG): $(OBJS)
	$(LD) $(LDLIBS) -o $@ $(OBJS) -lnuma

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *.o mgen
	
