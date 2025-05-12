#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>

#define MAXBUF 8192

//
// This program is intended to help test your web server.
// You can use it to test that you are correctly having multiple threads
// handling http requests.
// 

double get_seconds() {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double) t.tv_sec + (double) t.tv_usec / 1e6;
}

int main(int argc, char *argv[]) {
    // Extract arguments
    double sleep_time = 0.0;
    char *buf;
    
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        // Just expecting a single number
        sleep_time = (double) atoi(buf);
    } else if (argc > 1) {
        // Support direct command-line invocation for testing
        sleep_time = (double) atoi(argv[1]);
    }
    
    if (sleep_time <= 0) {
        sleep_time = 1.0; // Default to 1 second if no valid time specified
    }
    
    // Print HTTP headers when run as CGI
    if (getenv("QUERY_STRING") != NULL) {
        printf("Content-Type: text/html\r\n\r\n");
    }
    
    double start_time = get_seconds();
    
    // Output start marker and flush immediately to avoid buffering
    printf("<p>Starting to spin for %.2f seconds...</p>\r\n", sleep_time);
    fflush(stdout);
    
    // Use sleep for integer seconds
    if (sleep_time >= 1.0) {
        sleep((unsigned int)sleep_time);
    } else {
        // Use usleep for sub-second precision (usleep takes microseconds)
        usleep((unsigned int)(sleep_time * 1000000));
    }
    
    double end_time = get_seconds();
    
    // Make the response body
    printf("<p>Welcome to the CGI spin program</p>\r\n");
    printf("<p>My purpose is to waste time on the server!</p>\r\n");
    printf("<p>I was asked to spin for %.2f seconds</p>\r\n", sleep_time);
    printf("<p>I actually spun for %.2f seconds</p>\r\n", end_time - start_time);
    
    // Make sure the output is flushed
    fflush(stdout);
    
    return 0;
}