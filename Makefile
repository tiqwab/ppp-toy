CC := gcc

CFLAGS := $(CFLAGS)
EXEC := ppp

.PHONY: default clean

default: $(EXEC)

$(EXEC): ppp.c
	$(CC) -o $@ -Wall -Wextra $(CFLAGS) $<

clean:
	rm -rf $(EXEC) *.o
