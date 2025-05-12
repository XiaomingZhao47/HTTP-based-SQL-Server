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

## Multi-Threaded Web Server (Project 3)

The web server now supports multi-threading and scheduling algorithms to efficiently handle multiple simultaneous requests.

### Command Line Options

The web server can be started with the following options:

```
./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
```

- `-d basedir`: The root directory from where the web server should operate (default: current directory)
- `-p port`: The port number for the web server to listen on (default: 10000)
- `-t threads`: The number of worker threads to create (default: 1)
- `-b buffers`: The number of request connections that can be accepted at one time (default: 1)
- `-s schedalg`: The scheduling algorithm to use (FIFO or SFF, default: FIFO)

Example:
```
./wserver -p 8003 -t 4 -b 16 -s SFF
```

### Scheduling Algorithms

#### FIFO (First-In-First-Out)
Processes requests in the order they are received. When a worker thread becomes available, it handles the oldest request in the buffer.

#### SFF (Smallest File First)
Prioritizes requests for smaller files. When a worker thread becomes available, it handles the request with the smallest file size in the buffer. This can improve overall throughput, especially for mixed workloads with varying file sizes.

### Testing the Multi-Threaded Server

The project includes various tests to verify the multi-threading and scheduling functionality:

```
make test-mt           # Test basic multi-threading capabilities
make test-fifo         # Test FIFO scheduler
make test-sff          # Test SFF scheduler
make test-fifo-sff     # Compare FIFO and SFF schedulers
make test-schedulers   # Comprehensive scheduler testing
make test-sql-p3       # Test concurrent SQL operations
make test-p3           # Run all Project 3 tests
make test-p3-simple    # Run simplified Project 3 tests
make test-all-p3       # Run the full Project 3 test suite
```

For performance testing with multiple threads and requests:
```
make perf-test
```

### Starting Server for Manual Testing

To start the server with a specific configuration:
```
make start-server      # Starts with 4 threads, 16 buffers, SFF scheduling
```

Stop the server when done:
```
make stop-server
```

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
## Remote Testing

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