#include <stddef.h>

#ifndef REDIS_H
#define REDIS_H

#define REDIS_INIT_CAP 1024

typedef struct {
  char *key;
  char *value;
} RedisItem;

typedef struct {
  size_t count;
  size_t capacity;
  RedisItem **items;
} Redis;

typedef struct {
  char *key;
  char *value;
} KeyValueData;

// TODO: separate from hahstable code
typedef struct {
  int remote_fd;
  Redis *r;
} ThreadInput;

Redis *init_redis(void);
int redis_store(Redis *, KeyValueData *);
char *redis_get_error(void);

#endif
