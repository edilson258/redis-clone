#include "../include/network.h"
#include "../include/redis.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  int sfd = create_server_socket();

  if (sfd == -1) {
    return -1;
  }

  Redis *r = init_redis();

  // Main loop
  while (1) {
    int remote_fd = accept_conn(sfd);
    if (remote_fd == -1) {
      fprintf(stderr, "[ERROR]: Couldn't Accept: %s\n", strerror(errno));
      continue;
    }

    ThreadInput *ti = malloc(sizeof(ThreadInput));
    ti->remote_fd = remote_fd;
    ti->r = r;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client, ti);
    pthread_join(thread_id, NULL);
  };

  close(sfd);

  return 0;
}
