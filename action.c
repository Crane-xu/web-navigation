#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "action.h"

void action(int client_socket, const char *msg) {
  char other[BUFFER_SIZE];

  snprintf(other, BUFFER_SIZE,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %ld\r\n\r\n"
           "%s",
           strlen(msg), msg);
  send(client_socket, other, strlen(other), 0);
}
