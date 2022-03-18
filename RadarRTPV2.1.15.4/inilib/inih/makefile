################## Makefile for microradarRTP ################

CC = gcc
CFLAGS =  -P -Wall -fPIC 
INCLUDEDIR = /usr/local/include
LIBDIR   = /usr/local/lib 

INIOBJS = ini.o INIReader.o

all : libini.a makefile
.PHONY : all


.cpp.o :
	$(CC) $(CFLAGS) -c  $< 


libini.a : $(INIOBJS)
	ar rcs libini.a $^ 
	cp libini.a $(LIBDIR)
	cp *.h $(INCLUDEDIR)

.PHONY: clean
clean :
	-rm -f *.o a.out core 

################################################################
