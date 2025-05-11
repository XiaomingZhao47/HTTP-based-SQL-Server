#!/bin/bash
# run_p3_tests.sh - Run all Project 3 tests
PORT=8003

echo "==============================================="
echo "    Project 3 Comprehensive Test Suite"
echo "==============================================="

# Make sure no server is running
pkill -f "wserver -p $PORT" 2>/dev/null
sleep 1

# Make scripts executable
chmod +x test_fifo_sff.sh
chmod +x test_schedulers.sh
chmod +x test_sql_concurrent.sh
chmod +x test_threading.sh

# Test 1: Threading test
echo -e "\n===== Running Multi-threading Test ====="
echo "Starting server with 4 threads..."
./wserver -p $PORT -t 4 -b 16 -s FIFO &
SERVER_PID=$!
sleep 1

./test_threading.sh

echo "Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
sleep 1

# Test 2: Scheduler comparison test
echo -e "\n===== Running FIFO vs SFF Scheduler Test ====="
./test_fifo_sff.sh

# Test 3: Comprehensive scheduler test
echo -e "\n===== Running Comprehensive Scheduler Test ====="
./test_schedulers.sh

# Test 4: SQL Concurrency test
echo -e "\n===== Running SQL Concurrency Test ====="
echo "Starting server with 4 threads..."
./wserver -p $PORT -t 4 -b 16 -s FIFO &
SERVER_PID=$!
sleep 1

./test_sql_concurrent.sh

echo "Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
sleep 1

echo -e "\n==============================================="
echo "    All Project 3 Tests Completed"
echo "==============================================="
echo "Check results above to ensure your implementation meets requirements:"
echo "1. Multi-threading: Server should handle concurrent requests in parallel"
echo "2. FIFO Scheduler: Requests should complete in the order received"
echo "3. SFF Scheduler: Shorter requests should complete before longer ones"
echo "4. Thread Safety: Concurrent database operations should work correctly"
echo "==============================================="