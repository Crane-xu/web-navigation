#include "site.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 读取文件内容到字符串
char *read_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("无法打开文件");
    return NULL;
  }

  // 获取文件大小
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // 分配内存以存储文件内容
  char *buffer = (char *)malloc(size + 1);
  if (buffer == NULL) {
    perror("内存分配失败");
    fclose(file);
    return NULL;
  }

  // 读取文件内容
  fread(buffer, 1, size, file);
  buffer[size] = '\0';

  fclose(file);
  return buffer;
}

// 将 JSON 数据写入文件
void write_file(const char *filename, cJSON *root) {
  char *json_str = cJSON_Print(root);
  if (json_str == NULL) {
    fprintf(stderr, "JSON 打印失败\n");
    return;
  }

  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    perror("无法打开文件");
    free(json_str);
    return;
  }

  fputs(json_str, file);
  fclose(file);
  free(json_str);
}

// 查找 site
void find_site(cJSON *root, const char *url) {
  cJSON *sites = cJSON_GetObjectItemCaseSensitive(root, "sites");
  if (cJSON_IsArray(sites)) {
    int array_size = cJSON_GetArraySize(sites);
    for (int i = 0; i < array_size; i++) {
      cJSON *site = cJSON_GetArrayItem(sites, i);
      cJSON *site_url = cJSON_GetObjectItemCaseSensitive(site, "url");
      if (cJSON_IsString(site_url) && strcmp(site_url->valuestring, url) == 0) {
        printf("找到站点:\n");
        char *site_str = cJSON_Print(site);
        printf("%s\n", site_str);
        free(site_str);
        return;
      }
    }
  }
  printf("未找到站点: %s\n", url);
}

// 添加 site
void add_site(cJSON *root, const char *url, const char *img, const char *name) {
  cJSON *sites = cJSON_GetObjectItemCaseSensitive(root, "sites");
  if (!cJSON_IsArray(sites)) {
    sites = cJSON_AddArrayToObject(root, "sites");
  }

  cJSON *new_site = cJSON_CreateObject();
  cJSON_AddStringToObject(new_site, "url", url);
  cJSON_AddStringToObject(new_site, "img", img);
  cJSON_AddStringToObject(new_site, "name", name);

  cJSON_AddItemToArray(sites, new_site);
}

// 删除 site
int delete_site(cJSON *root, const char *url) {
  cJSON *sites = cJSON_GetObjectItemCaseSensitive(root, "sites");
  if (cJSON_IsArray(sites)) {
    int array_size = cJSON_GetArraySize(sites);
    for (int i = 0; i < array_size; i++) {
      cJSON *site = cJSON_GetArrayItem(sites, i);
      cJSON *site_url = cJSON_GetObjectItemCaseSensitive(site, "url");
      if (cJSON_IsString(site_url) && strcmp(site_url->valuestring, url) == 0) {
        cJSON_DeleteItemFromArray(sites, i);
        printf("已删除站点: %s\n", url);
        return 1;
      }
    }
  }
  printf("未找到要删除的站点: %s\n", url);
  return 0;
}

// 修改 site
void modify_site(cJSON *root, const char *old_url, const char *new_url,
                 const char *new_img, const char *new_name) {
  cJSON *sites = cJSON_GetObjectItemCaseSensitive(root, "sites");
  if (cJSON_IsArray(sites)) {
    int array_size = cJSON_GetArraySize(sites);
    for (int i = 0; i < array_size; i++) {
      cJSON *site = cJSON_GetArrayItem(sites, i);
      cJSON *site_url = cJSON_GetObjectItemCaseSensitive(site, "url");
      if (cJSON_IsString(site_url) &&
          strcmp(site_url->valuestring, old_url) == 0) {
        if (new_url != NULL) {
          cJSON_ReplaceItemInObject(site, "url", cJSON_CreateString(new_url));
        }
        if (new_img != NULL) {
          cJSON_ReplaceItemInObject(site, "img", cJSON_CreateString(new_img));
        }
        if (new_name != NULL) {
          cJSON_ReplaceItemInObject(site, "name", cJSON_CreateString(new_name));
        }
        printf("已修改站点: %s\n", old_url);
        return;
      }
    }
  }
  printf("未找到要修改的站点: %s\n", old_url);
}

cJSON *open_sites() {
  const char *filename = "./www/sites.json";
  char *json_text = read_file(filename);
  if (json_text == NULL) {
    return NULL;
  }

  // 解析 JSON 数据
  cJSON *root = cJSON_Parse(json_text);
  if (root == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON 解析错误: %s\n", error_ptr);
    }
    free(json_text);
    return NULL;
  }
  free(json_text);
  return root;
}

void close_sites(cJSON *root) {
  // 写入文件
  write_file("./www/sites.json", root);

  // 释放内存
  cJSON_Delete(root);
}

int test() {
  // const char *filename = "sites.json";
  // char *json_text = read_file(filename);
  // if (json_text == NULL) {
  //   return 1;
  // }

  // 解析 JSON 数据
  cJSON *root = open_sites();
  if (root == NULL) {
    // const char *error_ptr = cJSON_GetErrorPtr();
    // if (error_ptr != NULL) {
    //   fprintf(stderr, "JSON 解析错误: %s\n", error_ptr);
    // }
    // free(json_text);
    return 1;
  }
  // free(json_text);

  // 示例操作
  // 查找
  //   find_site(root, "http://10.168.1.226:5173/");

  // 添加
  //   add_site(root, "http://example.com", "new_img.png", "新站点");

  // 删除
  //   delete_site(root, "http://example.com");

  //   // 修改
  modify_site(root, "http://10.168.1.226:5173/", "http://new-url.com",
              "new_img.png", "新商城后台");

  close_sites(root);
  // // 写入文件
  // write_file("sites.json", root);

  // // 释放内存
  // cJSON_Delete(root);

  return 0;
}