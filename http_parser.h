
#include <sys/types.h>

#define MAX_HEADER_SIZE 8192
#define MAX_BODY_SIZE 10485760 // 10MB

typedef struct {
  char *method;
  char *path;
  char *version;
  char **headers;
  int header_count;
  char *body;
  size_t body_size;
  size_t body_capacity;
} http_request;

// 释放HTTP请求资源
void free_http_request(http_request *request);

// 查找头部字段
char *find_header(http_request *request, const char *name);

// 解析HTTP头部
int parse_headers(char *buffer, http_request *request);

// 接收完整的HTTP请求
http_request *receive_http_request(int client_socket);