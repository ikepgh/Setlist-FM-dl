CC = gcc -Wall
CFLAGS =
#-v -g -O3 -s
LIBS = -lcurl -lssl -lcrypto -ldl -lpthread -lxml2 -lxslt -lz -liconv -lm

all: Setlist-FM-dl

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

Setlist-FM-dl:
	$(CC) $(CFLAGS) setlistfmdl.c $(LIBS) -o Setlist-FM-dl
