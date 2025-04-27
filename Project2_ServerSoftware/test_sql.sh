#!/bin/bash
# test_sql.sh

SERVER_URL="http://localhost:8003/cgi-bin/sql.cgi"

# Test CREATE TABLE
echo "Testing CREATE TABLE..."
curl -s "$SERVER_URL?CREATE%20TABLE%20movies%20(id%20smallint,%20title%20char(30),%20length%20int)"
echo ""

# Test INSERT
echo "Testing INSERT..."
curl -s "$SERVER_URL?INSERT%20INTO%20movies%20VALUES%20(2,%20'Lyle,%20Lyle,%20Crocodile',%20100)"
echo ""

# Test SELECT
echo "Testing SELECT..."
curl -s "$SERVER_URL?SELECT%20*%20FROM%20movies"
echo ""

# Test UPDATE
echo "Testing UPDATE..."
curl -s "$SERVER_URL?UPDATE%20movies%20SET%20length%20=%20150%20WHERE%20id%20=%202"
echo ""

# Test SELECT again to verify update
echo "Testing SELECT after UPDATE..."
curl -s "$SERVER_URL?SELECT%20*%20FROM%20movies"
echo ""

# Test DELETE
echo "Testing DELETE..."
curl -s "$SERVER_URL?DELETE%20FROM%20movies%20WHERE%20id%20=%202"
echo ""

# Test SELECT again to verify delete
echo "Testing SELECT after DELETE..."
curl -s "$SERVER_URL?SELECT%20*%20FROM%20movies"
echo ""