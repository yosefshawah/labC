CC = gcc
CFLAGS = -m32  -Wall -Werror -g
LDFLAGS =

all: myshell mypipeline looper

myshell: myshell.o LineParser.o
	$(CC) $(LDFLAGS) $^ -o $@
looper: looper.o
	$(CC) $(LDFLAGS) $< -o $@
mypipeline: mypipeline.o
	$(CC) $(LDFLAGS) $< -o $@

%.o: %.c LineParser.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o myshell mypipeline
