#!/bin/bash
# test_schedulers.sh - Comprehensive test for different server schedulers
SERVER_URL="http://localhost:8003"
SPIN_URL="$SERVER_URL/cgi-bin/spin.cgi"
PORT=8003

echo "===== Testing Different Scheduler Implementations ====="

# Function to send a request and measure response time
send_request() {
    local duration=$1
    local identifier=$2
    local start=$(date +%s.%N)
    curl -s "$SPIN_URL?$duration" > /dev/null
    local end=$(date +%s.%N)
    local elapsed=$(echo "$end - $start" | bc)
    echo "$(date +%T.%N) - $identifier completed in $elapsed seconds (duration: ${duration}s)"
}

# Test a specific scheduler
test_scheduler() {
    local scheduler=$1
    local threads=$2
    echo -e "\n===== Testing '$scheduler' scheduler with $threads threads ====="
    
    # Stop any running server
    pkill -f "wserver -p 8003" 2>/dev/null
    sleep 1
    
    # Start server with specified scheduler and threads
    echo "Starting server with $scheduler scheduler, $threads threads, 5 buffers..."
    ./wserver -p 8003 -t $threads -b 5 -s $scheduler > /dev/null 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # For SFF with multiple threads, we need to test differently
    if [ "$scheduler" = "SFF" ] && [ $threads -gt 1 ]; then
        # Test: Send large request followed by small request
        echo -e "\nSending large and small requests (SFF multi-threaded test):"
        echo "- Sending large request (5s) first..."
        curl -s "$SPIN_URL?5" > /dev/null &
        LARGE_PID=$!
        sleep 0.5  # Wait to ensure large request is being processed
        
        echo "- Sending small request (1s) second..."
        start_time=$(date +%s.%N)
        curl -s "$SPIN_URL?1" > /dev/null
        end_time=$(date +%s.%N)
        small_time=$(echo "$end_time - $start_time" | bc)
        echo "Small request (1s) completed in $small_time seconds"
        
        # Wait for large request to complete
        wait $LARGE_PID
        echo "Large request (5s) completed"
        
        # Analysis
        echo -e "\nTest Results Analysis:"
        echo "- Small request completion time: $small_time seconds"
        
        if (( $(echo "$small_time < 2.0" | bc -l) )); then
            echo "PASSED: With SFF scheduler and multiple threads, small request completed quickly"
            echo "   Small request completed in $small_time seconds (expected < 2.0s)"
        else
            echo "FAILED: With SFF scheduler and multiple threads, small request should complete quickly"
            echo "   Small request took $small_time seconds (expected < 2.0s)"
        fi
    
    # Regular test for FIFO or SFF with 1 thread
    else
        # Test: Send requests of different sizes in a specific order
        echo -e "\nSending mixed workload:"
        
        # First send a large request
        echo "- Sending large request (5s)..."
        curl -s "$SPIN_URL?5" > /dev/null &
        LARGE_PID=$!
        sleep 0.2
        
        # Then send a small request
        echo "- Sending small request (1s)..."
        start_time=$(date +%s.%N)
        curl -s "$SPIN_URL?1" > /dev/null
        end_time=$(date +%s.%N)
        small_time=$(echo "$end_time - $start_time" | bc)
        echo "Small request (1s) completed in $small_time seconds"
        
        # Wait for large request to complete
        wait $LARGE_PID
        echo "Large request (5s) completed"
        
        # Analysis based on scheduler
        echo -e "\nTest Results Analysis:"
        echo "- Small request completion time: $small_time seconds"
        
        if [ "$scheduler" = "FIFO" ]; then
            if [ $threads -eq 1 ]; then
                # For FIFO with 1 thread, small request should wait for large request
                if (( $(echo "$small_time > 4.5" | bc -l) )); then
                    echo "PASSED: With FIFO scheduler, requests completed in order they were received"
                    echo "   Small request waited for previous requests as expected"
                else
                    echo "FAILED: With FIFO scheduler, small request should wait for previous requests"
                    echo "   Small request completed too quickly (took $small_time seconds)"
                fi
            else
                # For FIFO with multiple threads, behavior is less predictable
                echo "- With multiple threads, FIFO scheduler assigns requests to threads in order"
                echo "- Actual completion order depends on thread availability and execution time"
            fi
        elif [ "$scheduler" = "SFF" ] && [ $threads -eq 1 ]; then
            # For SFF with 1 thread (non-preemptive), small request must wait
            if (( $(echo "$small_time > 4.5" | bc -l) )); then
                echo "PASSED: With SFF scheduler and 1 thread, small request had to wait (non-preemptive scheduling)"
                echo "   Small request took $small_time seconds (expected > 4.5s with non-preemptive scheduling)"
            else
                echo "UNEXPECTED: With SFF scheduler and 1 thread, small request should wait due to non-preemptive scheduling"
                echo "   Small request took $small_time seconds (expected > 4.5s)"
            fi
        fi
    fi
    
    # Stop the server
    echo "Stopping server..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    sleep 1
    
    echo "Test completed for '$scheduler' scheduler"
}

# Run tests for the implemented schedulers
echo -e "\n======= Testing FIFO Scheduler (1 thread) ======="
test_scheduler FIFO 1

echo -e "\n======= Testing SFF Scheduler (1 thread) ======="
test_scheduler SFF 1

echo -e "\n======= Testing FIFO Scheduler (4 threads) ======="
test_scheduler FIFO 4

echo -e "\n======= Testing SFF Scheduler (4 threads) ======="
test_scheduler SFF 4

echo -e "\n===== All scheduler tests completed ====="