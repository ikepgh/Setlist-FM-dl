CC = ppc-morphos-gcc-9 -Wall -g -v
CFLAGS =  `xml2-config --cflags` `xslt-config --cflags`
#-v -g -O3 -s   2.95.3  -lxml2 -lz -liconv -lm -lxslt
LIBS = -lcurl  -lnghttp2 -ldl -lssl -lcrypto -lpthread `xml2-config --libs` `xslt-config --libs` -noixemul

all: Setlist-FM-dl

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

Setlist-FM-dl:
	$(CC) $(CFLAGS) setlistfmdl.c $(LIBS) -o Setlist-FM-dl
