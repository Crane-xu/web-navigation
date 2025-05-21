#include "constants.h"
#include "mime_types.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

void send_response(int client_fd, const char *filepath);