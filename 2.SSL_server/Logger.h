/*************************************************************************
	> File Name: Logger.h
	> Author: 
	> Mail: 
	> Created Time: Sat 10 May 2025 08:38:52 PM CST
 ************************************************************************/

#ifndef _LOGGER_H
#define _LOGGER_H
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdarg> // 引入处理可变参数的头文件

// 日志级别枚举，用于区分不同类别的日志
enum LogLevel {
	INFO,
	WARNING,
	ERROR
};

// Logger类，用于执行日志记录操作
class Logger {
public:
	// logMessage静态成员函数，用于记录日志信息
	// 参数包括日志级别、格式化字符串以及可变参数列表
	static void logMessage(LogLevel level, const char* format, ...) {
		// 打开日志文件，以追加模式输入 ////to learn
		std::ofstream logFile("server.log", std::ios::app);
		// 获取日志时间
		auto now = std::chrono::system_clock::now();
		auto now_c = std::chrono::system_clock::to_time_t(now);

		// 根据日志级别确定日志级别字符串
		std::string levelStr;
		switch (level) {
			case INFO: levelStr = "INFO"; break;
			case WARNING: levelStr = "WARNING"; break;
			case ERROR: levelStr = "ERROR"; break;
		}

		// 使用可变参数处理日志信息的格式化
		va_list args; // 声明可变参数列表
		va_start(args, format); // 初始化args变量，并指向可变参数的第一个参数。format是最后一个命名参数
		char buffer[2048] = {0}; // 声明一个字符数组buffer，大小为2048，用于存储格式化后的日志信息
		vsnprintf(buffer, sizeof(buffer) - 1, format, args); ////这里我也减去了1
		//其中：
		// - buffer 是目标字符串数组
		// - sizeof(buffer) 是写入buffer的最大字符数，防止溢出
		// - format 是格式化字符串，指定日志信息的格式
		// - args 是可变参数列表，包含所有传入的可变参数
		va_end(args); // 清理args，结束可变参数的处理


		// 将时间戳、日志级别和格式化后的日志信息写入日志文件
		logFile << std::ctime(&now_c) << " [" << levelStr << "] " << buffer << std::endl;

		//关闭日志文件
		logFile.close();
	}
};

// 定义宏以简化日志记录操作，提供INFO、WARNING、ERROR三种日志级别的宏 ////注意宏 __VA_ARGS__
#define LOG_INFO(...) Logger::logMessage(INFO, __VA_ARGS__)
#define LOG_WARNING(...) Logger::logMessage(WARNING, __VA_ARGS__)
#define LOG_ERROR(...) Logger::logMessage(ERROR, __VA_ARGS__)


//当在代码中调用LOG_INFO("Hello, %s", name)时,
//宏会替换为Logger::logMessage(INFO, "Hello, %s", name).

#ifdef _D
//#define DBG(fmt, args...) printf(fmt, ##args)
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(fmt, args...) 
#endif
#define NONE  "\e[0m"      //清除颜色，即之后的打印为正常输出，之前的不受影响
#define BLACK  "\e[0;30m"  //深黑
#define L_BLACK  "\e[1;30m" //亮黑，偏灰褐
#define RED   "\e[0;31m" //深红，暗红
#define L_RED  "\e[1;31m" //鲜红
#define GREEN  "\e[0;32m" //深绿，暗绿
#define L_GREEN   "\e[1;32m" //鲜绿
#define BROWN "\e[0;33m" //深黄，暗黄
#define YELLOW "\e[1;33m" //鲜黄
#define BLUE "\e[0;34m" //深蓝，暗蓝
#define L_BLUE "\e[1;34m" //亮蓝，偏白灰
#define PINK "\e[0;35m" //深粉，暗粉，偏暗紫
#define L_PINK "\e[1;35m" //亮粉，偏白灰
#define CYAN "\e[0;36m" //暗青色
#define L_CYAN "\e[1;36m" //鲜亮青色
#define GRAY "\e[0;37m" //灰色
#define WHITE "\e[1;37m" //白色，字体粗一点，比正常大，比bold小
#define BOLD "\e[1m" //白色，粗体
#define UNDERLINE "\e[4m" //下划线，白色，正常大小
#define BLINK "\e[5m" //闪烁，白色，正常大小
#define REVERSE "\e[7m" //反转，即字体背景为白色，字体为黑色
#define HIDE "\e[8m" //隐藏
#define CLEAR "\e[2J" //清除
#define CLRLINE "\r\e[K" //清除行
#endif
