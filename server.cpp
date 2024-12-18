// server.cpp
#include "rudp.h"
#include <fstream>

// 这是服务端的实现，为了方便，这里没有考虑多客户机的情况。
// 如果要使用多客户机，可以添加pthread
// 在接收到SYN之后，转到新的线程去处理这个客户机即可
// 主要是懒得搞了，反正效果差不多
int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    // 日志配置
    FLAGS_log_dir = "./logs";       // 日志保存目录
    FLAGS_logtostderr = 1;          // 日志输出到 stderr
    FLAGS_minloglevel = 0;          // 日志级别: INFO 及以上
    FLAGS_v = 2;                    // 设置详细级别

    if (argc != 3) {
        LOG(ERROR) << "Usage: ./server <port> <filename>";
        return -1;
    }

    int port = atoi(argv[1]);
    std::string filename = argv[2];

    int sockfd;
    sockaddr_in server_addr{}, client_addr{};

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation failed";
        return -1;
    }

    // Bind the socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 这里有可能端口已经被占用，先判断一下
    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG(ERROR) << "Bind failed";
        close(sockfd);
        return -1;
    }

    // Waiting for connection
    LOG(INFO) << "Server listening on port " << port;

    // Connection establishment (三次握手)
    Packet pkt;
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);

	// 收到建立连接的请求
        if (n > 0 && pkt.type == SYN) {
            LOG(INFO) << "Received SYN from client";
            // Send SYN-ACK
            Packet syn_ack_pkt;
            syn_ack_pkt.type = SYN_ACK;
            syn_ack_pkt.seq = pkt.seq + 1;
            sendPacket(sockfd, syn_ack_pkt, client_addr);
            LOG(INFO) << "Sent SYN-ACK to client";

            // Wait for ACK
            n = recvPacket(sockfd, pkt, client_addr);
            if (n > 0 && pkt.type == ACK) {
                LOG(INFO) << "Received ACK from client";
                break; // Connection established
            }
        }
    }

    // Receive file from client
    std::ofstream outfile("received_from_client_" + filename, std::ios::binary);
    uint32_t expected_seq = 0; // 期望收到的数据报数量
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);
        if (n > 0 && pkt.type == DATA) {
            if (pkt.seq == expected_seq) {
                outfile.write(pkt.data, n - HEADER_SIZE);
                LOG(INFO) << "Received data packet with seq " << pkt.seq;
                // Send DATA_ACK 表示收到数据报
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = pkt.seq;
                sendPacket(sockfd, ack_pkt, client_addr);
                LOG(INFO) << "Sent ACK for seq " << pkt.seq;
                expected_seq++;
            } else {
                // Send ACK for last received packet 接收完成，发送ACK
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = expected_seq - 1;
                sendPacket(sockfd, ack_pkt, client_addr);
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
            sendPacket(sockfd, fin_ack_pkt, client_addr);
            LOG(INFO) << "Sent FIN-ACK to client";
            break;
        }
    }

    // 接收文件完成，下面要发文件给客户端试试
    outfile.close();

    // Send file to client
    std::ifstream infile(filename, std::ios::binary);

    // 打开失败
    if (!infile) {
        LOG(ERROR) << "Failed to open file " << filename;
        close(sockfd);
        return -1;
    }

    uint32_t seq_num = 0;
    while (true) {
        Packet data_pkt;
        data_pkt.type = DATA;
        data_pkt.seq = seq_num;
        infile.read(data_pkt.data, DATA_SIZE);
        std::streamsize bytes_read = infile.gcount();
        if (bytes_read > 0) {
            while (true) {
                sendPacket(sockfd, data_pkt, client_addr);
                LOG(INFO) << "Sent data packet with seq " << seq_num;
                // Wait for ACK
                ssize_t n = recvPacket(sockfd, pkt, client_addr);
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
        } else {
            // End of file
            infile.close();
            break;
        }
    }

    // Connection termination (四次挥手)
    Packet fin_pkt;
    fin_pkt.type = FIN;
    sendPacket(sockfd, fin_pkt, client_addr);
    LOG(INFO) << "Sent FIN to client";

    // Wait for FIN-ACK
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);
        if (n > 0 && pkt.type == FIN_ACK) {
            LOG(INFO) << "Received FIN-ACK from client";
            break;
        }
    }

    close(sockfd);
    LOG(INFO) << "Connection closed";
    return 0;
}

