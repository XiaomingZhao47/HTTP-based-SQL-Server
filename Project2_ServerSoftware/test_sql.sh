#!/bin/bash
# test_sql.sh

SERVER_URL="http://localhost:8003/cgi-bin/sql.cgi"

# test CREATE TABLE
echo "Testing CREATE TABLE..."
curl -s "$SERVER_URL?CREATE%20TABLE%20movies%20(id%20smallint,%20title%20char(30),%20length%20int)"
echo ""

# test INSERT
echo "Testing INSERT..."
curl -s "$SERVER_URL?INSERT%20INTO%20movies%20VALUES%20(2,%20'Lyle,%20Lyle,%20Crocodile',%20100)"
echo ""

# test SELECT
echo "Testing SELECT..."
curl -s "$SERVER_URL?SELECT%20*%20FROM%20movies"
echo ""

# test UPDATE
echo "Testing UPDATE..."
curl -s "$SERVER_URL?UPDATE%20movies%20SET%20length%20=%20150%20WHERE%20id%20=%202"
echo ""

# test SELECT again to verify update
echo "Testing SELECT after UPDATE..."
curl -s "$SERVER_URL?SELECT%20*%20FROM%20movies"
echo ""

# test DELETE
echo "Testing DELETE..."
curl -s "$SERVER_URL?DELETE%20FROM%20movies%20WHERE%20id%20=%202"
echo ""

# test SELECT again to verify delete
echo "Testing SELECT after DELETE..."
curl -s "$SERVER_URL?SELECT%20*%20FROM%20movies"
echo ""

# test sample in description
echo "Testing the sample from description"

# SELECT statement
echo "Testing SELECT with length < 50..."
curl -s "$SERVER_URL?SELECT+title,+length+FROM+movies+WHERE+length+<+50"
echo -e "\n"

# INSERT and DELETE statements
echo "Testing INSERT for 'The Electric State'..."
curl -s "$SERVER_URL?INSERT+INTO+movies+VALUES+(0,+'The+Electric+State',+100)"
echo -e "\n"

echo "Testing INSERT for 'Wicked'..."
curl -s "$SERVER_URL?INSERT+INTO+movies+VALUES+(3,+'Wicked',+150)"
echo -e "\n"

echo "Testing INSERT for 'A Complete Unknown'..."
curl -s "$SERVER_URL?INSERT+INTO+movies+VALUES+(5,+'A+Complete+Unknown',+230)"
echo -e "\n"

echo "Testing SELECT after INSERTs..."
curl -s "$SERVER_URL?SELECT+*+FROM+movies"
echo -e "\n"

echo "Testing DELETE where length = 100..."
# curl -s "$SERVER_URL?DELETE+FROM+movies+WHERE+length+=+100"
echo -e "\n"

echo "Testing SELECT after DELETE..."
# curl -s "$SERVER_URL?SELECT+*+FROM+movies"
echo -e "\n"