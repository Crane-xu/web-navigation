#include "http_parser.h"

// 处理函数的类型定义
typedef void (*request_handler)(int, http_request *);
// 定义请求处理结构体
typedef struct {
  const char *method;
  const char *path;
  request_handler handler;
} request_mapping;

void handle_request(int client_fd, char *buffer);
void *handle_client(void *client_socket_void);