#!/bin/bash
# test_threading.sh - Test script for multi-threaded web server
SERVER_URL="http://localhost:8003"
SPIN_URL="$SERVER_URL/cgi-bin/spin.cgi"

echo "===== Testing Multi-threaded Web Server ====="

# Test concurrent request handling
test_concurrent_requests() {
    local threads=$1
    echo -e "\n===== Testing with $threads threads ====="
    
    # Stop any running server
    pkill -f "wserver -p 8003" 2>/dev/null
    sleep 1
    
    # Start server with specified number of threads
    echo "Starting server with $threads threads..."
    ./wserver -p 8003 -t $threads -b 10 -s FIFO > /dev/null 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # Test 1: Sequential vs Parallel Performance
    echo -e "\nTest 1: Sequential vs Parallel Request Performance"
    
    # First, time 3 sequential 2-second requests
    echo "Running 3 sequential 2-second requests..."
    total_sequential=0
    for i in {1..3}; do
        start_time=$(date +%s.%N)
        curl -s "$SPIN_URL?2" > /dev/null
        end_time=$(date +%s.%N)
        request_time=$(echo "$end_time - $start_time" | bc)
        total_sequential=$(echo "$total_sequential + $request_time" | bc)
        echo "Sequential request $i took $request_time seconds"
    done
    echo "Total time for sequential requests: $total_sequential seconds"
    
    # Now, time 3 parallel 2-second requests
    echo -e "\nRunning 3 parallel 2-second requests..."
    parallel_start=$(date +%s.%N)
    
    # Start background processes for parallel requests
    for i in {1..3}; do
        curl -s "$SPIN_URL?2" > /dev/null &
        PIDS[$i]=$!
    done
    
    # Wait for all requests to complete
    for pid in ${PIDS[*]}; do
        wait $pid
    done
    
    parallel_end=$(date +%s.%N)
    parallel_total=$(echo "$parallel_end - $parallel_start" | bc)
    echo "Total time for parallel requests: $parallel_total seconds"
    
    # Calculate speedup
    speedup=$(echo "$total_sequential / $parallel_total" | bc -l)
    echo -e "\nSpeedup from parallel execution: $speedup"
    
    # Expected speedup should be close to min(3, threads) since we're running 3 parallel requests
    expected_speedup=$(( threads > 3 ? 3 : threads ))
    
    # Analyze results
    if (( $(echo "$speedup > $expected_speedup * 0.7" | bc -l) )); then
        echo "PASSED: Good parallel performance, speedup is approximately $speedup"
        echo "   (Should be close to min(3, $threads) = $expected_speedup)"
    else
        echo "FAILED: Poor parallel performance, speedup is only $speedup"
        echo "   (Should be close to min(3, $threads) = $expected_speedup)"
    fi
    
    # Stop the server
    echo "Stopping server..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    sleep 1
}

# Test with different thread counts
test_concurrent_requests 1
test_concurrent_requests 2
test_concurrent_requests 4

echo -e "\n===== Multi-threading tests completed ====="