
#include "server.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: server port\n");
    exit(EXIT_FAILURE);
  }
  run(argv[1]);
}