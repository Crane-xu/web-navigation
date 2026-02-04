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
    http_request *request = (http_request *)calloc(1, sizeof(http_request));
    if (!request) {
        fprintf(stderr, "Memory allocation failed for http_request\n");
        return NULL;
    }

    // 接收头部
    char header_buffer[MAX_HEADER_SIZE] = {0};
    size_t header_pos = 0;
    int found_headers_end = 0;

    while (header_pos < MAX_HEADER_SIZE - 1) {
        ssize_t bytes = recv(client_socket, header_buffer + header_pos, 1, 0);
        if (bytes <= 0) {
            if (bytes == 0) {
                fprintf(stderr, "Client disconnected\n");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv failed");
            }
            free_http_request(request);
            return NULL;
        }

        header_pos += bytes;
        
        // 检查是否找到头部结束标记 \r\n\r\n
        if (header_pos >= 4 && 
            memcmp(header_buffer + header_pos - 4, "\r\n\r\n", 4) == 0) {
            found_headers_end = 1;
            break;
        }
    }

    if (!found_headers_end) {
        if (header_pos >= MAX_HEADER_SIZE - 1) {
            fprintf(stderr, "Header too large (max: %d)\n", MAX_HEADER_SIZE);
        } else {
            fprintf(stderr, "Incomplete headers received\n");
        }
        free_http_request(request);
        return NULL;
    }

    header_buffer[header_pos] = '\0';

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
        // 跳过空白字符并验证数字格式
        char *ptr = content_length_str;
        while (isspace(*ptr)) ptr++;
        
        // 验证是否为有效数字
        if (*ptr != '\0') {
            char *endptr;
            content_length = strtol(ptr, &endptr, 10);
            
            // 检查转换是否成功且没有额外字符
            if (endptr == ptr || *endptr != '\0') {
                fprintf(stderr, "Invalid Content-Length format: %s\n", content_length_str);
                content_length = 0;
            } else if (content_length < 0) {
                fprintf(stderr, "Negative Content-Length: %ld\n", content_length);
                content_length = 0;
            }
        }
    }

    // 处理请求体
    if (content_length > 0) {
        // 检查body大小限制
        if (content_length > MAX_BODY_SIZE) {
            fprintf(stderr, "Body too large: %ld (max: %d)\n", content_length, MAX_BODY_SIZE);
            free_http_request(request);
            return NULL;
        }

        // 分配body内存（精确大小）
        request->body = (char *)malloc(content_length + 1); // +1 for safety
        if (!request->body) {
            fprintf(stderr, "Memory allocation failed for body (%ld bytes)\n", content_length);
            free_http_request(request);
            return NULL;
        }

        request->body_capacity = content_length;
        request->body_size = 0;

        // 接收body数据（批量读取提高效率）
        size_t total_received = 0;
        while (total_received < (size_t)content_length) {
            size_t remaining = content_length - total_received;
            size_t chunk_size = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;

            ssize_t bytes = recv(client_socket, request->body + total_received, chunk_size, 0);
            if (bytes <= 0) {
                if (bytes == 0) {
                    fprintf(stderr, "Client disconnected during body transfer\n");
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("recv failed during body transfer");
                }
                free_http_request(request);
                return NULL;
            }
            total_received += bytes;
        }

        request->body_size = total_received;
        request->body[total_received] = '\0'; // 添加字符串结束符
        
        // 验证实际接收大小
        if (request->body_size != (size_t)content_length) {
            fprintf(stderr, "Body size mismatch: expected %ld, got %zu\n", 
                    content_length, request->body_size);
        }
    }

    // 处理multipart/form-data（保持现有逻辑）
    char *content_type = find_header(request, "Content-Type");
    if (content_type && strstr(content_type, "multipart/form-data") != NULL) {
        char *boundary = strstr(content_type, "boundary=");
        if (boundary) {
            boundary += strlen("boundary=");
            // 可以进一步处理边界...
        }
    }

    return request;
}