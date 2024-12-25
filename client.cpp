// client.cpp
#include <fstream>

#include "rudp.h"

// 客户端实现，发送文件给服务器，然后接收服务器的文件
int main(int argc, char* argv[]) {
    std::string process_name = argv[0];
    google::InitGoogleLogging(argv[0]);

    // 日志配置
    FLAGS_log_dir = "./logs";  // 日志保存目录
    FLAGS_logtostderr = 1;     // 日志输出到 stderr
    FLAGS_minloglevel = 0;     // 日志级别: INFO 及以上
    FLAGS_colorlogtostderr = true;  // 设置输出到屏幕的日志显示相应颜色
    FLAGS_colorlogtostdout = true;  // 设置输出到标准输出的日志显示相应颜色
    FLAGS_v = 2;                    // 设置详细级别

    if (argc != 3) {
        LOG(ERROR) << "Usage: " << process_name << " <host>:<port> <filename>";
        return -1;
    }

    std::string host_port = argv[1];
    std::string filename = argv[2];

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
    // 按理说这边应该走 DNS
    // 解析，但是不知道为什么没用，所以在inet_pton()之前手动处理
    // 这里的处理不是很优雅，希望有更好的解决方式
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

    // 发送文件给服务器
    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        LOG(ERROR) << "Failed to open file " << filename;
        rudp_close_connection(sockfd, server_addr);
        close(sockfd);
        return -1;
    }

    char buffer[DATA_SIZE];
    ssize_t sent_bytes;
    uint32_t seq_num = 0;
    while (infile.read(buffer, DATA_SIZE) || infile.gcount() > 0) {
        std::streamsize bytes_read = infile.gcount();
        sent_bytes =
            rudp_send_data(sockfd, buffer, bytes_read, server_addr, seq_num);
        if (sent_bytes > 0) {
            LOG(INFO) << "Sent data chunk of size " << sent_bytes;
        } else {
            LOG(ERROR) << "Failed to send data to server";
            // 处理错误或退出
            infile.close();
            rudp_close_connection(sockfd, server_addr);
            close(sockfd);
            return -1;
        }
        // 如果读取的字节数小于 DATA_SIZE，可能是最后一个数据块
        if (bytes_read < DATA_SIZE) {
            break;
        }
    }

    infile.close();
    LOG(INFO) << "File sent to server";

    // 接收服务器发送的文件
    std::ofstream outfile("received_from_server_" + filename, std::ios::binary);
    if (!outfile) {
        LOG(ERROR) << "Failed to create output file";
        close(sockfd);
        return -1;
    }

    ssize_t received_bytes;
    uint32_t expected_seq = 0;
    while (true) {
        received_bytes = rudp_receive_data(sockfd, buffer, DATA_SIZE,
                                           server_addr, expected_seq);
        if (received_bytes > 0) {
            // 写入接收到的数据到文件
            outfile.write(buffer, received_bytes);
            LOG(INFO) << "Received data chunk of size " << received_bytes;
            if (received_bytes < DATA_SIZE) {
                // 可能是最后一个数据包
                break;
            }
        } else if (received_bytes == 0) {
            // 数据接收完成
            break;
        } else {
            LOG(ERROR) << "Failed to receive data from server";
            // 处理错误或退出
            outfile.close();
            close(sockfd);
            return -1;
        }
    }

    outfile.close();
    LOG(INFO) << "File received from server";

    // 等待服务器关闭连接（四次挥手）
    if (rudp_wait_close(sockfd, server_addr) == 0) {
        LOG(INFO) << "Connection closed by server";
    } else {
        LOG(ERROR) << "Failed during connection termination";
    }

    // 关闭 socket
    close(sockfd);
    LOG(INFO) << "Socket closed";
    return 0;
}
