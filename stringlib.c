#include "stringlib.h"
#include <string.h>

int starts_with(const char *str, const char *prefix) {
  size_t len_str = strlen(str);
  size_t len_prefix = strlen(prefix);

  if (len_prefix > len_str) {
    return 0;
  }

  for (size_t i = 0; i < len_prefix; i++) {
    if (str[i] != prefix[i])
      return 0;
  }
  return 1;
}

char *get_filename(const char *buffer) {
  // 查找文件名
  char *filename_start = (char *)strstr(buffer, "filename=\"");
  if (filename_start == NULL) {
    return NULL;
  }
  filename_start += strlen("filename=\"");
  char *filename_end = strchr(filename_start, '\"');
  if (filename_end == NULL) {
    return NULL;
  }
  size_t filename_length = filename_end - filename_start;
  char *filename = (char *)malloc(filename_length + 1);
  if (filename == NULL) {
    return NULL;
  }
  strncpy(filename, filename_start, filename_length);
  filename[filename_length] = '\0';
  return filename;
}

