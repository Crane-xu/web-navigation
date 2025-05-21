#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "action.h"
#include "cJSON.h"
#include "site.h"
#include "static_files.h"
#include "stringlib.h"
#include "handler.h"

void write_filepath(char *filepath, int fplen, char *path) {
  snprintf(filepath, fplen, ".%s",
           path); // 简单处理，添加前缀"."表示当前目录
  if (filepath[strlen(filepath) - 1] == '/') {
    strcat(filepath, "index.html"); // 默认请求 index.html
  }
}

void handle_static(int client_fd, http_request *r) {
  char filepath[1024];
  write_filepath(filepath, 1024, r->path);
  send_response(client_fd, filepath);
}

void handle_upload(int client_fd, http_request *r) {
  char *filename = get_filename(r->body);
  if (filename == NULL) {
    action(client_fd, "{\"message\":\"查找文件名败！\",\"status\":200}");
    return;
  }
  char fpath[30];
  sprintf(fpath, "./www/img/%s", filename);
  // 保存上传的文件
  FILE *file = fopen(fpath, "wb");
  if (file == NULL) {
    action(client_fd, "{\"message\":\"文件上传失败！\",\"status\":200}");
  }
  const char *body_start = strstr(r->body, "\r\n\r\n");
  body_start += 4;
  fwrite(body_start, 1, r->body_size - (body_start - r->body), file);
  fclose(file);

  char response[1024];
  snprintf(response, 1024,
           "{\"message\":\"文件保存成功！\","
           "\"data\": \"%s\","
           "\"status\":200}",
           filename);
  action(client_fd, response);
  free(filename);
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
        "{\"message\":\"请求错误，请检查参数！\",\"status\":500,\"ok\":false}");
    return;
  }
  // cJSON_IsObject(const cJSON *const item)
  //
  cJSON *operate = cJSON_GetObjectItemCaseSensitive(request_root, "operate");
  cJSON *site = cJSON_GetObjectItemCaseSensitive(request_root, "site");
  if (site == NULL) {
    action(
        client_fd,
        "{\"message\":\"请求错误，请检查参数！\",\"status\":500,\"ok\":false}");
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