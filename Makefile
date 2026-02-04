# Makefile for Web Server Project
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDFLAGS = -lpthread

# 源文件列表
SRCS = main.c server.c http_parser.c handler.c action.c site.c \
       static_files.c cJSON.c stringlib.c mime_types.c

# 生成对应的目标文件列表
OBJS = $(SRCS:.c=.o)

# 头文件路径
INCLUDES = -I.

# 最终目标
TARGET = webserver

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 编译每个.c文件为.o文件
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 清理编译生成的文件
clean:
	rm -f $(OBJS) $(TARGET)

# 安装到系统路径
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# 发布版本
release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)

.PHONY: all clean install debug release