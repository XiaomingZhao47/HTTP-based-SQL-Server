#ifndef __REQUEST_H__
#define __REQUEST_H__

void request_handle(int fd);
int request_get_filesize(int fd);

#endif // __REQUEST_H__