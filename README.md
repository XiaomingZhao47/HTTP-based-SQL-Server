# HTTP-based-SQL-Server

## Overview

This project implements a simple SQL database system with a web interface. It supports basic SQL operations including CREATE TABLE, INSERT, SELECT, UPDATE, and DELETE commands. The system uses a file-based storage approach with fixed-size blocks (256b) and supports three data types: CHAR, SMALLINT, and INTEGER.

## Building the Project

To compile the project, simply run:

```
make
```

This will build the following components:
- `wserver`: A web server to handle HTTP requests
- `wclient`: A client for testing the web server
- `spin.cgi`: A CGI program for testing purposes
- `sql.cgi`: The main SQL processing CGI program

The Makefile will also install `sql.cgi` into the `cgi-bin` directory

## Running the System

1. Start the web server:

```
./wserver -p 8003
```

The default port is 8003, but you can change it in the Makefile.

2. Once the server is running, you can interact with the SQL system through your web browser:

```
http://localhost:8003/cgi-bin/sql.cgi?SQL_COMMAND
```

Replace `SQL_COMMAND` with a URL-encoded SQL query.

## Supported SQL Commands

### CREATE TABLE
Creates a new table with specified columns

```
CREATE TABLE table_name (column1 type1, column2 type2, ...)
```

Example:
```
CREATE TABLE movies (id smallint, title char(30), length int)
```

### INSERT INTO & INSERT
Adds a new record to a table
```
INSERT INTO table_name VALUES (value1, value2, ...)
```
Example:
```
INSERT INTO movies VALUES (1, 'The Matrix', 136)
```
### SELECT
Retrieves data from a table.
```
SELECT column1, column2, ... FROM table_name [WHERE condition]
```

Example:
```
SELECT * FROM movies WHERE id = 1
```

### UPDATE
Modifies existing records in a table
```
UPDATE table_name SET column1 = value1 [WHERE condition]
```

Example:
```
UPDATE movies SET length = 150 WHERE id = 1
```

### DELETE
Removes records from a table
```
DELETE FROM table_name [WHERE condition]
```

Example:
```
DELETE FROM movies WHERE id = 1
```

## Testing

Run the automated tests with
```
make test
```
This will start the server, run a series of test SQL commands, and stop the server

For manual testing, start the server in the background
```
make test-bg
```
Then run the test script manually
```
./test_sql.sh
```
## Unit Testing

To run unit tests without using any server functionality
```
make unit_test
./sql_test
```
## Cleaning Up

To remove compiled binaries and database files
```
make clean
```

## File Storage

The system stores data in the following files
- `schema.dat`: Contains table schemas
- `<table_name>.dat`: Contains the data for each table




