#define _GNU_SOURCE 1
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "constants.h"
#include "http_parser.h"

// 释放HTTP请求资源
void free_http_request(http_request *request) {
  if (request) {
    if (request->method)
      free(request->method);
    if (request->path)
      free(request->path);
    if (request->version)
      free(request->version);

    for (int i = 0; i < request->header_count; i++) {
      if (request->headers[i])
        free(request->headers[i]);
    }

    if (request->headers)
      free(request->headers);
    if (request->body)
      free(request->body);

    free(request);
  }
}

// 查找头部字段
char *find_header(http_request *request, const char *name) {
  for (int i = 0; i < request->header_count; i++) {
    if (strncasecmp(request->headers[i], name, strlen(name)) == 0 &&
        request->headers[i][strlen(name)] == ':') {
      return request->headers[i] + strlen(name) + 1;
    }
  }
  return NULL;
}

// 解析HTTP头部
int parse_headers(char *buffer, http_request *request) {
  char *line = buffer;
  char *end = buffer + strlen(buffer);

  // 解析请求行
  char *method_end = strchr(line, ' ');
  char *path_end = strchr(method_end + 1, ' ');
  char *version_end = strchr(path_end + 1, '\r');

  if (!method_end || !path_end || !version_end) {
    fprintf(stderr, "Invalid request line\n");
    return 0;
  }

  // 分配内存并复制请求行各部分
  size_t method_len = method_end - line;
  size_t path_len = path_end - (method_end + 1);
  size_t version_len = version_end - (path_end + 1);

  request->method = (char *)malloc(method_len + 1);
  request->path = (char *)malloc(path_len + 1);
  request->version = (char *)malloc(version_len + 1);

  if (!request->method || !request->path || !request->version) {
    fprintf(stderr, "Memory allocation failed\n");
    return 0;
  }

  memcpy(request->method, line, method_len);
  request->method[method_len] = '\0';

  memcpy(request->path, method_end + 1, path_len);
  request->path[path_len] = '\0';

  memcpy(request->version, path_end + 1, version_len);
  request->version[version_len] = '\0';

  // 解析头部字段
  request->headers = NULL;
  request->header_count = 0;

  line = version_end + 2; // 跳过\r\n

  while (line < end && *line != '\r') {
    char *line_end = strchr(line, '\r');
    if (!line_end)
      break;

    size_t line_len = line_end - line;
    char *header_line = (char *)malloc(line_len + 1);
    if (!header_line) {
      fprintf(stderr, "Memory allocation failed\n");
      return 0;
    }

    memcpy(header_line, line, line_len);
    header_line[line_len] = '\0';

    // 添加到头部数组
    request->headers = (char **)realloc(
        request->headers, (request->header_count + 1) * sizeof(char *));
    request->headers[request->header_count] = header_line;
    request->header_count++;

    line = line_end + 2; // 跳过\r\n
  }

  return 1;
}

// 接收完整的HTTP请求
http_request *receive_http_request(int client_socket) {
  http_request *request = (http_request *)malloc(sizeof(http_request));
  if (!request)
    return NULL;

  // 初始化请求结构
  request->method = NULL;
  request->path = NULL;
  request->version = NULL;
  request->headers = NULL;
  request->header_count = 0;
  request->body = NULL;
  request->body_size = 0;
  request->body_capacity = 0;

  // 接收头部
  char header_buffer[MAX_HEADER_SIZE];
  size_t header_pos = 0;
  int found_headers_end = 0;

  while (header_pos < MAX_HEADER_SIZE - 1) {
    // 接收数据
    ssize_t bytes = recv(client_socket, header_buffer + header_pos, 1, 0);
    if (bytes <= 0) {
      perror("recv failed");
      free_http_request(request);
      return NULL;
    }

    header_pos += bytes;
    header_buffer[header_pos] = '\0';

    // 检查是否找到头部结束标记 \r\n\r\n
    if (header_pos >= 4 &&
        strncmp(header_buffer + header_pos - 4, "\r\n\r\n", 4) == 0) {
      found_headers_end = 1;
      break;
    }
  }

  if (!found_headers_end) {
    fprintf(stderr, "Header too large or incomplete\n");
    free_http_request(request);
    return NULL;
  }

  // 解析头部
  if (!parse_headers(header_buffer, request)) {
    fprintf(stderr, "Failed to parse headers\n");
    free_http_request(request);
    return NULL;
  }

  // 查找 Content-Length
  char *content_length_str = find_header(request, "Content-Length");
  long content_length = 0;

  if (content_length_str) {
    // 跳过空白字符
    while (isspace(*content_length_str))
      content_length_str++;
    content_length = atol(content_length_str);

    if (content_length > 0) {
      // 分配body内存
      request->body_capacity =
          (content_length < MAX_BODY_SIZE) ? content_length : MAX_BODY_SIZE;
      request->body = (char *)malloc(request->body_capacity);
      if (!request->body) {
        fprintf(stderr, "Memory allocation failed\n");
        free_http_request(request);
        return NULL;
      }

      // 接收body
      size_t bytes_received = 0;
      while (bytes_received < content_length) {
        size_t remaining = content_length - bytes_received;
        size_t chunk_size = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;

        ssize_t bytes =
            recv(client_socket, request->body + bytes_received, chunk_size, 0);
        if (bytes <= 0) {
          perror("recv failed");
          free_http_request(request);
          return NULL;
        }

        bytes_received += bytes;
      }

      request->body_size = bytes_received;
    }
  }

  // 查找 Content-Type (处理multipart/form-data)
  char *content_type = find_header(request, "Content-Type");
  if (content_type && strstr(content_type, "multipart/form-data") != NULL) {
    // 处理multipart表单数据
    char *boundary = strstr(content_type, "boundary=");
    if (boundary) {
      boundary += strlen("boundary=");
      // 可以进一步处理边界...
    }
  }

  return request;
}