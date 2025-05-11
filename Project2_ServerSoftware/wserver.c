#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>  // For INT_MAX
#include "request.h"
#include "io_helper.h"

char default_root[] = ".";

// Default values
#define DEFAULT_THREADS 1
#define DEFAULT_BUFFER_SIZE 1
#define MAX_THREADS 100
#define MAX_BUFFER_SIZE 100

// Scheduling algorithms
#define FIFO 0
#define SFF 1

// Structure to represent a request in the buffer
typedef struct
{
  int fd;                  // Client file descriptor
  struct sockaddr_in addr; // Client address
  int filesize;            // For SFF scheduling
  int valid;               // Flag to mark if this request is valid or has been removed
} request_t;

// Global variables for thread management
int num_threads = DEFAULT_THREADS;
int buffer_size = DEFAULT_BUFFER_SIZE;
int scheduling_alg = FIFO;
int buffer_count = 0;
int buffer_head = 0;
int buffer_tail = 0;
request_t *request_buffer;

// Synchronization variables
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;

// Function to estimate file size for SFF scheduling
int estimate_filesize(int fd)
{
  // Use request_get_filesize from request.c to properly estimate file size
  return request_get_filesize(fd);
}

// Add a request to the buffer
void add_request(int fd, struct sockaddr_in addr)
{
  pthread_mutex_lock(&buffer_mutex);

  // Wait if buffer is full
  while (buffer_count == buffer_size)
  {
    pthread_cond_wait(&buffer_not_full, &buffer_mutex);
  }

  request_t request;
  request.fd = fd;
  request.addr = addr;
  request.valid = 1;  // Mark as valid

  // Always calculate filesize regardless of algorithm
  // This avoids having to recalculate if we switch algorithms
  request.filesize = estimate_filesize(fd);
  printf("Added request with fd=%d, filesize=%d\n", fd, request.filesize);

  request_buffer[buffer_tail] = request;
  buffer_tail = (buffer_tail + 1) % buffer_size;
  buffer_count++;

  pthread_cond_signal(&buffer_not_empty);
  pthread_mutex_unlock(&buffer_mutex);
}

// FIFO: Get the request at the head of the buffer
request_t get_fifo_request()
{
  // Find the first valid request starting from the head
  int curr = buffer_head;
  while (!request_buffer[curr].valid && curr != buffer_tail) {
    curr = (curr + 1) % buffer_size;
  }
  
  // If we found a valid request, mark it as invalid and return it
  if (request_buffer[curr].valid) {
    request_t request = request_buffer[curr];
    request_buffer[curr].valid = 0;  // Mark as invalid
    
    // If this was at the head, move head pointer past all invalid requests
    if (curr == buffer_head) {
      while (buffer_head != buffer_tail && !request_buffer[buffer_head].valid) {
        buffer_head = (buffer_head + 1) % buffer_size;
      }
    }
    
    return request;
  }
  
  // This should never happen if buffer_count > 0
  fprintf(stderr, "Error: No valid requests found in buffer\n");
  exit(1);
}

// SFF: Get the request with the smallest filesize
request_t get_sff_request()
{
  // Find the smallest file among valid requests
  int smallest_idx = -1;
  int smallest_size = INT_MAX;  // Use INT_MAX from limits.h
  printf("Starting SFF search with %d items in buffer\n", buffer_count);

  // Search through the entire buffer for the smallest valid request
  for (int i = 0; i < buffer_size; i++)
  {
    int idx = (buffer_head + i) % buffer_size;
    
    // Only consider valid requests
    if (request_buffer[idx].valid) {
      printf("Checking request at index %d with size %d\n", idx, request_buffer[idx].filesize);
      
      if (request_buffer[idx].filesize < smallest_size)
      {
        smallest_idx = idx;
        smallest_size = request_buffer[idx].filesize;
        printf("Found new smallest request: size %d at index %d\n", smallest_size, smallest_idx);
      }
    }
  }

  if (smallest_idx == -1) {
    // This should never happen if buffer_count > 0
    fprintf(stderr, "Error: No valid requests found in buffer\n");
    exit(1);
  }

  // Save the request to return
  request_t request = request_buffer[smallest_idx];
  printf("Selected request with size %d from index %d\n", request.filesize, smallest_idx);

  // Mark this request as invalid
  request_buffer[smallest_idx].valid = 0;
  
  // If this was at the head, update head pointer
  if (smallest_idx == buffer_head) {
    while (buffer_head != buffer_tail && !request_buffer[buffer_head].valid) {
      buffer_head = (buffer_head + 1) % buffer_size;
    }
    printf("Updated head to %d\n", buffer_head);
  }

  return request;
}

// Get the next request from the buffer based on scheduling algorithm
request_t get_request()
{
  pthread_mutex_lock(&buffer_mutex);

  // Wait if buffer is empty
  while (buffer_count == 0)
  {
    pthread_cond_wait(&buffer_not_empty, &buffer_mutex);
  }

  request_t request;

  // Choose scheduling algorithm
  printf("Using scheduler: %s\n", scheduling_alg == FIFO ? "FIFO" : "SFF");
  if (scheduling_alg == FIFO)
  {
    request = get_fifo_request();
  }
  else
  { // SFF
    request = get_sff_request();
  }

  buffer_count--;

  pthread_cond_signal(&buffer_not_full);
  pthread_mutex_unlock(&buffer_mutex);

  return request;
}

// Worker thread function
void *worker_thread(void *arg)
{
  while (1)
  {
    request_t request = get_request();
    printf("Thread %lu processing request with fd=%d, size=%d\n", 
           (unsigned long)pthread_self(), request.fd, request.filesize);
    request_handle(request.fd);
    close_or_die(request.fd);
    printf("Thread %lu completed request\n", (unsigned long)pthread_self());
  }
  return NULL;
}

//
// ./wserver [-d <basedir>] [-p <portnum>] [-t threads] [-b buffers] [-s schedalg]
//
int main(int argc, char *argv[])
{
  int c;
  char *root_dir = default_root;
  int port = 10000;
  char *sched_alg = "FIFO";

  while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
    switch (c)
    {
    case 'd':
      root_dir = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      num_threads = atoi(optarg);
      if (num_threads <= 0)
      {
        fprintf(stderr, "Number of threads must be positive\n");
        exit(1);
      }
      if (num_threads > MAX_THREADS)
      {
        fprintf(stderr, "Too many threads, maximum is %d\n", MAX_THREADS);
        exit(1);
      }
      break;
    case 'b':
      buffer_size = atoi(optarg);
      if (buffer_size <= 0)
      {
        fprintf(stderr, "Buffer size must be positive\n");
        exit(1);
      }
      if (buffer_size > MAX_BUFFER_SIZE)
      {
        fprintf(stderr, "Too many buffers, maximum is %d\n", MAX_BUFFER_SIZE);
        exit(1);
      }
      break;
    case 's':
      sched_alg = optarg;
      if (strcasecmp(sched_alg, "FIFO") == 0)
      {
        scheduling_alg = FIFO;
        printf("Setting scheduler to FIFO (0)\n");
      }
      else if (strcasecmp(sched_alg, "SFF") == 0)
      {
        scheduling_alg = SFF;
        printf("Setting scheduler to SFF (1)\n");
      }
      else
      {
        fprintf(stderr, "Invalid scheduling algorithm. Must be FIFO or SFF\n");
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]\n");
      exit(1);
    }

  // Allocate request buffer
  request_buffer = (request_t *)malloc(buffer_size * sizeof(request_t));
  if (!request_buffer)
  {
    fprintf(stderr, "Failed to allocate memory for request buffer\n");
    exit(1);
  }

  // Initialize buffer
  for (int i = 0; i < buffer_size; i++) {
    request_buffer[i].valid = 0;  // Mark all slots as invalid initially
  }

  // run out of this directory
  chdir_or_die(root_dir);

  // Create worker threads
  pthread_t threads[MAX_THREADS];
  for (int i = 0; i < num_threads; i++)
  {
    if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0)
    {
      fprintf(stderr, "Failed to create thread %d\n", i);
      exit(1);
    }
  }

  printf("Server starting on port %d with %d threads, %d buffers, and %s scheduling\n",
         port, num_threads, buffer_size, sched_alg);

  // now, get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1)
  {
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr, (socklen_t *)&client_len);

    // Instead of handling the request directly, add it to the buffer
    add_request(conn_fd, client_addr);
  }

  // Clean up (never reached in this example)
  free(request_buffer);
  return 0;
}