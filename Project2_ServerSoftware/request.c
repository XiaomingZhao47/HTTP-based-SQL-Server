#include "io_helper.h"
#include "request.h"
#include <pthread.h>

//
// Some of this code stolen from Bryant/O'Hallaron
// Hopefully this is not a problem ... :)
//

#define MAXBUF (8192)

// Mutex for synchronizing printf and other shared operations
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;

void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXBUF], body[MAXBUF];

  // Create the body of error message first (have to know its length for header)
  sprintf(body, ""
                "<!doctype html>\r\n"
                "<head>\r\n"
                "  <title>OSTEP WebServer Error</title>\r\n"
                "</head>\r\n"
                "<body>\r\n"
                "  <h2>%s: %s</h2>\r\n"
                "  <p>%s: %s</p>\r\n"
                "</body>\r\n"
                "</html>\r\n",
          errnum, shortmsg, longmsg, cause);

  // Write out the header information for this response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  write_or_die(fd, buf, strlen(buf));

  sprintf(buf, "Content-Type: text/html\r\n");
  write_or_die(fd, buf, strlen(buf));

  sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
  write_or_die(fd, buf, strlen(buf));

  // Write out the body last
  write_or_die(fd, body, strlen(body));
}

//
// Reads and discards everything up to an empty text line
//
void request_read_headers(int fd)
{
  char buf[MAXBUF];

  readline_or_die(fd, buf, MAXBUF);
  while (strcmp(buf, "\r\n"))
  {
    readline_or_die(fd, buf, MAXBUF);
  }
  return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int request_parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi"))
  {
    // static
    strcpy(cgiargs, "");
    sprintf(filename, ".%s", uri);
    if (uri[strlen(uri) - 1] == '/')
    {
      strcat(filename, "index.html");
    }
    return 1;
  }
  else
  {

    // dynamic
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
    {
      strcpy(cgiargs, "");
    }
    sprintf(filename, ".%s", uri);
    return 0;
  }
}

//
// Fills in the filetype given the filename
//
void request_get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

// Thread-safe version of request_serve_dynamic
void request_serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXBUF], *argv[] = {NULL};

  // The server does only a little bit of the header.
  // The CGI script has to finish writing out the header.
  sprintf(buf, ""
               "HTTP/1.0 200 OK\r\n"
               "Server: OSTEP WebServer\r\n");

  write_or_die(fd, buf, strlen(buf));

  // Use mutex to protect fork operation
  pthread_mutex_lock(&request_mutex);
  pid_t pid = fork_or_die();
  pthread_mutex_unlock(&request_mutex);

  if (pid == 0)
  {                                            // child
    setenv_or_die("QUERY_STRING", cgiargs, 1); // args to cgi go here
    dup2_or_die(fd, STDOUT_FILENO);            // make cgi writes go to socket (not screen)
    extern char **environ;                     // defined by libc
    execve_or_die(filename, argv, environ);
  }
  else
  {
    wait_or_die(NULL);
  }
}

void request_serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXBUF], buf[MAXBUF];

  request_get_filetype(filename, filetype);
  srcfd = open_or_die(filename, O_RDONLY, 0);

  // Rather than call read() to read the file into memory,
  // which would require that we allocate a buffer, we memory-map the file
  srcp = mmap_or_die(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  close_or_die(srcfd);

  // put together response
  sprintf(buf, ""
               "HTTP/1.0 200 OK\r\n"
               "Server: OSTEP WebServer\r\n"
               "Content-Length: %d\r\n"
               "Content-Type: %s\r\n\r\n",
          filesize, filetype);

  write_or_die(fd, buf, strlen(buf));

  //  Writes out to the client socket the memory-mapped file
  write_or_die(fd, srcp, filesize);
  munmap_or_die(srcp, filesize);
}

// New function to estimate file size for SFF scheduling
int request_get_filesize(int fd)
{
  char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
  char filename[MAXBUF], cgiargs[MAXBUF];
  struct stat sbuf;

  // Read the request line
  int n = recv(fd, buf, MAXBUF - 1, MSG_PEEK);
  if (n <= 0)
    return 0;

  // Find the end of the first line
  char *end = strstr(buf, "\r\n");
  if (!end)
    return n; // If can't parse, return request size

  // Extract method, URI, and version
  buf[end - buf] = '\0';
  sscanf(buf, "%s %s %s", method, uri, version);

  // Parse URI to get filename
  int is_static = request_parse_uri(uri, filename, cgiargs);

  // If it's a CGI script with a parameter, use that as the size estimate
  if (!is_static && strstr(filename, "spin.cgi") && strlen(cgiargs) > 0)
  {
    return atoi(cgiargs) * 1000; // Scale up for better differentiation
  }

  // For static files, try to get the actual file size
  if (stat(filename, &sbuf) >= 0)
  {
    return sbuf.st_size;
  }

  // Default: return the request size
  return n;
}

// Handle a request - thread-safe version
void request_handle(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
  char filename[MAXBUF], cgiargs[MAXBUF];

  readline_or_die(fd, buf, MAXBUF);
  sscanf(buf, "%s %s %s", method, uri, version);

  // Using mutex to protect printf
  pthread_mutex_lock(&request_mutex);
  printf("method:%s uri:%s version:%s\n", method, uri, version);
  pthread_mutex_unlock(&request_mutex);

  if (strcasecmp(method, "GET"))
  {
    request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
    return;
  }
  request_read_headers(fd);

  is_static = request_parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    request_error(fd, filename, "404", "Not found", "server could not find this file");
    return;
  }

  if (is_static)
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      request_error(fd, filename, "403", "Forbidden", "server could not read this file");
      return;
    }
    request_serve_static(fd, filename, sbuf.st_size);
  }
  else
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      request_error(fd, filename, "403", "Forbidden", "server could not run this CGI program");
      return;
    }
    request_serve_dynamic(fd, filename, cgiargs);
  }
}