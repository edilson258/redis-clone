#include "../include/network.h"
#include "../include/parse.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Note:
 *  The function bellow has 2 BUGS
 *  and 1 OVERHEAD
 */
RequestData *request_extract_data(int remote_fd) {
  char *buf = NULL;
  size_t buf_size = 0;

  while (1) {
    char chunk[CHUNK_SIZE];

    /*
     * @BUG: The line bellow will wait forever if
     * remote_fd has 0 bytes to be received,
     * unless the remote cancels request
     */
    int readbytes = recv(remote_fd, chunk, CHUNK_SIZE, 0);

    if (readbytes == -1) {
      fprintf(stderr, "[ERROR]: Couldn't Receive: %s\n", strerror(errno));
      break;
    }

    /*
     * @OVERHEAD: Too many calls to realloc
     */
    buf = realloc(buf, buf_size + readbytes);

    if (buf == NULL) {
      fprintf(stderr, "[ERROR]: Couldn't allocate Mem. for received data: %s\n",
              strerror(errno));
      break;
    }

    memcpy(buf + buf_size, chunk, readbytes);
    buf_size += readbytes;

    /*
     * @BUG: This may result in a infinity loop
     * if the last copied chunk size be grather or equal
     * to CHUNK_SIZE
     */
    if (readbytes < CHUNK_SIZE) {
      break;
    }
  }

  RequestData *rdata = malloc(sizeof(RequestData));
  rdata->text = buf;
  rdata->length = buf_size;

  return rdata;
}

char *slice(RequestData *rdata, size_t count) {
  if (rdata->length < count) {
    fprintf(stderr,
            "[ERROR]: Couldn't get slice: count is grather than text length\n");
    return NULL;
  }
  char *out = malloc(sizeof(char) * count);
  memcpy(out, rdata->text, count);
  return out;
}

void respond_with_text(int remote_fd, char *statcode, char *reason) {
  char *body_templ = "%s";
  size_t body_len = 1 + snprintf(NULL, 0, body_templ, reason);
  char body[body_len];
  snprintf(body, body_len, body_templ, reason);
  char *header_templ = "HTTP/1.1 %s\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %zu\r\n"
                       "\r\n";
  size_t header_len = 1 + snprintf(NULL, 0, header_templ, statcode, body_len);
  char header[header_len];
  snprintf(header, header_len, header_templ, statcode, body_len);
  size_t res_len = header_len + body_len;
  char res[res_len];
  snprintf(res, res_len, "%s%s", header, body);
  send(remote_fd, res, res_len, 0);
}

/*
 * @TODO: optmize code repitition
 */
RequestMethod request_extract_method(RequestData *rdata, int remote_fd) {
  if (strcmp("GET", slice(rdata, 3)) == 0) {
    return GET;
  }

  if (strcmp("POST", slice(rdata, 4)) == 0) {
    return POST;
  }

  fprintf(stderr, "[ERROR]: Unsupported request method\n");
  respond_with_text(remote_fd, "405", "ERROR: Unsupported request method");
  close(remote_fd);
  pthread_exit(NULL);
}

Request *request_build(int remote_fd) {
  RequestData *rdata = request_extract_data(remote_fd);
  Request *req = malloc(sizeof(Request));
  req->remote_fd = remote_fd;
  req->method = request_extract_method(rdata, remote_fd);
  req->data = rdata;

  return req;
}

size_t extract_content_len(char *text) {
  regex_t regex;
  char *pattern = "Content-Length: [0-9]{1,}";

  int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
  if (ret) {
    fprintf(stderr, "[ERROR]: Error compiling regex\n");
    return -1;
  }

  const size_t nmatch = 1;
  regmatch_t pmatch[nmatch];

  if (regexec(&regex, text, nmatch, pmatch, 0) != 0) {
    fprintf(stderr, "[ERROR]: Error compiling regex\n");
    return -1;
  }

  size_t match_len = pmatch[0].rm_eo - pmatch[0].rm_so;
  char ss[match_len];
  memcpy(ss, text + pmatch[0].rm_so, match_len);
  ss[match_len] = '\0';
  size_t content_len = atoi(ss + 16); // +16 to remove `Content-Length: `
  return content_len;
}

void handle_request_post(Redis *r, Request *req) {
  size_t offset = req->data->length - extract_content_len(req->data->text);
  KeyValueData *kv = malloc(sizeof(KeyValueData));

  if (parse_post_content(req->data->text + offset, kv)) {
    fprintf(stderr,
            "[ERROR]:%s:%d => Couldn't parse_post_content for remote_fd: %d\n",
            __FILE_NAME__, __LINE__, req->remote_fd);
    respond_with_text(req->remote_fd, "400",
                      "ERROR: Invalid post data. expect: {\"key\":\"value\"}");
    close(req->remote_fd);
  }

  if (redis_store(r, kv)) {
    fprintf(stderr,
            "[ERROR]:%s:%d => Couldn't store user data for remote_fd: %d\n",
            __FILE_NAME__, __LINE__, req->remote_fd);
    respond_with_text(req->remote_fd, "500", redis_get_error());
    close(req->remote_fd);
  }

  respond_with_text(req->remote_fd, "201", "Data stored successfully");
  close(req->remote_fd);

  free(req->data->text);
  free(req->data);
}

void handle_request_get(Redis *r, Request *req) {
  (void) r;
  respond_with_text(req->remote_fd, "404",
                    "ERROR: Can't handle GET requests for now");
  printf("Can't handle GET requests for now\n");
}

void handle_request(Redis *r, int remote_fd) {
  Request *req = request_build(remote_fd);

  /*
   * @TODO: Add flag in build step to ensure convering all enum variations
   */
  switch (req->method) {
  case GET:
    handle_request_get(r, req);
    break;
  case POST:
    handle_request_post(r, req);
    break;
  }
}

void *handle_client(void *_thread_input) {
  ThreadInput *thread_id = (ThreadInput *)_thread_input;

  handle_request(thread_id->r, thread_id->remote_fd);

  // Note: remote_fd is closed

  return NULL;
}

/*
 * Function bellow will:
 * - create server socket
 * - bind server socket to an address
 * - listen for incoming connections
 *
 * @RETURN:
 * - will return server socket's file descriptor
 */
int create_server_socket(void) {
  printf("[INFO]: Creating Server Socket...\n");
  int sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sfd == -1) {
    fprintf(stderr, "[ERROR]: Couldn't create server: %s\n", strerror(errno));
    return -1;
  }

  if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
    fprintf(stderr, "[ERROR]: Couldn't set to reuse Addr: %s\n",
            strerror(errno));
  }

  struct sockaddr_in saddr = {.sin_family = AF_INET,
                              .sin_port = htons(PORT),
                              .sin_addr.s_addr = inet_addr(HOST)};

  socklen_t saddr_len = sizeof(saddr);

  printf("[INFO]: Binding Server Socket...\n");
  if (bind(sfd, (struct sockaddr *)&saddr, saddr_len) == -1) {
    fprintf(stderr, "[ERROR]: Couldn't Bind: %s\n", strerror(errno));
    return -1;
  }

  printf("[INFO]: Listening for connections...\n");
  if (listen(sfd, MAX_CONN) == -1) {
    fprintf(stderr, "[ERROR]: Couldn't Listen: %s\n", strerror(errno));
    return -1;
  }

  return sfd;
}

int accept_conn(int sfd) { return accept(sfd, NULL, 0); }
