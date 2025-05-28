#include "HttpServer.h"
#include "Database.h"
// 测试命令 curl http://localhost:8080/register -X POST
//curl: (7) Failed to connect to localhost port 8080: Connection refused 可能是由于停止旧的server后没有及时释放8080端口，新server没有占用该端口

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]); // 从命令行获取端口
    }
    printf("port: %d\n", port);
    Database db("users.db");
    HttpServer server(port, 10, db);
    server.setupRoutes();
    server.start();
    return 0;
}