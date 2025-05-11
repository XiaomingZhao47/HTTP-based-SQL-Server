# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC = gcc
CFLAGS = -Wall -pthread
OBJS = wserver.o wclient.o request.o io_helper.o 
PORT = 8003

.SUFFIXES: .c .o 

all: wserver wclient spin.cgi sql.cgi install

wserver: wserver.o request.o io_helper.o
	$(CC) $(CFLAGS) -o wserver wserver.o request.o io_helper.o 

wclient: wclient.o io_helper.o
	$(CC) $(CFLAGS) -o wclient wclient.o io_helper.o

spin.cgi: spin.c
	$(CC) $(CFLAGS) -o spin.cgi spin.c

sql.cgi: sql.c io_helper.o
	$(CC) $(CFLAGS) -o sql.cgi sql.c io_helper.o

install: sql.cgi spin.cgi
	if [ ! -d cgi-bin ]; then mkdir -p cgi-bin; fi
	cp sql.cgi cgi-bin/
	chmod +x cgi-bin/sql.cgi
	cp spin.cgi cgi-bin/
	chmod +x cgi-bin/spin.cgi

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) wserver wclient spin.cgi sql.cgi cgi-bin/sql.cgi movies.dat schema.dat sql_test test_table.dat
	-rm -f concurrent_test.dat thread_test.dat concurrent_ops.dat *.log

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

# ====== Project 3 Main Testing Targets ======

# Setup all test scripts
setup-p3-tests: all
	-chmod +x test_fifo.sh test_sff.sh test_fifo_sff.sh test_threading.sh test_schedulers.sh test_sql_concurrent.sh run_p3_tests.sh 2>/dev/null || true

# Test threading capabilities
test-mt: all setup-p3-tests
	@echo "Testing multi-threading capabilities..."
	@./wserver -p $(PORT) -t 4 -b 16 -s FIFO & \
	sleep 1; \
	echo "Running 3 parallel requests (2 seconds each)..."; \
	time ( \
		curl -s "http://localhost:$(PORT)/cgi-bin/spin.cgi?2" > /dev/null & \
		curl -s "http://localhost:$(PORT)/cgi-bin/spin.cgi?2" > /dev/null & \
		curl -s "http://localhost:$(PORT)/cgi-bin/spin.cgi?2" > /dev/null & \
		wait \
	); \
	pkill -f "wserver -p $(PORT)"; \
	echo "Server stopped"; \
	echo "If multi-threading works, completion time should be ~2 seconds."

# Detailed threading test
test-threading: all setup-p3-tests
	./test_threading.sh || echo "Test execution failed, check the script path and permissions"

# Test FIFO scheduling
test-fifo: all setup-p3-tests
	./test_fifo.sh || echo "Test execution failed, check the script path and permissions"

# Test SFF scheduling
test-sff: all setup-p3-tests
	./test_sff.sh || echo "Test execution failed, check the script path and permissions"

# Test both schedulers (FIFO and SFF)
test-fifo-sff: all setup-p3-tests
	./test_fifo_sff.sh || echo "Test execution failed, check the script path and permissions"

# Test all schedulers with different configurations
test-schedulers: all setup-p3-tests
	./test_schedulers.sh || echo "Test execution failed, check the script path and permissions"

# Test concurrent SQL operations
test-sql-p3: all setup-p3-tests
	@echo "Testing concurrent SQL operations..."
	@./wserver -p $(PORT) -t 4 -b 16 -s FIFO & \
	sleep 1 ; \
	./test_sql_concurrent.sh || echo "Test execution failed, check the script path and permissions"; \
	pkill -f "wserver -p $(PORT)" ; \
	echo -e "\nServer stopped"

# Run all Project 3 tests
test-p3: all setup-p3-tests
	./run_p3_tests.sh || echo "Test execution failed, check the script path and permissions"

# Run a simple comprehensive test for Project 3
test-p3-simple: all setup-p3-tests
	@echo "============================================="
	@echo "         Project 3 Simple Test Suite"
	@echo "============================================="
	@echo "1. Testing multi-threading..."
	@make -s test-mt
	@echo "============================================="
	@echo "2. Testing FIFO scheduler..."
	@make -s test-fifo 
	@echo "============================================="
	@echo "3. Testing SFF scheduler with multiple threads..."
	@./wserver -p $(PORT) -t 2 -b 5 -s SFF & \
	sleep 1; \
	echo "Sending large request (5s) first..."; \
	curl -s "http://localhost:$(PORT)/cgi-bin/spin.cgi?5" > /dev/null & \
	LARGE_PID=$$!; \
	sleep 0.5; \
	echo "Sending small request (1s) immediately after..."; \
	time curl -s "http://localhost:$(PORT)/cgi-bin/spin.cgi?1" > /dev/null; \
	wait $$LARGE_PID; \
	pkill -f "wserver -p $(PORT)"; \
	echo "With SFF and multiple threads, small request should complete quickly (~1s)"
	@echo "============================================="
	@echo "Project 3 tests completed!"

# Start server with specific configuration for manual testing
start-server: wserver
	./wserver -p $(PORT) -t 4 -b 16 -s SFF & \
	echo "Server started on port $(PORT) with 4 threads, 16 buffers, SFF scheduling" ; \
	echo "Use 'make stop-server' to stop the server"

# Stop the server
stop-server:
	-pkill -f "wserver -p $(PORT)" || echo "No server running"

# Performance test with client
perf-test: wclient wserver setup-p3-tests
	./wserver -p $(PORT) -t 4 -b 16 -s SFF & \
	sleep 1 ; \
	echo "Running performance test with 8 threads and 50 requests..." ; \
	./wclient localhost $(PORT) 8 50 "/cgi-bin/spin.cgi?1,/cgi-bin/spin.cgi?2,/cgi-bin/spin.cgi?3" ; \
	pkill -f "wserver -p $(PORT)"