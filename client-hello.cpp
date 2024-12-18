// client.cpp
#include "rudp.h"
#include <cstring> // 为 strncpy 引入头文件

// 这里是客户端的实现
// 先发文件，再收文件
// 三次握手和四次挥手
int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    // 日志配置
    FLAGS_log_dir = "./logs";       // 日志保存目录
    FLAGS_logtostderr = 1;          // 日志输出到 stderr
    FLAGS_minloglevel = 0;          // 日志级别: INFO 及以上
    FLAGS_v = 2;                    // 设置详细级别

    if (argc != 3) {
        LOG(ERROR) << "Usage: ./client <host>:<port> <filename>";
        return -1;
    }

    std::string host_port = argv[1];
    std::string filename = argv[2];

    // 妈的这里要单独判断localhost的话感觉很不爽，就这样好了
    size_t colon_pos = host_port.find(':');
    if (colon_pos == std::string::npos) {
        LOG(ERROR) << "Invalid host:port format";
        return -1;
    }

    // 没法支持localhost，解析不了这个ip
    std::string host = host_port.substr(0, colon_pos);
    int port = atoi(host_port.substr(colon_pos + 1).c_str());

    int sockfd;
    sockaddr_in server_addr{};

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation failed";
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        LOG(ERROR) << "Invalid address/ Address not supported";
        close(sockfd);
        return -1;
    }

    // Connection establishment (three-way handshake)
    Packet pkt;
    Packet recv_pkt;

    // Send SYN 一旦连接创建出来了，就先发一个握手包，看看能不能通
    pkt.type = SYN;
    pkt.seq = 0;
    sendPacket(sockfd, pkt, server_addr);
    LOG(INFO) << "Sent SYN to server";

    // Wait for SYN-ACK
    while (true) {
        ssize_t n = recvPacket(sockfd, recv_pkt, server_addr);
        if (n > 0 && recv_pkt.type == SYN_ACK) {
            LOG(INFO) << "Received SYN-ACK from server";
            // Send ACK
            pkt.type = ACK;
            pkt.seq = recv_pkt.seq;
            sendPacket(sockfd, pkt, server_addr);
            LOG(INFO) << "Sent ACK to server";
            break; // Connection established
        }
    }

    // 给服务器发信息
    uint32_t seq_num = 0;
    while (true) {
        Packet data_pkt;
        data_pkt.type = DATA;
        data_pkt.seq = seq_num;
        // 原始字符数组
        char char_array[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '\0'}; // 注意结尾加 \0 以标识字符串结束
        // 使用 strncpy 将字符串复制到固定大小的 char 数组中
        std::strncpy(data_pkt.data, char_array, DATA_SIZE - 1); // 确保不会溢出，并保留最后的空终止符
        data_pkt.data[DATA_SIZE - 1] = '\0'; // 明确添加字符串结束符
        sendPacket(sockfd, data_pkt, server_addr);
        LOG(INFO) << "Sent data packet with seq " << seq_num;
        // Wait for ACK
        ssize_t n = recvPacket(sockfd, pkt, server_addr);
        if (n > 0 && pkt.type == DATA_ACK && pkt.seq == seq_num) {
            LOG(INFO) << "Received ACK for seq " << seq_num;
            seq_num++;
            break;
        } else {
            LOG(WARNING) << "No ACK or wrong ACK received, resending packet";
            // Resend the packet
            continue;
        }
    }

    // 这里已经可以断开连接了，实际上等服务端的FIN-ASK的时候，中间再去接受个文件也行
    // Initiate connection termination
    Packet fin_pkt;
    fin_pkt.type = FIN;
    sendPacket(sockfd, fin_pkt, server_addr);
    LOG(INFO) << "Sent FIN to server";

    // Wait for FIN-ACK
    while (true) {
        ssize_t n = recvPacket(sockfd, recv_pkt, server_addr);
        if (n > 0 && recv_pkt.type == FIN_ACK) {
            LOG(INFO) << "Received FIN-ACK from server";
            break;
        }
    }

    // 这里去接受服务器的信息
    uint32_t expected_seq = 0; // 期望收到的数据报数量
     while (true) {
        ssize_t n = recvPacket(sockfd, pkt, server_addr);
         if (n > 0 && pkt.type == DATA) {
            if (pkt.seq == expected_seq) {
                LOG(INFO) << "Received data packet with seq " << pkt.seq << "data is: " << pkt.data;
                // Send DATA_ACK 表示收到数据报
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = pkt.seq;
                sendPacket(sockfd, ack_pkt, server_addr);
                LOG(INFO) << "Sent ACK for seq " << pkt.seq;
                expected_seq++;
            } else {
                // Send ACK for last received packet 接收完成，发送ACK
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = expected_seq - 1;
                sendPacket(sockfd, ack_pkt, server_addr);
                LOG(WARNING) << "Unexpected seq. Expected " << expected_seq
                             << ", but got " << pkt.seq;
            }
        } else if (n == 0) {
            // Timeout, continue waiting
            continue;
        } else if (pkt.type == FIN) {
            // Connection termination initiated by client
            LOG(INFO) << "Received FIN from client";
            // Send FIN-ACK
            Packet fin_ack_pkt;
            fin_ack_pkt.type = FIN_ACK;
            sendPacket(sockfd, fin_ack_pkt, server_addr);
            LOG(INFO) << "Sent FIN-ACK to client";
            break;
        }
     }


    close(sockfd);
    LOG(INFO) << "Connection closed";
    return 0;
}

