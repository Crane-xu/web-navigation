#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "action.h"
#include "mime_types.h"
#include "static_files.h"
#include "stringlib.h"
#include "handler.h"
#include "server.h"

int server_fd;
volatile int keep_running = 1;
pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

// 信号处理函数
void handle_sigint(int sig) {
  printf("Received SIGINT (Ctrl+C), shutting down server...\n");
  keep_running = 0;
  close(server_fd);
  exit(EXIT_SUCCESS);
}

// 处理客户端请求的线程函数
// void *handle_client(void *client_socket_void) {
//   int client_socket = *(int *)client_socket_void;
//   free(client_socket_void); // 释放动态分配的内存

//   char buffer[BUFFER_SIZE] = {0};
//   http_parser parser;
//   parser_init(&parser);
//   ssize_t bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
//   if (bytes <= 0) {
//     parser.request_complete = 1;
//   }
//   parser.bytes_received = bytes;
//   handle_request(client_socket, buffer);

//   close(client_socket);
//   return NULL;
// }

int run(char *port_str) {
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  // 注册信号处理函数
  if (signal(SIGINT, handle_sigint) == SIG_ERR) {
    perror("signal");
    exit(EXIT_FAILURE);
  }

  // 创建套接字
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // 设置 SO_REUSEADDR 选项
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  unsigned long port = strtoul(port_str, NULL, 10);
  // 检查端口号是否在有效范围内
  if (port > 65535) {
    fprintf(stderr, "Invalid port number: %s\n", port_str);
    exit(EXIT_FAILURE);
  }

  // 绑定套接字到地址和端口
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  // 将主机字节序转换为网络字节序
  address.sin_port = htons((uint16_t)port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // 监听套接字
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Server is listening on port %s...\n", port_str);

  while (keep_running) {
    int new_socket;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      perror("accept");
      continue;
    }

    // 为每个客户端连接创建一个新线程
    pthread_t thread_id;
    int *client_socket_ptr = (int *)malloc(sizeof(int));
    *client_socket_ptr = new_socket;
    if (pthread_create(&thread_id, NULL, handle_client, client_socket_ptr) !=
        0) {
      perror("pthread_create");
      free(client_socket_ptr);
      close(new_socket);
    }
    // 注意：这里没有调用 pthread_join，因为我们希望服务器能够继续接受新的连接
  }

  close(server_fd);
  printf("Server shut down.\n");
  return 0;
}