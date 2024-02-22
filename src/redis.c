#include "../include/redis.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

Redis *init_redis(void) {
  Redis *r = malloc(sizeof(Redis));
  r->items = malloc(sizeof(RedisItem*) * REDIS_INIT_CAP);
  r->count = 0;
  r->capacity = REDIS_INIT_CAP;
  return r;
}

size_t hash(Redis *r, char *str) {
  size_t hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c;

  return hash % r->capacity;
}

int redis_store(Redis *r, KeyValueData *kv) {
  size_t pos = hash(r, kv->key);

  RedisItem *data = malloc(sizeof(RedisItem));
  data->key = kv->key;
  data->value = kv->value;

  if (r->items[pos]) {
    fprintf(stderr,
            "[ERROR]:%s:%d => Store Error: Couldn't store data: collision "
            "detected\n",
            __FILE_NAME__, __LINE__);
    return -1;
  }

  r->items[pos] = data;

  return 0;
}

char *redis_get_error(void) { return "error"; }
