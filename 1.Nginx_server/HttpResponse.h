/*************************************************************************
	> File Name: HttpResponse.h
	> Author: 
	> Mail: 
	> Created Time: Sat 24 May 2025 04:32:07 PM CST
 ************************************************************************/

//http_response.h
#pragma once
#ifndef _HTTPRESPONSE_H
#define _HTTPRESPONSE_H

#include <string>
#include <unordered_map>
#include <sstream>

class HttpResponse {
public:
	HttpResponse(int code = 200) : statusCode(code) {}

	void setStatusCode(int code) {
		statusCode = code;
	}

	void setHeader(const std::string name, const std::string& value) {
		headers[name] = value;
	}

	void setBody(const std::string& b) {
		body = b;
	}

	//将响应转换为字符串
	std::string toString() const {
		std::ostringstream oss;
		oss << "HTTP/1.1 " << statusCode << " " << getStatusMessage() << "\r\n";
		// 添加其他响应头
		for (const auto& header: headers) {
			oss << header.first << ": " << header.second << "\r\n";
		}
		// 添加空行分割响应头和响应体
		oss << "\r\n" << body;
		return oss.str();
	}

	// 创建一个包含错误信息的响应 ///
	static HttpResponse makeErrorResponse(int code, const std::string& message) {
		HttpResponse response(code);
		response.setBody(message);
		return response;
	}

	// 创建一个包含成功信息的响应 ///
	static HttpResponse makeOkResponse(const std::string& message) {
		HttpResponse response(200);
		response.setBody(message);
		return response;
	}

private:
	int statusCode; // 响应状态码
	std::unordered_map<std::string, std::string> headers; //响应头信息
	std::string body; // 响应体

	std::string getStatusMessage() const {
		switch (statusCode) {
			case 200: return "OK";
			case 404: return "Not Found";
			//其他

			default: return "Unknown";
		}

	}
};

#endif
