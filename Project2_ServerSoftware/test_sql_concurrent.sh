#!/bin/bash
# test_sql_concurrent.sh - Test script for concurrent SQL server requests
SERVER_URL="http://localhost:8003/cgi-bin/sql.cgi"
SPIN_URL="http://localhost:8003/cgi-bin/spin.cgi"

echo "===== Testing Concurrent SQL Requests ====="

# Setup: Create a test table
echo -e "\nCreating test table..."
curl -s "$SERVER_URL?CREATE%20TABLE%20concurrent_test%20(id%20smallint,%20name%20char(30),%20value%20int)"
echo

# Insert initial data
echo -e "\nInserting initial data..."
curl -s "$SERVER_URL?INSERT%20INTO%20concurrent_test%20VALUES%20(1,%20'Test%20One',%2010)"
curl -s "$SERVER_URL?INSERT%20INTO%20concurrent_test%20VALUES%20(2,%20'Test%20Two',%2020)"
curl -s "$SERVER_URL?INSERT%20INTO%20concurrent_test%20VALUES%20(3,%20'Test%20Three',%2030)"
echo

# Function to send a request that will take some time (using spin.cgi)
# This simulates a long-running query
send_long_request() {
    local id=$1
    local sleep_time=$2
    echo "Starting long request $id (duration: ${sleep_time}s)..."
    curl -s "$SPIN_URL?$sleep_time" > /dev/null &
    echo "Request $id sent to background"
    return $!  # Return PID of background process
}

# Function to perform a quick SQL query while long requests are running
perform_quick_query() {
    local query=$1
    local description=$2
    echo -e "\nPerforming quick query: $description"
    local start_time=$(date +%s.%N)
    local result=$(curl -s "$SERVER_URL?$query")
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    echo "$result"
    echo "Query completed in $duration seconds"
}

# Test 1: Multiple concurrent long-running requests
echo -e "\n===== Test 1: Multiple concurrent requests ====="
send_long_request 1 5
PID1=$!
send_long_request 2 3
PID2=$!
send_long_request 3 4
PID3=$!

# While those are running, perform some quick SQL queries
sleep 1  # Brief pause to ensure long requests have started

# Test SELECT while long requests are running
perform_quick_query "SELECT%20*%20FROM%20concurrent_test" "SELECT all records"

# Test UPDATE while long requests are running
perform_quick_query "UPDATE%20concurrent_test%20SET%20value%20=%2050%20WHERE%20id%20=%201" "UPDATE record"

# Test INSERT while long requests are running
perform_quick_query "INSERT%20INTO%20concurrent_test%20VALUES%20(4,%20'Test%20Four',%2040)" "INSERT new record"

# Wait for all background processes to complete
echo -e "\nWaiting for all requests to complete..."
wait $PID1 $PID2 $PID3
echo "All long requests completed"

# Verify final state
echo -e "\n===== Final state verification ====="
perform_quick_query "SELECT%20*%20FROM%20concurrent_test" "SELECT all records"

# Test 2: Load test with multiple similar requests
echo -e "\n===== Test 2: Load test with similar requests ====="

# Create an array to store PIDs
PIDS=()

# Send multiple SELECT requests in parallel
echo "Sending 10 parallel SELECT requests..."
for i in {1..10}; do
    curl -s "$SERVER_URL?SELECT%20*%20FROM%20concurrent_test" > /dev/null &
    PIDS+=($!)
done

# Wait for all background processes to complete
for pid in "${PIDS[@]}"; do
    wait $pid
done
echo "All SELECT requests completed"

# Clean up
echo -e "\n===== Cleanup ====="
curl -s "$SERVER_URL?DELETE%20FROM%20concurrent_test"
echo -e "\nTest completed successfully!"