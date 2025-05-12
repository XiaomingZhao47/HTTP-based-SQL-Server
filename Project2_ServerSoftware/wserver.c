#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "request.h"
#include "io_helper.h"

char default_root[] = ".";

// default values
#define DEFAULT_THREADS 1
#define DEFAULT_BUFFER_SIZE 1
#define MAX_THREADS 100
#define MAX_BUFFER_SIZE 100

// scheduling algorithms
#define FIFO 0
#define SFF 1

// request in the buffer
typedef struct
{
  int fd;                  // client file descriptor
  struct sockaddr_in addr; // client address
  int filesize;            // SFF scheduling
} request_t;

// global variables
int num_threads = DEFAULT_THREADS;
int buffer_size = DEFAULT_BUFFER_SIZE;
int scheduling_alg = FIFO;
int buffer_count = 0;
int buffer_head = 0;
int buffer_tail = 0;
request_t *request_buffer;

// synchronization variables
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;

/**
 * Estimates the file size for Shortest File First (SFF) scheduling
 * parses the HTTP request to determine the requested URI and attempts to estimate
 * the size of the resource. For CGI scripts with a 'spin' parameter, it uses the parameter
 * value as a proxy for file size
 *
 * @param fd the client file descriptor to read the HTTP request from
 * @return estimated size of the requested resource in bytes
 */
int estimate_filesize(int fd)
{
  // read the HTTP request to estimate file size
  char buffer[8192];
  int n = recv(fd, buffer, sizeof(buffer) - 1, MSG_PEEK);
  if (n <= 0)
    return 0;

  buffer[n] = '\0';

  char *uri_start = strstr(buffer, "GET ");
  if (!uri_start)
    return n;

  uri_start += 4; // skip "GET "
  char *uri_end = strchr(uri_start, ' ');
  if (!uri_end)
    return n;

  int uri_len = uri_end - uri_start;
  char uri[1024];
  if (uri_len >= sizeof(uri))
    uri_len = sizeof(uri) - 1;
  strncpy(uri, uri_start, uri_len);
  uri[uri_len] = '\0';

  // if it's a CGI script with spin parameter, use that as a proxy for file size
  if (strstr(uri, "spin.cgi?"))
  {
    char *param = strchr(uri, '?');
    if (param)
    {
      int spin_time = atoi(param + 1);
      return spin_time * 1000;
    }
  }

  return n;
}

/**
 * adds a client request to the request buffer
 * if the buffer is full, the function will block until space becomes available
 * the file size of the requested resource is estimated for potential SFF scheduling
 *
 * @param fd the client socket file descriptor
 * @param addr the client's address information
 */
void add_request(int fd, struct sockaddr_in addr)
{
  pthread_mutex_lock(&buffer_mutex);

  while (buffer_count == buffer_size)
  {
    pthread_cond_wait(&buffer_not_full, &buffer_mutex);
  }

  request_t request;
  request.fd = fd;
  request.addr = addr;
  request.filesize = estimate_filesize(fd);

  // add new request to the buffer
  request_buffer[buffer_tail] = request;
  buffer_tail = (buffer_tail + 1) % buffer_size;
  buffer_count++;

  // signal that buffer is not empty
  pthread_cond_broadcast(&buffer_not_empty);
  pthread_mutex_unlock(&buffer_mutex);
}

/**
 * retrieves request from the buffer using First-In-First-Out (FIFO) scheduling policy
 * returns the request at the head of the buffer (oldest request)
 *
 * @return the request at the head of the buffer
 */
request_t get_fifo_request()
{
  request_t request = request_buffer[buffer_head];
  buffer_head = (buffer_head + 1) % buffer_size;
  return request;
}

/**
 * retrieves request from the buffer using Shortest File First (SFF) scheduling policy
 * scans the entire buffer to find the request with the smallest estimated file size
 * removes the selected request from the buffer and adjusts the buffer
 *
 * @return the request with the smallest estimated file size
 */
request_t get_sff_request()
{
  // find the smallest file
  int smallest_idx = buffer_head;
  int smallest_size = request_buffer[smallest_idx].filesize;

  for (int i = 0; i < buffer_count; i++)
  {
    int idx = (buffer_head + i) % buffer_size;
    if (request_buffer[idx].filesize < smallest_size)
    {
      smallest_idx = idx;
      smallest_size = request_buffer[idx].filesize;
    }
  }

  // save the request to return
  request_t request = request_buffer[smallest_idx];

  if (smallest_idx != buffer_head)
  {
    // swap with head
    request_buffer[smallest_idx] = request_buffer[buffer_head];
  }

  // advance head
  buffer_head = (buffer_head + 1) % buffer_size;

  return request;
}

/**
 * gets the next request from the buffer based on the scheduling algorithm
 * if the buffer is empty, the function will block until a request is available
 * delegates to either get_fifo_request() or get_sff_request() based on the scheduling_alg setting
 *
 * @return the next request to be processed according to the current scheduling policy
 */
request_t get_request()
{
  pthread_mutex_lock(&buffer_mutex);

  // wait if buffer is empty
  while (buffer_count == 0)
  {
    pthread_cond_wait(&buffer_not_empty, &buffer_mutex);
  }

  request_t request;

  // choose scheduling algorithm
  if (scheduling_alg == FIFO)
  {
    request = get_fifo_request();
  }
  else
  {
    request = get_sff_request();
  }

  buffer_count--;

  pthread_cond_signal(&buffer_not_full);
  pthread_mutex_unlock(&buffer_mutex);

  return request;
}

/**
 * worker thread function that continuously processes requests from the request buffer
 * each thread calls get_request() to obtain the next request to handle,
 * processes the request with request_handle(), and then closes the client
 *
 * @param arg thread argument (unused)
 * @return NULL (thread runs until program termination)
 */
void *worker_thread(void *arg)
{
  while (1)
  {
    request_t request = get_request();
    request_handle(request.fd);
    close_or_die(request.fd);
  }
  return NULL;
}

/**
 * main function that initializes and starts the web server
 * command-line arguments, sets up the request buffer, creates worker threads
 * and enters the main accept loop to receive client connections
 *
 * command-line options
 * -d <basedir>  : Set the root directory for the server
 * -p <portnum>  : Set the port number to listen on
 * -t <threads>  : Set the number of worker threads
 * -b <buffers>  : Set the size of the request buffer
 * -s <schedalg> : Set the scheduling algorithm (FIFO or SFF)
 *
 * @param argc number of command-line arguments
 * @param argv array of command-line argument strings
 * @return 0 on success
 */
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
      }
      else if (strcasecmp(sched_alg, "SFF") == 0)
      {
        scheduling_alg = SFF;
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

  // allocate request buffer
  request_buffer = (request_t *)malloc(buffer_size * sizeof(request_t));
  if (!request_buffer)
  {
    fprintf(stderr, "Failed to allocate memory for request buffer\n");
    exit(1);
  }

  // run out of this directory
  chdir_or_die(root_dir);

  // create worker threads
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

  // get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1)
  {
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr, (socklen_t *)&client_len);

    // add it to the buffer
    add_request(conn_fd, client_addr);
  }

  // clean up
  free(request_buffer);
  return 0;
}