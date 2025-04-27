# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
CFLAGS = -Wall
OBJS = wserver.o wclient.o request.o io_helper.o 

.SUFFIXES: .c .o 

all: wserver wclient spin.cgi sql.cgi

wserver: wserver.o request.o io_helper.o
	$(CC) $(CFLAGS) -o wserver wserver.o request.o io_helper.o 

wclient: wclient.o io_helper.o
	$(CC) $(CFLAGS) -o wclient wclient.o io_helper.o

spin.cgi: spin.c
	$(CC) $(CFLAGS) -o spin.cgi spin.c

sql.cgi: sql.c io_helper.o
	$(CC) $(CFLAGS) -o sql.cgi sql.c io_helper.o

install: sql.cgi
	cp sql.cgi cgi-bin/
	chmod +x cgi-bin/sql.cgi

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) wserver wclient spin.cgi sql.cgi cgi-bin/sql.cgi


sql.cgi: sql.c
	$(CC) $(CFLAGS) -o sql.cgi sql.c

unit_test: sql.c
	$(CC) $(CFLAGS) -DUNIT_TEST -o sql_test sql.c