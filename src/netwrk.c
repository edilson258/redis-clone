#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/pthread_types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>

#define HOST "127.0.0.1"
#define PORT 8080
#define MAX_CONN 10
#define CHUNK_SIZE 256

typedef struct {
  char *data;
  size_t data_len;
} RequestData;

/*
 * Note:
 *  The function bellow has 2 BUGS
 *  and 1 OVERHEAD
 */
RequestData *extract_request_data(int remote_fd) {
  char *buf = NULL;
  size_t buf_size = 0;

  while (1) {
    char chunk[CHUNK_SIZE];

    /* 
    * @BUG: The line bellow will wait forever if
    * remote_fd has 0 bytes to be received, 
    * unless the remote cancel request
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
  rdata->data = buf;
  rdata->data_len = buf_size;

  return rdata;
}

void handle_client_request(int remote_fd) {
  RequestData *rdata = extract_request_data(remote_fd);
  printf("\n%s\n", rdata->data);
}

void handle_client_response(int remote_fd) {
  char *res = "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 13\r\n"
              "\r\n"
              "Hello, World!\r\n";

  send(remote_fd, res, strlen(res), 0);
}

void *handle_client(void *_remote_fd) {
  int remote_fd = *(int *)_remote_fd;

  handle_client_request(remote_fd);
  handle_client_response(remote_fd);

  close(remote_fd);
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
int create_server_socket() {
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
                              .sin_addr = inet_addr(HOST)};

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

int main(void) {
  int sfd = create_server_socket();

  if (sfd == -1) {
    return -1;
  }

  // Main loop
  while (1) {
    int remote_fd = accept(sfd, NULL, 0);
    if (remote_fd == -1) {
      fprintf(stderr, "[ERROR]: Couldn't Accept: %s\n", strerror(errno));
      continue;
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client, &remote_fd);
    pthread_join(thread_id, NULL);
  };

  close(sfd);

  return 0;
}
