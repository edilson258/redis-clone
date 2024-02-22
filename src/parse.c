#include "../include/parse.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *cont;
} Parser;

int eat_expect(Parser *p, char *expct) {
  if (strlen(expct) > strlen(p->cont)) {
    fprintf(stderr, "[ERROR]:%s:%d => Coudn't eat_expect: content is shorter\n",
            __FILE_NAME__, __LINE__);
    return -1;
  }

  while (expct[0]) {
    if (isspace(p->cont[0])) {
      p->cont++;
      continue;
    }
    if (p->cont[0] != expct[0]) {
      fprintf(stderr, "[ERROR]:%s:%d => Coudn't eat_expect: No match\n",
              __FILE_NAME__, __LINE__);
      return -1;
    }
    p->cont++;
    expct++;
  }

  return 0;
}

int eat_upto(Parser *p, char ch, char **dest) {
  size_t contlen = strlen(p->cont);
  size_t n = 0;

  while (n < contlen && p->cont[n] != ch) {
    n++;
  }

  *dest = malloc(sizeof(char) * n + 1);
  strncpy(*dest, p->cont, n);
  p->cont += n;

  return 0;
}

int parse_post_content(char *content, KeyValueData *kv) {
  Parser p = {.cont = content};

  if (eat_expect(&p, "{\"")) {
    return -1;
  }

  if (eat_upto(&p, '"', &kv->key)) {
    return -1;
  }

  if (eat_expect(&p, "\":\"")) {
    return -1;
  }

  if (eat_upto(&p, '"', &kv->value)) {
    return -1;
  }

  if (eat_expect(&p, "\"}")) {
    return -1;
  }

  return 0;
}
