#
# Makefile for fbv
#

CROSS	= 
CC	= gcc 
CFLAGS  = -O3 -Wall -c

SOURCES	= $(wildcard *.c)
OBJECTS	= $(SOURCES:.c=.o)
OUT	= fbplayer

LIBS	= -lgif -ljpeg -lpng

all: $(OUT)
	@echo Build DONE.

$(OUT): $(OBJECTS)
	$(CROSS)$(CC) -o $(OUT) $(OBJECTS) $(LIBS)

%.o:%.c
	$(CROSS)$(CC) -c -o $@ $^ $(CFLAGS) 

.PHONY:clean
clean:
	-rm -rf $(OBJECTS) $(OUT)
