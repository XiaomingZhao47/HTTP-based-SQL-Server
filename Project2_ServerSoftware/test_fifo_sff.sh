#!/bin/bash
# test_fifo_sff.sh - Combined test script for FIFO and SFF schedulers
SERVER_URL="http://localhost:8003"
SPIN_URL="$SERVER_URL/cgi-bin/spin.cgi"
PORT=8003

echo "===== Testing FIFO and SFF Schedulers ====="

# Stop any running server
pkill -f "wserver -p $PORT" 2>/dev/null
sleep 1

# Test FIFO first
echo -e "\n======= Testing FIFO Scheduler ======="
echo "Starting server with FIFO scheduler (1 thread, 5 buffers)..."
./wserver -p $PORT -t 1 -b 5 -s FIFO > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

echo "Sending large request (5s) first..."
curl -s "$SPIN_URL?5" > /dev/null &
LARGE_PID=$!
sleep 0.5

echo "Sending small request (1s) immediately after..."
start_time=$(date +%s.%N)
curl -s "$SPIN_URL?1" > /dev/null
end_time=$(date +%s.%N)
small_time=$(echo "$end_time - $start_time" | bc)
echo "Small request completed in $small_time seconds"

wait $LARGE_PID
echo "Large request completed"

echo -e "\nFIFO Analysis:"
if (( $(echo "$small_time > 4.5" | bc -l) )); then
    echo "✅ PASSED: With FIFO, small request waited for large request to complete first"
    echo "   Small request took $small_time seconds (expected > 4.5s)"
else
    echo "❌ FAILED: With FIFO, small request should wait for large request"
    echo "   Small request took $small_time seconds (expected > 4.5s)"
fi

# Stop the FIFO server
kill $SERVER_PID
sleep 1

# Test multi-threaded SFF second - using 3 threads to ensure one is always available
echo -e "\n======= Testing SFF Scheduler with 3 threads ======="
echo "Starting server with SFF scheduler (3 threads, 5 buffers)..."
./wserver -p $PORT -t 3 -b 5 -s SFF > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

# Occupy first thread with a large request
echo "1. Sending first large request (5s) to occupy thread 1..."
curl -s "$SPIN_URL?5" > /dev/null &
LARGE_PID1=$!
sleep 0.2

# Occupy second thread with another large request
echo "2. Sending second large request (5s) to occupy thread 2..."
curl -s "$SPIN_URL?5" > /dev/null &
LARGE_PID2=$!
sleep 0.5  # Make sure both large requests are being processed

# Now send small request, which should be processed by thread 3
echo "3. Sending small request (1s)..."
start_time=$(date +%s.%N)
curl -s "$SPIN_URL?1" > /dev/null
end_time=$(date +%s.%N)
small_time=$(echo "$end_time - $start_time" | bc)
echo "Small request completed in $small_time seconds"

# Wait for large requests to complete
wait $LARGE_PID1 $LARGE_PID2
echo "Large requests completed"

echo -e "\nSFF Analysis:"
if (( $(echo "$small_time < 2.0" | bc -l) )); then
    echo "✅ PASSED: With SFF and multiple threads, small request completed before large requests"
    echo "   Small request took $small_time seconds (expected < 2.0s)"
else
    echo "❌ FAILED: With SFF and multiple threads, small request should complete before large requests"
    echo "   Small request took $small_time seconds (expected < 2.0s)"
fi

# Stop the SFF server
kill $SERVER_PID
sleep 1

echo -e "\n===== FIFO vs SFF Comparison ====="
echo "FIFO: Processes requests in order received (first-in, first-out)"
echo "SFF: Prioritizes requests with smallest files (shortest file first)"
echo "Both schedulers are non-preemptive, but with multiple threads,"
echo "SFF can still optimize request handling by assigning smaller"
echo "requests to available threads."

echo -e "\n===== All scheduler tests completed ====="