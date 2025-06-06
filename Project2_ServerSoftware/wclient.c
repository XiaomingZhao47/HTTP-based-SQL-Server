//
// client.c: A very, very primitive HTTP client.
//
// To run, try:
//      client hostname portnumber filename
//
// Sends one HTTP request to the specified HTTP server.
// Prints out the HTTP response.
//
// For testing your server, you will want to modify this client.
// For example:
// You may want to make this multi-threaded so that you can
// send many requests simultaneously to the server.
//
// You may also want to be able to request different URIs;
// you may want to get more URIs from the command line
// or read the list from a file.
//
// When we test your server, we will be using modifications to this client.
//

#include "io_helper.h"
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MAXBUF (8192)
#define MAX_FILES 100
#define MAX_THREADS 100

// struct to hold request parameters for worker threads
typedef struct
{
  char *host;
  int port;
  char *filename;
  int thread_id;
  int request_id;
  struct timeval start_time;
  struct timeval end_time;
  int response_size;
} request_params;

// mutex for printing results
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * sends an HTTP request for the specified file to the connected server
 * formulates a simple HTTP GET request including the hostname and sends it through
 * the provided file descriptor
 *
 * @param fd the file descriptor for the client connection to the server
 * @param filename the URI/path of the file to request from the server
 */
void client_send(int fd, char *filename)
{
  char buf[MAXBUF];
  char hostname[MAXBUF];

  gethostname_or_die(hostname, MAXBUF);

  /* form and send the HTTP request */
  sprintf(buf, "GET %s HTTP/1.1\n", filename);
  sprintf(buf, "%shost: %s\n\r\n", buf, hostname);
  write_or_die(fd, buf, strlen(buf));
}

/**
 * reads the HTTP response from the server and calculates its total size
 * first processes the HTTP header by reading until an empty line is encountered,
 * then reads the response body until end of data
 *
 * @param fd the file descriptor for the client connection to the server
 * @return the total size of the HTTP response in bytes
 */
int client_read(int fd)
{
  char buf[MAXBUF];
  int n;
  int total_size = 0;

  // read and process the HTTP Header
  n = readline_or_die(fd, buf, MAXBUF);
  while (strcmp(buf, "\r\n") && (n > 0))
  {
    total_size += n;
    n = readline_or_die(fd, buf, MAXBUF);
  }

  // read and process the HTTP Body
  n = readline_or_die(fd, buf, MAXBUF);
  while (n > 0)
  {
    total_size += n;
    n = readline_or_die(fd, buf, MAXBUF);
  }

  return total_size;
}

/**
 * thread function that handles a single HTTP request
 * establishes a connection to the server, sends the request, reads the response
 * calculates timing metrics, and outputs the results in a thread-safe manner
 *
 * @param arg pointer to a request_params structure containing request parameters
 * @return NULL (thread terminates after completing the request)
 */
void *request_thread(void *arg)
{
  request_params *params = (request_params *)arg;
  int clientfd;

  // record start time
  gettimeofday(&params->start_time, NULL);

  // connection to the specified host and port
  clientfd = open_client_fd_or_die(params->host, params->port);

  // send the request
  client_send(clientfd, params->filename);

  // read the response
  params->response_size = client_read(clientfd);

  // record end time
  gettimeofday(&params->end_time, NULL);

  // calculate response time in milliseconds
  long response_time = (params->end_time.tv_sec - params->start_time.tv_sec) * 1000 +
                       (params->end_time.tv_usec - params->start_time.tv_usec) / 1000;

  // print results with mutex protection to avoid interleaved output
  pthread_mutex_lock(&print_mutex);
  printf("Thread %d, Request %d: %s - Response size: %d bytes, Time: %ld ms\n",
         params->thread_id, params->request_id, params->filename,
         params->response_size, response_time);
  pthread_mutex_unlock(&print_mutex);

  close_or_die(clientfd);
  return NULL;
}

/**
 * main function that initializes and manages the HTTP client
 * parses command-line arguments, validates input parameters, processes the list of files to request
 * creates and manages worker threads to send HTTP requests, and displays performance metrics
 *
 * Command-line options
 * <host>            : the hostname or IP address of the HTTP server
 * <port>            : the port number of the HTTP server
 * <num_threads>     : the number of concurrent threads to use
 * <num_requests>    : the total number of requests to send
 * <file1,file2,...> : comma-separated list of files to request
 *
 * @param argc number of command-line arguments
 * @param argv array of command-line argument strings
 * @return 0 on success
 */
int main(int argc, char *argv[])
{
  char *host;
  int port;
  int num_threads;
  int num_requests;
  char *files_arg;
  char *files[MAX_FILES];
  int num_files = 0;

  if (argc != 6)
  {
    fprintf(stderr, "Usage: %s <host> <port> <num_threads> <num_requests> <file1,file2,...>\n", argv[0]);
    exit(1);
  }

  host = argv[1];
  port = atoi(argv[2]);
  num_threads = atoi(argv[3]);
  num_requests = atoi(argv[4]);
  files_arg = argv[5];

  // Validate input parameters
  if (num_threads <= 0 || num_threads > MAX_THREADS)
  {
    fprintf(stderr, "Number of threads must be between 1 and %d\n", MAX_THREADS);
    exit(1);
  }

  if (num_requests <= 0)
  {
    fprintf(stderr, "Number of requests must be positive\n");
    exit(1);
  }

  // Parse comma-separated list of files
  char *token = strtok(files_arg, ",");
  while (token != NULL && num_files < MAX_FILES)
  {
    files[num_files++] = token;
    token = strtok(NULL, ",");
  }

  if (num_files == 0)
  {
    fprintf(stderr, "No files specified\n");
    exit(1);
  }

  printf("Client starting: %d threads sending %d requests to %s:%d\n",
         num_threads, num_requests, host, port);

  // Create thread parameters and thread objects
  pthread_t threads[MAX_THREADS];
  request_params *params = (request_params *)malloc(num_threads * sizeof(request_params));

  if (params == NULL)
  {
    fprintf(stderr, "Failed to allocate memory for request parameters\n");
    exit(1);
  }

  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);

  // Start threads to send requests
  int request_count = 0;
  while (request_count < num_requests)
  {
    int threads_to_launch = (num_requests - request_count < num_threads) ? (num_requests - request_count) : num_threads;

    // Launch a batch of threads
    for (int i = 0; i < threads_to_launch; i++)
    {
      params[i].host = host;
      params[i].port = port;
      params[i].filename = files[request_count % num_files]; // Cycle through files
      params[i].thread_id = i;
      params[i].request_id = request_count++;

      if (pthread_create(&threads[i], NULL, request_thread, &params[i]) != 0)
      {
        fprintf(stderr, "Failed to create thread %d\n", i);
        exit(1);
      }
    }

    // Wait for all threads in this batch to complete
    for (int i = 0; i < threads_to_launch; i++)
    {
      pthread_join(threads[i], NULL);
    }
  }

  gettimeofday(&end_time, NULL);

  long total_time = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                    (end_time.tv_usec - start_time.tv_usec) / 1000;

  printf("\nSummary:\n");
  printf("Total requests: %d\n", num_requests);
  printf("Total time: %ld ms\n", total_time);
  printf("Requests per second: %.2f\n", (float)num_requests * 1000 / total_time);

  free(params);
  exit(0);
}