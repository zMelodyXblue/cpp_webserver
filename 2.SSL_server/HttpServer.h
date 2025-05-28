/*************************************************************************
	> File Name: HttpServer.h
	> Author: 
	> Mail: 
	> Created Time: Sat 24 May 2025 04:32:24 PM CST
 ************************************************************************/

// #pragma once 是预处理命令，确保头文件在多次包含时只会编译一次，防止重复定义的问题
#ifndef _HTTPSERVER_H
#define _HTTPSERVER_H
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <stdlib.h> //引入标准库，用于通用工具函数
#include <sys/socket.h> //引入socket编程接口
#include <sys/epoll.h> 
#include <fcntl.h> 
#include <netinet/in.h> // 引入网络字节序转换函数
#include <unistd.h> //引入UNIX标准函数库
#include <cstring>
#include <map>
#include <vector>
#include <functional>
#include <memory>

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
		: server_fd(-1), epollfd(-1), port(port), max_events(max_events), db(db) {
		SSL_library_init(); // 初始化OpenSSL
		OpenSSL_add_ssl_algorithms(); // 加载SSL算法
		SSL_load_error_strings(); // 加载SSL算法
		const SSL_METHOD* method = TLS_server_method(); // 设置SSL方法为TLS_server_method
		sslCtx = SSL_CTX_new(method); // 创建SSL上下文
		if (!sslCtx) {
			throw std::runtime_error("Unable to create SSL context");
		}

		// 加载服务器证书和私钥
		if (SSL_CTX_use_certificate_file(sslCtx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
			SSL_CTX_use_PrivateKey_file(sslCtx, "server.key", SSL_FILETYPE_PEM) <= 0) {
			ERR_print_errors_fp(stderr);
			throw std::runtime_error("Failed to load cert or key file");
		}

		setupRoutes(); // 设置路由
	}

	~HttpServer() {
		SSL_CTX_free(sslCtx); // 释放SSL上下文
		EVP_cleanup(); // 清理加密库
	}

	// 启动服务器方法，设置套接字、epoll，启动线程池并进入循环等待处理客户端连接
	void start() {
		setupServerSocket(); // 创建并配置服务器套接字
		setupEpoll(); // 创建并配置epoll实例
		ThreadPool pool(16); // 创建一个拥有16个工作线程的线程池以应对高并发场景

		// 初始化epoll_event数组，用于存放epoll_wait返回的就绪事件
		std::vector<struct epoll_event> events(max_events); ///

		// 主循环，不断等待新的连接请求或已连接套接字上的读写事件
		while (true) {
			int nfds = epoll_wait(epollfd, events.data(), max_events, -1); // 等待epoll事件发生
			// 遍历所有就绪事件
			for (int i = 0; i < nfds; ++i) {
				if (events[i].data.fd == server_fd) {
					acceptConnection();
					continue;
				}
				LOG_INFO("Handling connection for fd: %d", events[i].data.fd);
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
		LOG_INFO("Routes setup completed."); 
	}
	
private:
    // 成员变量：服务器套接字文件描述符、epoll实例的文件描述符、epoll最大监听事件数、监听端口号
    int server_fd, epollfd, max_events, port;
    Router router; // Router对象用于处理HTTP请求的路由分发
    Database& db; // 数据库引用，用于访问和操作数据库 ///
	SSL_CTX* sslCtx; // SSL上下文
	std::map<int, SSL*> sslMap; // 存储每个连接的SSL对象的映射

	// 添加SSL对象到映射中，用于跟踪每个连接的SSL状态
	void addSSLToMap(int fd, SSL* ssl) {
		sslMap[fd] = ssl; // 将文件描述符与其对应的SSL对象关联
		LOG_INFO("Added SSL object for fd: %d to map", fd);
	}

	// 从映射中获取指定文件描述符对应的SSL对象
	SSL* getSSLFromMap(int fd) {
		auto it = sslMap.find(fd);
		if (it != sslMap.end()) {
			LOG_INFO("Found SSL object for fd: %d in map", fd);
			return it->second;
		}
		LOG_ERROR("getSSL object not found for fd: %d in map", fd);
		return nullptr;
	}

	// 从映射中移除指定文件描述符对应的SSL对象，并释放相关资源
	void removeSSLFromMap(int fd) {
		auto it = sslMap.find(fd);
		if (it == sslMap.end()) {
			return ;
		}
		SSL_free(it->second);
		sslMap.erase(it);
		LOG_INFO("Removed SSL object for fd: %d from map", fd);
		return ;
	}

    // 设置服务器套接字的方法，包括创建套接字、配置地址信息、设置重用地址选项、绑定端口、监听连接
	void setupServerSocket() {
		// 创建TCP套接字
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd == -1) {
			LOG_ERROR("server socket creation failed");
			throw std::runtime_error("socket failed");
		}

		struct sockaddr_in address = {}; // 声明一个用于存储IPv4地址信息的结构体
		// 定义地址并绑定
		address.sin_family = AF_INET; // 设置地址族为IPv4
		address.sin_addr.s_addr = INADDR_ANY; //服务器绑定本地及其的所有可用网络接口
		address.sin_port = htons(port); // 设置服务器端口号，使用htons确保端口号的字节序正确（主机字节序转换为网络字节序）

		int opt = 1;
		// 设置SO_REUSEADDR选项，允许快速重启服务器并重用相同端口
		setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
			LOG_ERROR("Bind failed");
			throw std::runtime_error("bind failed");
		}
		if (listen(server_fd, 3) < 0) {
			LOG_ERROR("Listen failed");
			throw std::runtime_error("listen failed");
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
			throw std::runtime_error("epoll_create1 failed");
			//exit(EXIT_FAILURE);
		}
		LOG_INFO("Epoll instance created with fd %d", epollfd);
		// 初始化epoll_event结构体，注册对服务器套接字的EPOLLIN | EPOLLET事件监听
		struct epoll_event event = {};
		// 配置服务器套接字的epoll事件
		event.events = EPOLLIN | EPOLLET; // 监听可读事件并启用边缘触发
		event.data.fd = server_fd; // 关联服务器socket的文件描述符
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
			LOG_ERROR("Failed to add server_fd to epoll");
			throw std::runtime_error("epoll_ctl failed");
			//exit(EXIT_FAILURE);
		}
		//LOG_INFO("Server socket added to epoll instance");
	}

	// 将新接受的客户端连接添加到epoll监听中，并关联SSL对象
	void addClientToEpoll(int client_fd, SSL* ssl) {
		struct epoll_event event = {0};
		event.events = EPOLLIN | EPOLLET;
		event.data.fd = client_fd;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &event) != 0) {
			LOG_ERROR("Failed to add client socket to epoll");
			SSL_free(ssl); // 释放ssl对象
			close(client_fd); // 关闭客户端连接
		} else {
			addSSLToMap(client_fd, ssl);
			LOG_INFO("Added new client to epoll and ssl map");
		}
		return ;
	}
	// 接收新连接的方法，将连接放入epoll监听列表中
	void acceptConnection() {
		struct sockaddr_in client_addr;
		socklen_t client_addrlen = sizeof(client_addr);
		int client_fd;

		// 循环接受所有到达的连接请求
		while((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addrlen)) > 0) {
			LOG_INFO("Accepted new connection, fd: %d", client_fd); // 日志记录新连接的文件描述符
            setNonBlocking(client_fd); // 将新接受的客户端套接字设置为非阻塞模式

			SSL* ssl = SSL_new(sslCtx); // 为新连接创建一个新的SSL对象
			SSL_set_fd(ssl, client_fd); // 将新创建的SSL对象与客户端的文件描述符绑定
			LOG_INFO("SSL object created and set for fd: %d", client_fd);

			// 尝试进行非阻塞的SSL握手
			while (true) {
				int ssl_err = SSL_accept(ssl); // 进行SSL握手
				if (ssl_err <= 0) {
					// 如果SSL握手未完成
					int err = SSL_get_error(ssl, ssl_err); // 获取SSL错误代码
					if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
						// 如果是因为非阻塞IO而暂时不能继续握手，则需要等待更多的数据 ///因为SSL比较费时间///
						struct epoll_event event = {0}; // 定义epoll事件
						event.events = EPOLLIN | EPOLLET | (err == SSL_ERROR_WANT_WRITE ? EPOLLOUT : 0); // 设置事件类型 ///
						event.data.ptr = ssl; // 将SSL对象作为事件数据 ///
						event.data.fd = client_fd; // 将客户端文件描述符作为事件数据
						if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &event) == 0) {
							addSSLToMap(client_fd, ssl); // 将SSL对象添加到映射中
							break; // 成功添加到epoll后退出循环
						} else {
							LOG_ERROR("Epoll_ctl ADD failed: %s", strerror(errno)); // 记录epoll_ctl失败的日志
							SSL_free(ssl); // 释放SSL对象
							close(client_fd); // 关闭客户端连接
						}
					} else {
						ERR_print_errors_fp(stderr); // 打印SSL错误信息
						SSL_free(ssl); // 释放SSL对象
						close(client_fd); // 关闭客户端连接
					}
					break; // 退出循环
				} else {
					// 如果SSL握手成功
					addClientToEpoll(client_fd, ssl); // 将客户端和其SSL对象添加到epoll监控中
					break;
				}
			}
		}

		if (client_fd == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
			LOG_ERROR("Accept failed: %s", strerror(errno)); // 如果接受连接失败，记录错误日志
		}

	}

	// 处理HTTP请求，包括解析请求、路由处理、通过SSL发送响应
	void processRequest(const char* buffer, int fd, SSL* ssl) {
		HttpRequest request;
		if (!request.parse(buffer)) {
			LOG_ERROR("Failed to parse HTTP request");
			return ;
		}

		// 根据HttpRequest对象通过Router对象获取对应的HttpResponse对象
		HttpResponse response = router.routeRequest(request);
		// 将HttpResponse对象转换为字符串形式，并发送给客户端
		std::string response_str = response.toString();
		DBG(BLUE "response_str: \n%s" NONE"\n" , response_str.c_str()); ///

		int bytes_sent = SSL_write(ssl, response_str.c_str(), response_str.length()); // 通过SSL发送响应
		if (bytes_sent <= 0) {
			int err = SSL_get_error(ssl, bytes_sent); // 获取SSL错误代码
			LOG_ERROR("SSL_write failed with SSL error: %d", err);
		} else {
			LOG_INFO("Response sent to client");
		}
	}

	// 处理客户端连接请求的方法，读取请求、路由分发、生成响应并发送回客户端
	void handleConnection(int fd) {
		SSL* ssl = getSSLFromMap(fd); // 从映射中获取关联的SSL对象
		if (!ssl) {
			LOG_ERROR("SSL object not found for fd: %d", fd);
			close(fd);
			return ;
		}

		char buffer[4096] = {0};
		int bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1); // 通过SSL读取数据 // 默认客户端发送的数据也是经过SSL加密的

		if (bytes_read > 0) {
			buffer[bytes_read] = '\0'; // 确保字符串以空字符结束
			processRequest(buffer, fd, ssl); // 处理HTTP请求
		} else if (bytes_read <= 0) { // 如果读取失败
			int err = SSL_get_error(ssl, bytes_read); // 获取SSL错误代码
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) { // 检查是否是非阻塞IO的正常等待状态
				LOG_INFO("SSL_read needs more data, waiting for next epoll event."); // 记录日志，等待更多数据
			} else {
				LOG_ERROR("SSL_read failed for fd: %d with SSL error: %d", fd, err); // 记录SSL读取失败的错误日志
				ERR_print_errors_fp(stderr); // 打印错误信息到标准错误输出
				removeSSLFromMap(fd); // 从映射中移除SSL对象
				close(fd); // 关闭连接
			}
		}

		return ;
	}

	// 设置文件描述符为非阻塞模式的方法
	void setNonBlocking(int sock) {
		int opts = fcntl(sock, F_GETFL, 0); //获取文件描述符的状态标志
		if (opts == -1) {
			LOG_ERROR("fcntl(F_GETFL) failed on socket %d: %s", sock, strerror(errno)); ////
			throw std::runtime_error("fcntl F_GETFL failed"); // 获取失败，抛出异常
			//exit(EXIT_FAILURE);
		}
		opts |= O_NONBLOCK; // 设置非阻塞标志
		if (fcntl(sock, F_SETFL, opts) < 0) {
			LOG_ERROR("fcntl(F_SETFL) failed on socket %d: %s", sock, strerror(errno));
			throw std::runtime_error("fcntl F_SETFL failed"); // 设置失败，抛出异常
			//exit(EXIT_FAILURE);
		}
		LOG_INFO("Set socket %d to non-blocking", sock);
		return ;
	}
};


#endif
