// server.cpp
#include "rudp.h"
#include <cstring> // 用于 strncpy

// qc: 这个版本只是一个简单的示例，主要用于测试基本的 RUDP 功能。
int main(int argc, char* argv[]) {
    std::string process_name = argv[0];
    google::InitGoogleLogging(argv[0]);

    // 日志配置
    FLAGS_log_dir = "./logs";       // 日志目录
    FLAGS_logtostderr = 1;          // 将日志输出到 stderr
    FLAGS_minloglevel = 0;          // 日志级别：INFO 及以上
    FLAGS_v = 2;                    // 详细级别

    // qc: 可以根据需要调整日志级别和输出位置。
    if (argc != 2) {
        LOG(ERROR) << "Usage: " << process_name << " <port>";
        return -1;
    }

    int port = atoi(argv[1]);

    int sockfd;
    sockaddr_in server_addr{}, client_addr{};

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation failed";
        return -1;
    }

    // 绑定套接字
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 检查端口是否已被占用
    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG(ERROR) << "Bind failed";
        close(sockfd);
        return -1;
    }

    // 等待连接
    LOG(INFO) << "Server listening on port " << port;

    // 建立连接（三次握手）
    if (rudp_accept(sockfd, client_addr) == 0) {
        LOG(INFO) << "Connection established with client";
} else {
    LOG(ERROR) << "Failed to establish connection";
        close(sockfd);
        return -1;
    }

    // 从客户端接收数据（例如，“Hello”消息）
    char buffer[DATA_SIZE];
    uint32_t expected_seq = 0;
    ssize_t received_bytes = rudp_receive_data(sockfd, buffer, DATA_SIZE, client_addr, expected_seq);
    if (received_bytes > 0) {
        LOG(INFO) << "Received data from client: " << buffer;
    } else {
         LOG(ERROR) << "Failed to receive data from client";
        // 处理错误或关闭连接  //TODO: 添加更具体的错误处理
    }

    // 向客户端发送数据（例如，“Hello”消息）
    const char* message = "Hello from Server";
    uint32_t seq_num = 0;
    ssize_t sent_bytes = rudp_send_data(sockfd, message, strlen(message) + 1, client_addr, seq_num);
    if (sent_bytes > 0) {
        LOG(INFO) << "Sent data to client: " << message;
    } else {
        LOG(ERROR) << "Failed to send data to client";
        // 处理错误或关闭连接  //FIXME:  这里需要完善错误处理逻辑
    }

    // 等待客户端的关闭请求并响应（四次握手）
    if (rudp_wait_close(sockfd, client_addr) == 0) {
        LOG(INFO) << "Connection termination initiated by client";
    } else {
        LOG(ERROR) << "Failed during connection termination";
    }

    // 关闭套接字
    close(sockfd);
    LOG(INFO) << "Connection closed";
    return 0;
}
