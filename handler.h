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

void send_json(int client_fd, int status, const char *message);

void cleanup(int client_fd, FILE *file, const char *fpath, 
             char *filename, char *end_boundary, int status, const char *msg);

const char *memfind(const char *haystack, size_t h_len, 
                   const char *needle, size_t n_len);

const char *find_file_data(const char *body, size_t body_len, 
      const char *boundary, const char *filename,
      size_t *data_len);

int parse_boundary(const char *content_type, char *boundary, size_t max_len);

void send_json_with_data(int client_fd, int status, const char *message, 
                         const char *filename, size_t size);