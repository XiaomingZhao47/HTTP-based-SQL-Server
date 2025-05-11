#include "io_helper.h"

// Mutex for thread-safety
pthread_mutex_t io_helper_mutex;

// Initialize io_helper for thread safety
void io_helper_init(void)
{
    pthread_mutex_init(&io_helper_mutex, NULL);
}

// Clean up io_helper resources
void io_helper_cleanup(void)
{
    pthread_mutex_destroy(&io_helper_mutex);
}

// readline implementation
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

// Thread-safe version of open_client_fd
int open_client_fd(char *hostname, int port)
{
    int client_fd;
    struct hostent *hp;
    struct sockaddr_in server_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    // Protect access to gethostbyname with mutex
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

    // Release mutex after using gethostbyname data
    pthread_mutex_unlock(&io_helper_mutex);

    // Establish a connection with the server
    if (connect(client_fd, (sockaddr_t *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(client_fd);
        return -1;
    }
    return client_fd;
}

int open_listen_fd(int port)
{
    // Create a socket descriptor
    int listen_fd;
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket() failed\n");
        return -1;
    }

    // Eliminates "Address already in use" error from bind
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
    {
        fprintf(stderr, "setsockopt() failed\n");
        return -1;
    }

    // Listen_fd will be an endpoint for all requests to port on any IP address for this host
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

    // Make it a listening socket ready to accept connection requests
    if (listen(listen_fd, 1024) < 0)
    {
        fprintf(stderr, "listen() failed\n");
        return -1;
    }
    return listen_fd;
}

// Function to get file size - used for SFF scheduling
int get_file_size(char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return st.st_size;
    }
    return 0; // Return 0 if can't determine size
}

// Function to estimate request size based on peeking at the request
int estimate_request_size(int fd)
{
    char buf[8192];
    int n = recv(fd, buf, sizeof(buf) - 1, MSG_PEEK);
    if (n <= 0)
        return 0;

    buf[n] = '\0';

    // Look for request URI
    char *uri_start = strstr(buf, "GET ");
    if (!uri_start)
        return n; // Default to request size if can't parse

    uri_start += 4; // Skip "GET "
    char *uri_end = strchr(uri_start, ' ');
    if (!uri_end)
        return n;

    int uri_len = uri_end - uri_start;
    char uri[1024];
    if (uri_len >= sizeof(uri))
        uri_len = sizeof(uri) - 1;
    strncpy(uri, uri_start, uri_len);
    uri[uri_len] = '\0';

    // If it's a CGI script with spin parameter, use that as a proxy for file size
    if (strstr(uri, "spin.cgi?"))
    {
        char *param = strchr(uri, '?');
        if (param)
        {
            int spin_time = atoi(param + 1);
            return spin_time * 1000; // Scale up for better differentiation
        }
    }

    // For static files, extract the filename and get its actual size
    char filename[1024];
    sprintf(filename, ".%s", uri);
    if (uri[strlen(uri) - 1] == '/')
    {
        strcat(filename, "index.html");
    }

    return get_file_size(filename);
}