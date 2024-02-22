#include <stddef.h>
#include "redis.h"

#ifndef NETWORK_H
#define NETWORK_H

#define HOST "127.0.0.1"
#define PORT 8080
#define MAX_CONN 10
#define CHUNK_SIZE 256

typedef struct {
  char *text;
  size_t length;
} RequestData;

typedef enum {
  GET = 0,
  POST,
} RequestMethod;

typedef struct {
  int remote_fd;
  RequestData *data;
  RequestMethod method;
} Request;

RequestData *request_extract_data(int remote_fd);
// TODO: make it more generic function
char *slice(RequestData *rdata, size_t count);
void respond_with_text(int remote_fd, char *statcode, char *reason);
RequestMethod request_extract_method(RequestData *rdata, int remote_fd);
Request *request_build(int remote_fd);
size_t extract_content_len(char *text);
void handle_request_post(Redis *r, Request *req);
void handle_request(Redis *r, int remote_fd);
void *handle_client(void *_thread_input);
int create_server_socket(void);
int accept_conn(int sfd);

#endif
