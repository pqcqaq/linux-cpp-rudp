// server.cpp
#include "rudp.h"
#include <fstream>

// 这是服务端的实现，为了方便，这里没有考虑多客户机的情况。
// 如果要使用多客户机，可以添加pthread
// 在接收到SYN之后，转到新的线程去处理这个客户机即可
// 主要是懒得搞了，反正效果差不多
int main(int argc, char* argv[]) {
    std::string process_name = argv[0];
    google::InitGoogleLogging(argv[0]);
    // 日志配置
    FLAGS_log_dir = "./logs";       // 日志保存目录
    FLAGS_logtostderr = 1;          // 日志输出到 stderr
    FLAGS_minloglevel = 0;          // 日志级别: INFO 及以上
    FLAGS_v = 2;                    // 设置详细级别

    if (argc != 3) {
        LOG(ERROR) << "Usage: " << process_name << " <port> <filename>";
        return -1;
    }

    int port = atoi(argv[1]);
    std::string filename = argv[2];

    int sockfd;
    sockaddr_in server_addr{}, client_addr{};

    // 创建 UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation failed";
        return -1;
    }

    // 绑定 socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 检查端口是否被占用
    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG(ERROR) << "Bind failed";
        close(sockfd);
        return -1;
    }

    // 等待连接
    LOG(INFO) << "Server listening on port " << port;

    // 连接建立（三次握手）
    if (rudp_accept(sockfd, client_addr) == 0) {
        LOG(INFO) << "Connection established with client";
    } else {
        LOG(ERROR) << "Failed to establish connection";
        close(sockfd);
        return -1;
    }

    // 从客户端接收文件
    std::ofstream outfile("received_from_client_" + filename, std::ios::binary);
    if (!outfile) {
        LOG(ERROR) << "Failed to create output file";
        close(sockfd);
        return -1;
    }

    char buffer[DATA_SIZE];
    ssize_t received_bytes;
    uint32_t expected_seq = 0;
    while (true) {
        received_bytes = rudp_receive_data(sockfd, buffer, DATA_SIZE, client_addr, expected_seq);
        if (received_bytes > 0) {
            // 写入接收到的数据到文件
            outfile.write(buffer, received_bytes);
            LOG(INFO) << "Received data chunk of size " << received_bytes;
            if (received_bytes < DATA_SIZE) {
                // 可能是最后一个数据包
                LOG(INFO) << "Received the last data chunk";
                break;
            }
        } else if (received_bytes == 0) {
            // 数据接收完成
            break;
        } else {
            LOG(ERROR) << "Failed to receive data from client";
            // 处理错误或退出
            break;
        }
    }

    outfile.close();
    LOG(INFO) << "File received from client";

    // 向客户端发送文件
    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        LOG(ERROR) << "Failed to open file " << filename;
        // 可以选择通知客户端失败
        rudp_close_connection(sockfd, client_addr);
        close(sockfd);
        return -1;
    }

    ssize_t sent_bytes;
    uint32_t seq_num = 0;
    while (infile.read(buffer, DATA_SIZE)) {
        std::streamsize bytes_read = infile.gcount();
        sent_bytes = rudp_send_data(sockfd, buffer, bytes_read, client_addr, seq_num);
        if (sent_bytes > 0) {
            LOG(INFO) << "Sent data chunk of size " << sent_bytes;
        } else {
            LOG(ERROR) << "Failed to send data to client";
            // 处理错误或退出
            break;
        }
    }
    // 处理可能的最后一部分数据
    if (infile.eof()) {
        std::streamsize bytes_read = infile.gcount();
        if (bytes_read > 0) {
            sent_bytes = rudp_send_data(sockfd, buffer, bytes_read, client_addr, seq_num);
            if (sent_bytes > 0) {
                LOG(INFO) << "Sent final data chunk of size " << sent_bytes;
            } else {
                LOG(ERROR) << "Failed to send final data to client";
            }
        }
    }

    infile.close();
    LOG(INFO) << "File sent to client";

    // 关闭连接（四次挥手）
    if (rudp_close_connection(sockfd, client_addr) == 0) {
        LOG(INFO) << "Connection closed";
    } else {
        LOG(ERROR) << "Failed to close connection properly";
    }

    // 关闭 socket
    close(sockfd);
    LOG(INFO) << "Socket closed";
    return 0;
}
