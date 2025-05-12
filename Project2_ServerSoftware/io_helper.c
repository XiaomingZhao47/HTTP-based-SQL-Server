#include "io_helper.h"

// mutex for thread-safety
pthread_mutex_t io_helper_mutex;

/**
 * initializes the I/O helper module for thread safety
 * creates and initializes the mutex used for synchronizing access to
 * thread-unsafe system calls
 */
void io_helper_init(void)
{
    pthread_mutex_init(&io_helper_mutex, NULL);
}

/**
 * cleans up resources used by the I/O helper module
 * destroys the mutex used for thread synchronization
 */
void io_helper_cleanup(void)
{
    pthread_mutex_destroy(&io_helper_mutex);
}

/**
 * reads a line of text from a file descriptor
 * reads characters one at a time until either a newline character is encountered
 * the maximum length is reached, or an EOF or error occurs
 * the resulting string is null-terminated
 *
 * @param fd file descriptor to read from
 * @param buf buffer to store the read line
 * @param maxlen maximum number of bytes to read (including null terminator)
 * @return number of bytes read (excluding null terminator) or -1 on error
 */
ssize_t readline(int fd, void *buf, size_t maxlen)
{
    char c;
    char *bufp = buf;
    int n;
    for (n = 0; n < maxlen - 1; n++)
    { // leave room at end for '\0'
        int rc;
        if ((rc = read_or_die(fd, &c, 1)) == 1)
        {
            *bufp++ = c;
            if (c == '\n')
                break;
        }
        else if (rc == 0)
        {
            if (n == 1)
                return 0; /* EOF, no data read */
            else
                break; /* EOF, some data was read */
        }
        else
            return -1; /* error */
    }
    *bufp = '\0';
    return n;
}

/**
 * opens client socket connection to a specified server
 * creates socket and establishes a connection to the server at the given hostname and port
 * uses thread-safe access to DNS resolution functions
 *
 * @param hostname hostname or IP address of the server to connect to
 * @param port port number to connect to on the server
 * @return connected socket file descriptor or negative value on error
 */
int open_client_fd(char *hostname, int port)
{
    int client_fd;
    struct hostent *hp;
    struct sockaddr_in server_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    // protect access to gethostbyname with mutex
    pthread_mutex_lock(&io_helper_mutex);
    if ((hp = gethostbyname(hostname)) == NULL)
    {
        pthread_mutex_unlock(&io_helper_mutex);
        close(client_fd);
        return -2; // check h_errno for cause of error
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr,
          (char *)&server_addr.sin_addr.s_addr, hp->h_length);
    server_addr.sin_port = htons(port);

    // release mutex after using gethostbyname data
    pthread_mutex_unlock(&io_helper_mutex);

    if (connect(client_fd, (sockaddr_t *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(client_fd);
        return -1;
    }
    return client_fd;
}

/**
 * creates a socket to listen for incoming client connections
 * creates and configures a socket with the specified port, sets socket options to allow
 * address reuse, binds to the specified port on all network interfaces, and
 * prepares the socket to accept connections
 *
 * @param port port number to listen on
 * @return listening socket file descriptor or -1 on error
 */
int open_listen_fd(int port)
{
    int listen_fd;
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket() failed\n");
        return -1;
    }

    // eliminates "Address already in use" error from bind
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
    {
        fprintf(stderr, "setsockopt() failed\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);
    if (bind(listen_fd, (sockaddr_t *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "bind() failed\n");
        return -1;
    }

    if (listen(listen_fd, 1024) < 0)
    {
        fprintf(stderr, "listen() failed\n");
        return -1;
    }
    return listen_fd;
}

/**
 * gets the size of a file specified by filename
 * uses the stat system call to retrieve file information and returns the file size
 *
 * @param filename path to the file whose size is to be determined
 * @return size of the file in bytes, or 0 if the file cannot be accessed
 */
int get_file_size(char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return st.st_size;
    }
    return 0; // return 0 if can't determine size
}

/**
 * estimates the size of an HTTP request by examining the request headers
 * peeks at the request data without consuming it, extracts the URI, and
 * either uses special handling for CGI scripts with 'spin' parameters or
 * determines the actual file size for static files
 *
 * @param fd client socket file descriptor to examine
 * @return estimated size in bytes of the requested resource
 */
int estimate_request_size(int fd)
{
    char buf[8192];
    int n = recv(fd, buf, sizeof(buf) - 1, MSG_PEEK);
    if (n <= 0)
        return 0;

    buf[n] = '\0';

    // look for request URI
    char *uri_start = strstr(buf, "GET ");
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
            return spin_time * 1000; // scale up
        }
    }

    // for static files, extract the filename and get its actual size
    char filename[1024];
    sprintf(filename, ".%s", uri);
    if (uri[strlen(uri) - 1] == '/')
    {
        strcat(filename, "index.html");
    }

    return get_file_size(filename);
}