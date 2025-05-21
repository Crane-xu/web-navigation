#include "static_files.h"

// 发送 HTTP 响应
void send_response(int client_fd, const char *filepath) {
  char buffer[BUFFER_SIZE];
  int file_fd;
  struct stat file_stat;

  // 打开文件
  if ((file_fd = open(filepath, O_RDONLY)) < 0) {
    const char *error_msg =
        "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
    send(client_fd, error_msg, strlen(error_msg), 0);
    return;
  }

  // 获取文件信息
  if (fstat(file_fd, &file_stat) < 0) {
    close(file_fd);
    const char *error_msg =
        "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\n\r\n500 "
        "Internal Error";
    send(client_fd, error_msg, strlen(error_msg), 0);
    return;
  }

  // 发送 HTTP 响应头
  snprintf(buffer, BUFFER_SIZE,
           "HTTP/1.1 200 OK\r\n"
           "Content-Length: %ld\r\n"
           "Content-Type: %s\r\n" // 可以根据需要修改 Content-Type
           "\r\n",
           file_stat.st_size, get_content_type(filepath));
  send(client_fd, buffer, strlen(buffer), 0);

  // 发送文件内容
  ssize_t bytes_read;
  while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
    send(client_fd, buffer, bytes_read, 0);
  }

  close(file_fd);
}