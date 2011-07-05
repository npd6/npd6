CC=gcc
CFLAGS=-c -Wall -g 
LDFLAGS=
SOURCES=main.c icmp6.c util.c ip6.c
OBJECTS=$(SOURCES:.c=.o)
HEADERS=includes.h npd6.h
EXECUTABLE=npd6

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)
