CC := gcc

DEFAULT_CFLAGS= -Wall -Wextra
CFLAGS := $(DEFAULT_CFLAGS) $(CFLAGS)
OBJS := lcp.o fcs.o code.o fcstable.o
EXEC := ppp

.PHONY: default clean

.SUFFIXES: .c .o

default: $(EXEC)

.c.o:
	$(CC) -o $@ -c $(CFLAGS) $<

fcstable.o: fcstable_gen
	./fcstable_gen > fcstable.c
	$(CC) -o $@ -c $(CFLAGS) fcstable.c

$(EXEC): ppp.c $(OBJS)
	$(CC) -o $@ $(CFLAGS) $< $(OBJS)

clean:
	rm -rf $(EXEC) fcstable_gen *.o
