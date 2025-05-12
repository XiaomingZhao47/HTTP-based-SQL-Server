#!/bin/bash
# test_sff.sh - Test script specifically for SFF scheduler
SERVER_URL="http://localhost:8003"
SPIN_URL="$SERVER_URL/cgi-bin/spin.cgi"
PORT=8003

echo "===== Testing SFF Scheduler Implementation ====="

# Stop any running server
pkill -f "wserver -p $PORT" 2>/dev/null
sleep 1

# Test 1: Single thread non-preemptive SFF (demonstrating the limitation)
echo -e "\nTest 1: Single Thread SFF - Non-preemptive Limitation"
echo "Starting server with SFF scheduler (1 thread, 5 buffers)..."
./wserver -p $PORT -t 1 -b 5 -s SFF > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

echo "Sending medium request (3s) first..."
curl -s "$SPIN_URL?3" > /dev/null &
MEDIUM_PID=$!
sleep 0.5

echo "Sending small request (1s) second..."
start_time=$(date +%s.%N)
curl -s "$SPIN_URL?1" > /dev/null
end_time=$(date +%s.%N)
small_time=$(echo "$end_time - $start_time" | bc)
echo "Small request completed in $small_time seconds"

# Wait for all requests to complete
wait $MEDIUM_PID
echo "Medium request completed"


if (( $(echo "$small_time > 2.5" | bc -l) )); then
    echo "Correct behavior for non-preemptive scheduling with 1 thread"
else
    echo "Unexpected behavior - small request didn't wait for medium request"
fi

# Stop the server
kill $SERVER_PID
sleep 1

# Test 2: Multi-thread SFF (proper testing of SFF scheduling)
echo -e "\nTest 2: Multi-Thread SFF - Proper Scheduling Test"
echo "Starting server with SFF scheduler (2 threads, 5 buffers)..."
./wserver -p $PORT -t 2 -b 5 -s SFF > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

echo "Sending large request (5s) first..."
curl -s "$SPIN_URL?5" > /dev/null &
LARGE_PID=$!
sleep 0.5

echo "Sending medium request (3s) second..."
curl -s "$SPIN_URL?3" > /dev/null &
MEDIUM_PID=$!
sleep 0.5

echo "Sending small request (1s) last..."
start_time=$(date +%s.%N)
curl -s "$SPIN_URL?1" > /dev/null
end_time=$(date +%s.%N)
small_time=$(echo "$end_time - $start_time" | bc)
echo "Small request completed in $small_time seconds"

# Wait for all requests to complete
wait $MEDIUM_PID $LARGE_PID
echo "All requests completed"

# Analysis
echo "Expected: small request takes ~1s to complete"
echo "Actual: small request took $small_time seconds"
if (( $(echo "$small_time < 2.0" | bc -l) )); then
    echo "PASSED: SFF is working correctly in multi-threaded mode!"
    echo "    Small request completed quickly (~1s) as expected"
else
    echo "FAILED: SFF is not working correctly in multi-threaded mode"
    echo "    Small request took too long ($small_time seconds)"
fi

# Stop the server
kill $SERVER_PID
sleep 1

echo -e "\n===== SFF Scheduler Test Completed ====="
echo "Conclusion: The SFF scheduler should prioritize the smallest request"
echo "when multiple worker threads are available."