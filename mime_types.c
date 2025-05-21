#include <string.h>
#include "mime_types.h"

// 一个简单的函数来获取文件的内容类型（MIME类型）
const char *get_content_type(const char *filepath) {
  const char *ext = strrchr(filepath, '.');
  if (!ext)
    return "text/plain"; // 默认类型

  if (strcmp(ext, ".html") == 0)
    return "text/html";
  if (strcmp(ext, ".css") == 0)
    return "text/css";
  if (strcmp(ext, ".js") == 0)
    return "application/javascript";
  // 可以根据需要添加更多类型
  return "text/plain"; // 默认返回
}