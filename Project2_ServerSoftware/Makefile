# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
CFLAGS = -Wall
OBJS = wserver.o wclient.o request.o io_helper.o 
PORT = 8003

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
	if [ ! -d cgi-bin ]; then mkdir -p cgi-bin; fi
	cp sql.cgi cgi-bin/
	chmod +x cgi-bin/sql.cgi

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) wserver wclient spin.cgi sql.cgi cgi-bin/sql.cgi movies.dat schema.dat

unit_test: sql.c
	$(CC) $(CFLAGS) -DUNIT_TEST -o sql_test sql.c

setup-test: sql.cgi
	chmod +x test_sql.sh

test: setup-test
	./wserver -p $(PORT) & \
	echo "server started on port $(PORT)" ; \
	sleep 1 ; \
	./test_sql.sh ; \
	pkill -f "wserver -p $(PORT)" ; \
	echo "server stopped"

test-bg: setup-test
	./wserver -p $(PORT) & \
	echo "server started on port $(PORT)" ; \
	sleep 1 ; \
	echo "run ./test_sql.sh to test SQL commands"