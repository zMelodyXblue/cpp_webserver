/*************************************************************************
	> File Name: HttpRequest.h
	> Author: 
	> Mail: 
	> Created Time: Sat 24 May 2025 04:31:47 PM CST
 ************************************************************************/

// http_requset.h
#pragma once
#ifndef _HTTPREQUEST_H
#define _HTTPREQUEST_H

#include <string>
#include <unordered_map>
#include <sstream>

#include "Logger.h"

// 定义HttpRequset 类用于解析和存储HTTP请求
class HttpRequest {
public:
	// 枚举类型，定义HTTP请求的方法
	enum Method {
		GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH, UNKNOW
	};

	// 枚举类型，定义解析HTTP请求的不同阶段
	enum ParseState {
		REQUEST_LINE, HEADERS, BODY, FINISH
	};

	HttpRequest() : method(UNKNOW), state(REQUEST_LINE) {}

	/*
	POST /login HTTP/1.1
	Host: localhost:7007
	Content-Type: application/x-222-form-urlencoded
	Content-Length: 30

	username=testname&password=test1
	*/
	// 解析整个HTTP请求的函数
	bool parse(std::string request) {
		std::istringstream iss(request);
		std::string line;
		bool result = true;

		// 按行读取请求，并根据当前解析状态处理每行
		while (std::getline(iss, line) && line != "\r") {
			if (state == REQUEST_LINE) {
				result = parseRequestLine(line);
			} else if (state == HEADERS) {
				result = parseHeader(line);
			}
			if (!result) {
				break; //如果解析失败，则跳出循环
			}
		}

		if (method == POST) {
			body = request.substr(request.find("\r\n\r\n") + 4);
		}
		return result;
	}

	// 解析表单形式的请求体，返回键值对字典
	std::unordered_map<std::string, std::string> parseFormBody() const {
		std::unordered_map<std::string, std::string> params;
		if (method != POST) return params;

		std::istringstream stream(body);
		std::string pair;

		LOG_INFO("Parsing body: %s", body.c_str()); // 记录原始body数据

		while (std::getline(stream, pair, '&')) { ////
			DBG(L_GREEN "parsing pair: %s" NONE "\n", pair.c_str());
			std::string::size_type pos = pair.find('=');
			if (pos == std::string::npos) continue; //
			/*
			// 错误处理：找不到 '=' 分隔符
			std::string error_msg = "Error parsing: " + pair;
			LOG_ERROR(error_msg.c_str()); // 记录错误信息
			std::cerr << error_msg << std::endl;
			*/
			std::string key = pair.substr(0, pos);
			std::string value = pair.substr(pos + 1);
			params[key] = value;

			LOG_INFO("Parsed key-value pair: %s = %s", key.c_str(), value.c_str()); // 记录每个解析出的键值对
		}

		return params;
	}

	// 获取HTTP请求方法的字符串表示
	std::string getMethodString() const {
		switch (method) {
			case GET: return "GET";
			case POST: return "POST";
			// 其他方法
			default: return "UNKNOW";
		}
		return "UNKNOW";
	}

	// 获取请求路径的函数
	const std::string& getPath() const {
		return path;
	}

	// 其他成员函数和变量...

	
private:
	Method method;
	ParseState state; // 请求解析状态
	std::string path, version;
	std::unordered_map<std::string, std::string> headers; // 请求头
	std::string body;

	// 解析请求行的函数
	bool parseRequestLine(const std::string& line) {
		std::istringstream iss(line);
		std::string method_str;
		iss >> method_str;
		if (method_str == "GET") method = GET;
		else if (method_str == "POST") method = POST;
		else method = UNKNOW;

		iss >> path; // 解析请求路径
		iss >> version; // 解析HTTP协议版本
		state = HEADERS;
		return true;
	}

	// 解析请求头的函数
	bool parseHeader(const std::string& line) {
		size_t pos = line.find(": ");
		if (pos == std::string::npos) {
			return false; // 如果格式不正确，则解析失败
		}
		std::string key = line.substr(0, pos);
		std::string value = line.substr(pos + 2);
		headers[key] = value; // 存储键值对到headers字典
		return true;
	}
};

#endif
