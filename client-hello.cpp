// client.cpp
#include <cstring>  // 为 strncpy 引入头文件

#include "rudp.h"

// 这是客户端的实现
int main(int argc, char* argv[]) {
    // 进程名称
    std::string process_name = argv[0];
    google::InitGoogleLogging(argv[0]);

    // 日志配置
    FLAGS_log_dir = "./logs";       // 日志保存目录
    FLAGS_logtostderr = 1;          // 日志输出到 stderr
    FLAGS_minloglevel = 0;          // 日志级别: INFO 及以上
    FLAGS_colorlogtostderr = true;  // 设置输出到屏幕的日志显示相应颜色
    FLAGS_colorlogtostdout = true;  // 设置输出到标准输出的日志显示相应颜色
    FLAGS_v = 2;                    // 设置详细级别

    if (argc != 2) {
        LOG(ERROR) << "Usage: " << process_name << " <host>:<port>";
        return -1;
    }

    std::string host_port = argv[1];

    size_t colon_pos = host_port.find(':');
    if (colon_pos == std::string::npos) {
        LOG(ERROR) << "Invalid host:port format";
        return -1;
    }

    std::string host = host_port.substr(0, colon_pos);
    int port = atoi(host_port.substr(colon_pos + 1).c_str());

    int sockfd;
    sockaddr_in server_addr{};

    // 创建 UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation failed";
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // 处理 "localhost" 地址
    if (host == "localhost") {
        host = "127.0.0.1";
    }

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        LOG(ERROR) << "Invalid address/ Address not supported";
        close(sockfd);
        return -1;
    }

    // 连接建立（三次握手）
    if (rudp_connect(sockfd, server_addr) == 0) {
        LOG(INFO) << "Connected to server";
    } else {
        LOG(ERROR) << "Failed to connect to server";
        close(sockfd);
        return -1;
    }

    // 向服务器发送数据（例如发送 "Hello World"）
    const char* message = "Hello from Client";
    uint32_t seq_num = 0;
    ssize_t sent_bytes = rudp_send_data(sockfd, message, strlen(message) + 1,
                                        server_addr, seq_num);
    if (sent_bytes > 0) {
        LOG(INFO) << "Sent data to server: " << message;
    } else {
        LOG(ERROR) << "Failed to send data to server";
        // 处理错误或关闭连接
    }

    // 从服务器接收数据（例如接收 "Hello" 消息）
    char buffer[DATA_SIZE];
    uint32_t expected_seq = 0;
    ssize_t received_bytes =
        rudp_receive_data(sockfd, buffer, DATA_SIZE, server_addr, expected_seq);
    if (received_bytes > 0) {
        LOG(INFO) << "Received data from server: " << buffer;
    } else {
        LOG(ERROR) << "Failed to receive data from server";
        // 处理错误或关闭连接
    }

    // 关闭连接（四次挥手）
    if (rudp_close_connection(sockfd, server_addr) == 0) {
        LOG(INFO) << "Connection closed";
    } else {
        LOG(ERROR) << "Failed to close connection";
    }

    // 关闭 socket
    close(sockfd);
    LOG(INFO) << "Socket closed";
    return 0;
}
