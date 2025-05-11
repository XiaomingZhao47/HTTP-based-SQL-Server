#!/bin/bash
# test_fifo.sh - Test script specifically for FIFO scheduler
SERVER_URL="http://localhost:8003"
SPIN_URL="$SERVER_URL/cgi-bin/spin.cgi"
PORT=8003

echo "===== Testing FIFO Scheduler ====="

# Stop any running server
pkill -f "wserver -p $PORT" 2>/dev/null
sleep 1

# Start server with FIFO scheduler (1 thread, 5 buffers)
echo "Starting server with FIFO scheduler (1 thread, 5 buffers)..."
./wserver -p $PORT -t 1 -b 5 -s FIFO > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

# Test: Small request should wait for large request with FIFO
echo -e "\nTest: Request Order with FIFO"
echo "Sending large request (5s) first, then small request (1s) immediately after"

# Send large request first
curl -s "$SPIN_URL?5" > /dev/null &
LARGE_PID=$!
# Wait to ensure large request is in buffer
sleep 0.5

# Send small request and measure its completion time
echo "Sending small request (1s)..."
start_time=$(date +%s.%N)
curl -s "$SPIN_URL?1" > /dev/null
end_time=$(date +%s.%N)
small_time=$(echo "$end_time - $start_time" | bc)
echo "Small request completed in $small_time seconds"

# Wait for large request to complete
wait $LARGE_PID
echo "Large request completed"

# Analysis of results
echo -e "\nAnalysis:"
if (( $(echo "$small_time > 4.5" | bc -l) )); then
    echo "✅ PASSED: With FIFO, small request waited for large request to complete first"
    echo "   Small request took $small_time seconds (expected > 4.5s)"
else
    echo "❌ FAILED: With FIFO, small request should wait for large request"
    echo "   Small request took $small_time seconds (expected > 4.5s)"
fi

# Stop the server
echo -e "\nStopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
sleep 1

echo "FIFO scheduler test completed!"