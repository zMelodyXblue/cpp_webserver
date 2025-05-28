/*************************************************************************
	> File Name: HttpServer.h
	> Author: 
	> Mail: 
	> Created Time: Sat 24 May 2025 04:32:24 PM CST
 ************************************************************************/

// #pragma once 是预处理命令，确保头文件在多次包含时只会编译一次，防止重复定义的问题
#ifndef _HTTPSERVER_H
#define _HTTPSERVER_H

#include <stdlib.h> //引入标准库，用于通用工具函数
#include <sys/socket.h> //引入socket编程接口
#include <sys/epoll.h> 
#include <fcntl.h> 
#include <netinet/in.h> // 引入网络字节序转换函数
#include <unistd.h> //引入UNIX标准函数库
#include <cstring>

#include "Logger.h"  //自定义日志模块
#include "ThreadPool.h"  //引入线程池模块，用于并发处理客户端连接请求
#include "Router.h" //引入路由模块，根据HTTP请求的方法和路径分发到不同的处理器
#include "HttpRequest.h"  //引入HTTP请求解析类，用于解析客户端发送过来的请求数据
#include "HttpResponse.h"  //引入HTTP响应构建类，用于构建服务端返回给客户端的响应数据
#include "Database.h"  //引入库，提供与数据库交互的功能

class HttpServer {
public:
	// 构造函数，初始化成员变量并传入参数（端口号，epoll的最大监听事件数以及数据库的引用）
	HttpServer(int port, int max_events, Database& db)
		: server_fd(-1), epollfd(-1), port(port), max_events(max_events), db(db) {}
	// 启动服务器方法，设置套接字、epoll，启动线程池并进入循环等待处理客户端连接
	void start() {
		setupServerSocket(); // 创建并配置服务器套接字
		setupEpoll(); // 创建并配置epoll实例
		ThreadPool pool(16); // 创建一个拥有16个工作线程的线程池以应对高并发场景

		// 初始化epoll_event数组，用于存放epoll_wait返回的就绪事件
		struct epoll_event events[max_events];

		// 主循环，不断等待新的连接请求或已连接套接字上的读写事件
		while (true) {
			int nfds = epoll_wait(epollfd, events, max_events, -1); // 等待epoll事件发生

			// 遍历所有就绪事件
			for (int i = 0; i < nfds; ++i) {
				if (events[i].data.fd == server_fd) {
					acceptConnection();
					continue;
				}
				pool.enqueue([fd = events[i].data.fd, this]() {
					this->handleConnection(fd);
				});
			}
		}
	}
	// 设置服务器路由映射表的方法
	void setupRoutes() {
		// 添加根路由处理器，返回简单的Hello World!响应
		router.addRoute("GET", "/", [](const HttpRequest& req) {
			HttpResponse response;
			response.setStatusCode(200);
			response.setBody("Helloworld!");
			return response;
		});

		// 设置与数据库相关的路由，使用传递进来的数据库引用
		router.setupDatabaseRoutes(db);
	}
	
private:
    // 成员变量：服务器套接字文件描述符、epoll实例的文件描述符、epoll最大监听事件数、监听端口号
    int server_fd, epollfd, max_events, port;
    // Router对象用于处理HTTP请求的路由分发
    Router router;

    // 数据库引用，用于访问和操作数据库
    Database& db; ///

    // 设置服务器套接字的方法，包括创建套接字、配置地址信息、设置重用地址选项、绑定端口、监听连接
	void setupServerSocket() {
		// 创建TCP套接字
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd == -1) {
			LOG_ERROR("Socket creation failed");
		}

		struct sockaddr_in address = {}; // 声明一个用于存储IPv4地址信息的结构体
		// 定义地址并绑定
		address.sin_family = AF_INET; // 设置地址族为IPv4
		address.sin_addr.s_addr = INADDR_ANY; //服务器绑定本地及其的所有可用网络接口
		address.sin_port = htons(port); // 设置服务器端口号，使用htons确保端口号的字节序正确（主机字节序转换为网络字节序）

		// 设置SO_REUSEADDR选项，允许快速重启服务器并重用相同端口
		int opt = 1;
		setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
			LOG_ERROR("Bing failed");
			return ;
		}
		if (listen(server_fd, 3) < 0) {
			LOG_ERROR("Listen failed");
			return ;
		}
		LOG_INFO("Server listening on port %d", port);
		// 设置服务器套接字为非阻塞模式
        setNonBlocking(server_fd);

	}

	void setupEpoll() {
		// 创建epoll实例
		epollfd = epoll_create1(0); // 创建一个新的epoll实例
		if (epollfd == -1) {
			LOG_ERROR("epoll_create1 failed");
			exit(EXIT_FAILURE);
		}
		LOG_INFO("Epoll instance created with fd %d", epollfd);
		// 初始化epoll_event结构体，注册对服务器套接字的EPOLLIN | EPOLLET事件监听
		struct epoll_event event = {};
		// 配置服务器套接字的epoll事件
		event.events = EPOLLIN | EPOLLET; // 监听可读事件并启用边缘触发
		event.data.fd = server_fd; // 关键服务器套接字
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
			LOG_ERROR("Failed to add server_fd to epoll");
			exit(EXIT_FAILURE);
		}
		LOG_INFO("Server socket added to epoll instance");
	}
	
	// 接收新连接的方法，将连接放入epoll监听列表中 ///待改
	void acceptConnection() {
		// 循环接收新的客户端连接请求
		struct sockaddr_in client_addr;
		socklen_t client_addrlen = sizeof(client_addr);
		int client_fd;
		while((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addrlen)) > 0) {
            // 将新接受的客户端套接字设置为非阻塞模式
            setNonBlocking(client_fd);

			// 注册客户端套接字到epoll监听列表，监听EPOLLIN | EPOLLET事件
			struct epoll_event event = {};
			event.events = EPOLLIN | EPOLLET;
			event.data.fd = client_fd;
			epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &event);
		}

		if (client_fd == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
			LOG_ERROR("%d: Error accepting new connection", errno);
		}

	}

	// 处理客户端连接请求的方法，读取请求、路由分发、生成响应并发送回客户端
	void handleConnection(int fd) {
		char buffer[4096] = {0};
		ssize_t bytes_read; // 读取的字节数

		// 循环读取客户端请求数据，直到无数据可读
		while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
			buffer[bytes_read] = '\0'; // 在缓冲区末尾添加结束符便于处理
			DBG(GREEN "request_buffer: %s" NONE"\n", buffer);
			
			// 解析请求数据并创建HttpRequest对象
			HttpRequest request;
			if (request.parse(buffer)) {
				// 根据HttpRequest对象通过Router对象获取对应的HttpResponse对象
				HttpResponse response = router.routeRequest(request);

				// 将HttpResponse对象转换为字符串形式，并发送给客户端
				std::string response_str = response.toString();
				DBG(BLUE "response_str: \n%s" NONE"\n" , response_str.c_str()); ///

				send(fd, response_str.c_str(), response_str.length(), 0);
			}
		}

		if (bytes_read == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK)) { ////
			// 发生错误或连接关闭
			LOG_ERROR("Error reading from socket %d", fd);
			close(fd);

		}
		//关闭客户端连接
		close(fd);
		LOG_INFO("Closed connection on fd %d", fd);
		return ;
	}

	// 设置文件描述符为非阻塞模式的方法
	void setNonBlocking(int sock) {
		int opts = fcntl(sock, F_GETFL, 0); //获取文件描述符的状态标志
		if (opts < 0) {
			LOG_ERROR("fcntl(F_GETFL) failed on socket %d: %s", sock, strerror(errno)); ////
			exit(EXIT_FAILURE);
		}
		opts |= O_NONBLOCK; // 设置非阻塞标志
		if (fcntl(sock, F_SETFL, opts) < 0) {
			LOG_ERROR("fcntl(F_SETFL) failed on socket %d: %s", sock, strerror(errno)); //记录详细错误信息
			exit(EXIT_FAILURE);
		}
		LOG_INFO("Set socket %d to non-blocking", sock);
		return ;
	}
};


#endif
