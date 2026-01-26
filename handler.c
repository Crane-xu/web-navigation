#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "action.h"
#include "cJSON.h"
#include "site.h"
#include "static_files.h"
#include "stringlib.h"
#include "handler.h"

// URL解码（精简版）
void url_decode(char* dst, const char* src) {
    while (*src) {
        if (*src == '%' && isxdigit(src[1]) && isxdigit(src[2])) {
            *dst++ = (char)strtol(src + 1, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

void write_filepath(char* filepath, int fplen, const char* path) {
    // 解码URL路径
    char decoded[1024];
    url_decode(decoded, path);
    
    // 构造文件路径
    snprintf(filepath, fplen, ".%s", decoded);
    
    // 如果是目录，添加默认文件
    size_t len = strlen(filepath);
    if (len > 0 && filepath[len-1] == '/' && len + 10 < (size_t)fplen) {
        strcat(filepath, "index.html");
    }
}

void handle_static(int client_fd, http_request* r) {
    char filepath[1024];
    write_filepath(filepath, sizeof(filepath), r->path);
    
    // 安全检查
    if (strstr(filepath, "..")) {
        const char* resp = "HTTP/1.1 403 Forbidden\r\n\r\n";
        write(client_fd, resp, strlen(resp));
        return;
    }
    
    // 发送文件
    send_response(client_fd, filepath);
}

void handle_upload(int client_fd, http_request *r) {
  // 1. 获取文件名
  char *filename = get_filename(r->body);
  if (!filename) {
    send_json(client_fd, 400, "查找文件名失败！");
    return;
  }

  // 2. 打开文件
  char fpath[256];
  snprintf(fpath, sizeof(fpath), "./www/img/%s", filename);
  FILE *file = fopen(fpath, "wb");
  if (!file) {
    send_json(client_fd, 500, "无法创建文件");
    free(filename);
    return;
  }

  // 3. 解析 boundary
  char *content_type = find_header(r, "Content-Type");
  if (!content_type) {
    cleanup(client_fd, file, fpath, filename, NULL, 400, "缺少 Content-Type");
    return;
  }

  char boundary[256];
  if (!parse_boundary(content_type, boundary, sizeof(boundary))) {
    cleanup(client_fd, file, fpath, filename, NULL, 400, "无效的 boundary");
    return;
  }

  // 4. 查找文件数据
  size_t data_len;
  const char *file_data = find_file_data(r->body, r->body_size, boundary, filename, &data_len);
  
  if (!file_data) {
    cleanup(client_fd, file, fpath, filename, NULL, 400, "未找到文件数据");
    return;
  }

  // 5. 写入文件（去掉末尾的 \r\n）
  if (data_len >= 2 && file_data[data_len-2] == '\r' && file_data[data_len-1] == '\n') {
    data_len -= 2;
  }
  
  if (fwrite(file_data, 1, data_len, file) != data_len) {
    cleanup(client_fd, file, fpath, filename, NULL, 500, "文件写入失败");
    return;
  }

  fclose(file);
  send_json_with_data(client_fd, 200, "文件保存成功", filename, data_len);
  free(filename);
}

// 辅助函数定义
void send_json(int client_fd, int status, const char *message) {
  char response[512];
  snprintf(response, sizeof(response),
           "{\"message\":\"%s\",\"status\":%d}",
           message, status);
  action(client_fd, response);
}

void send_json_with_data(int client_fd, int status, const char *message, 
                         const char *filename, size_t size) {
  char response[1024];
  snprintf(response, sizeof(response),
           "{\"message\":\"%s\",\"data\":\"%s\",\"size\":%zu,\"status\":%d}",
           message, filename, size, status);
  action(client_fd, response);
}

void cleanup(int client_fd, FILE *file, const char *fpath, 
             char *filename, char *end_boundary, int status, const char *msg) {
  if (file) fclose(file);
  if (fpath && file) remove(fpath);  // 只有打开文件失败时才remove
  if (filename) free(filename);
  if (end_boundary) free(end_boundary);
  send_json(client_fd, status, msg);
}

int parse_boundary(const char *content_type, char *boundary, size_t max_len) {
  const char *b = strstr(content_type, "boundary=");
  if (!b) return 0;
  
  b += strlen("boundary=");
  int i = 0, in_quotes = 0;
  
  while (i < max_len-1 && *b && *b != ';' && *b != '\r' && *b != '\n') {
    if (*b == '"') {
      in_quotes = !in_quotes;
    } else if (in_quotes || (*b != ' ' && *b != '\t')) {
      boundary[i++] = *b;
    }
    b++;
  }
  boundary[i] = '\0';
  return i > 0;
}

const char *find_file_data(const char *body, size_t body_len, 
                          const char *boundary, const char *filename,
                          size_t *data_len) {
  // 构造 boundary 标记
  char start_boundary[300], end_boundary[300];
  snprintf(start_boundary, sizeof(start_boundary), "--%s\r\n", boundary);
  snprintf(end_boundary, sizeof(end_boundary), "--%s--", boundary);
  
  size_t start_len = strlen(start_boundary);
  size_t end_len = strlen(end_boundary);
  
  const char *pos = body;
  size_t remaining = body_len;
  
  // 查找第一个 boundary
  const char *b_start = memfind(pos, remaining, start_boundary, start_len);
  if (!b_start) return NULL;
  
  pos = b_start + start_len;
  remaining = body_len - (pos - body);
  
  // 查找包含文件名的部分
  char filename_pattern[512];
  snprintf(filename_pattern, sizeof(filename_pattern), "filename=\"%s\"", filename);
  size_t pattern_len = strlen(filename_pattern);
  
  const char *fpos = memfind(pos, remaining, filename_pattern, pattern_len);
  if (!fpos) return NULL;
  
  // 查找数据开始位置
  const char *data_start = memfind(fpos, remaining - (fpos - pos), "\r\n\r\n", 4);
  if (!data_start) return NULL;
  
  data_start += 4;
  
  // 查找数据结束位置（下一个 boundary 或结束 boundary）
  const char *next_b = memfind(data_start, body_len - (data_start - body), "\r\n--", 4);
  const char *end_b = memfind(data_start, body_len - (data_start - body), end_boundary, end_len);
  
  const char *data_end = NULL;
  if (next_b && (!end_b || next_b < end_b)) {
    data_end = next_b;
  } else if (end_b) {
    data_end = end_b;
  }
  
  if (!data_end) return NULL;
  
  *data_len = data_end - data_start;
  return data_start;
}

// 二进制数据查找函数
const char *memfind(const char *haystack, size_t h_len, 
                   const char *needle, size_t n_len) {
  if (h_len < n_len || n_len == 0) return NULL;
  
  for (size_t i = 0; i <= h_len - n_len; i++) {
    if (memcmp(haystack + i, needle, n_len) == 0) {
      return haystack + i;
    }
  }
  return NULL;
}

void handle_sites(int client_fd, http_request *r) {
  // 解析 body JSON 数据
  cJSON *request_root = cJSON_Parse(r->body);
  if (request_root == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON 解析错误: %s\n", error_ptr);
    }
    action(
        client_fd,
        "{\"message\":\"请求错误，请检查参数！\",\"status\":400,\"ok\":false}");
    return;
  }
  // cJSON_IsObject(const cJSON *const item)
  //
  cJSON *operate = cJSON_GetObjectItemCaseSensitive(request_root, "operate");
  cJSON *site = cJSON_GetObjectItemCaseSensitive(request_root, "site");
  if (site == NULL) {
    action(
        client_fd,
        "{\"message\":\"请求错误，请检查参数！\",\"status\":400,\"ok\":false}");
    return;
  }
  cJSON *site_url = cJSON_GetObjectItemCaseSensitive(site, "url");
  cJSON *site_img = cJSON_GetObjectItemCaseSensitive(site, "img");
  cJSON *site_name = cJSON_GetObjectItemCaseSensitive(site, "name");
  if (cJSON_IsString(operate) && strcmp(operate->valuestring, "insert") == 0) {
    // 添加站点
    if (cJSON_IsString(site_url) && cJSON_IsString(site_img) &&
        cJSON_IsString(site_name)) {
      cJSON *sites_root = open_sites();
      if (sites_root == NULL) {
        action(
            client_fd,
            "{\"message\":\"读取站点数据失败！\",\"status\":500,\"ok\":false}");
        return;
      }
      add_site(sites_root, site_url->valuestring, site_img->valuestring,
               site_name->valuestring);
      close_sites(sites_root);
    }
    action(client_fd,
           "{\"message\":\"已添加站点！\",\"status\":200,\"ok\":true}");
    return;

  } else if (cJSON_IsString(operate) &&
             strcmp(operate->valuestring, "remove") == 0) {
    // 移除站点
    if (cJSON_IsString(site_url)) {
      cJSON *sites_root = open_sites();
      if (sites_root == NULL) {
        action(
            client_fd,
            "{\"message\":\"读取站点数据失败！\",\"status\":500,\"ok\":false}");
        return;
      }
      if (delete_site(sites_root, site_url->valuestring)) {
        // 删除图片文件
        if (cJSON_IsString(site_img)) {
          char img_path[256];
          const char *filename = strrchr(site_img->valuestring, '/');
          snprintf(img_path, sizeof(img_path), "./www/img/%s", 
                   filename ? filename + 1 : site_img->valuestring);
          remove(img_path);  // 忽略删除结果
        }

        action(client_fd,
               "{\"message\":\"已删除站点！\",\"status\":200,\"ok\":true}");
      } else {
        action(client_fd, "{\"message\":\"未找到要删除的站点！\",\"status\":"
                          "200,\"ok\":false}");
      }
      close_sites(sites_root);
    }
    return;
  }

  action(
      client_fd,
      "{\"message\":\"请求错误，请检查参数！\",\"status\":400,\"ok\":false}");
}

void handle_default(int client_fd, http_request *r) {
  action(client_fd, "{\"message\":\"Not Found\",\"status\":404}");
}

// 映射表
request_mapping mappings[] = {{"GET", "/www", handle_static},
                              {"POST", "/upload", handle_upload},
                              {"POST", "/sites", handle_sites},
                              {NULL, NULL, handle_default}};

void *handle_client(void *client_socket_void) {
  int client_socket = *(int *)client_socket_void;
  free(client_socket_void); // 释放动态分配的内存

  // 接收HTTP请求
  http_request *request = receive_http_request(client_socket);
  if (request) {
    // printf("Request received:\n");
    // printf("Method: %s\n", request->method);
    // printf("Path: %s\n", request->path);
    // printf("Version: %s\n", request->version);
    // printf("Headers count: %d\n", request->header_count);

    // // 打印所有头部
    // for (int i = 0; i < request->header_count; i++) {
    //   printf("Header %d: %s\n", i, request->headers[i]);
    // }

    // printf("Body size: %zu\n", request->body_size);

    for (int i = 0; i < sizeof(mappings) / sizeof(mappings[0]); i++) {
      request_mapping *mapping = &mappings[i];
      if ((mapping->method == NULL ||
           strcmp(mapping->method, request->method) == 0) &&
          (mapping->path == NULL ||
           (mapping->path[0] == '/' &&
            starts_with(request->path, mapping->path)) ||
           strcmp(mapping->path, request->path) == 0)) {
        mapping->handler(client_socket, request);
        break;
      }
    }
    free_http_request(request);
  }

  close(client_socket);
  return NULL;
}