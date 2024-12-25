// rudp.h
#ifndef RUDP_H
#define RUDP_H

#include <arpa/inet.h>
#include <cstring>
#include <glog/logging.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Constants
const int MAX_BUFFER_SIZE = 1024;
const int HEADER_SIZE = 16; // type (4 bytes) + seq (4 bytes) + checksum (4 bytes) + data_length (4 bytes)
const int DATA_SIZE = MAX_BUFFER_SIZE - HEADER_SIZE;

// Message Types
enum MessageType {
    SYN = 1, // 握手请求
    SYN_ACK, // 握手应答
    ACK, // 确认应答
    DATA, // 数据包
    DATA_ACK, // 数据包应答
    FIN, // 关闭请求
    FIN_ACK // 关闭应答
};

/**
 * @brief  数据包结构
 *  这里全部使用无符号整型，并且指定大小，以保证在不同平台上的一致性。
 */
struct Packet {
    uint32_t type;
    uint32_t seq;
    uint32_t checksum;
    uint32_t data_length;  //  记录实际数据长度
    char data[DATA_SIZE];

    Packet() : type(0), seq(0), checksum(0), data_length(0) {
        memset(data, 0, DATA_SIZE); // 将 data 字段初始化为 0
    }
};

/**
 * @brief 计算校验和
 *  这里实现一个最简单的校验和计算方法，即将所有字段相加。
 * @param pkt  要计算校验和的数据包
 * @return uint32_t  返回计算得到的校验和
 */
uint32_t calculateChecksum(Packet& pkt) {
    uint32_t sum = pkt.type + pkt.seq + pkt.data_length;
    for (uint32_t i = 0; i < pkt.data_length; ++i) {
        sum += static_cast<uint8_t>(pkt.data[i]);
    }
    return sum;
}

/**
 * @brief  发送数据包
 *  发送数据包时，需要计算校验和，并将校验和填充到数据包中。
 * @param sockfd  socket 文件描述符
 * @param pkt  要发送的数据包
 * @param addr  目标地址
 * @return ssize_t  返回发送的字节数
 */
ssize_t sendPacket(int sockfd, const Packet& pkt, const sockaddr_in& addr) {
    Packet send_pkt = pkt;
    // 确保数据包的长度正确
    send_pkt.checksum = 0;  // Reset checksum before calculation
    send_pkt.checksum = calculateChecksum(send_pkt);
    ssize_t bytes_sent = sendto(sockfd, &send_pkt, sizeof(send_pkt), 0,
                                (const struct sockaddr*)&addr, sizeof(addr));
    return bytes_sent;
}


/**
 * @brief  接收数据包
 *  接收数据包时，需要验证校验和，如果校验和不匹配，则返回 -1。
 * @param sockfd  socket 文件描述符
 * @param pkt  接收到的数据包
 * @param addr  发送方地址
 * @param timeout_sec  超时时间（秒）
 * @return ssize_t  返回接收的字节数
 */
ssize_t recvPacket(int sockfd, Packet& pkt, sockaddr_in& addr, int timeout_sec = 1) {
    socklen_t addr_len = sizeof(addr);
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    int rv = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (rv == -1) {
        perror("select"); // Error occurred
        return -1;
    } else if (rv == 0) {
        // Timeout occurred
        return 0;
    } else {
        ssize_t bytes_received = recvfrom(sockfd, &pkt, sizeof(pkt), 0,
                                          (struct sockaddr*)&addr, &addr_len);
        uint32_t received_checksum = pkt.checksum;
        pkt.checksum = 0;
        uint32_t calculated_checksum = calculateChecksum(pkt);
        if (received_checksum != calculated_checksum) {
            LOG(WARNING) << "Checksum mismatch!";
            return -1; // Indicate checksum error
        }
        return bytes_received;
    }
}

/*
    上面是基本的数据包发送和接收函数，下面是连接建立、数据传输和连接关闭的函数。
    服务端和客户端并不是对等的，所以上面俩可以通用，但是下面的握手和挥手都需要单独实现。
*/

/**
 * @brief  服务器接受连接请求（三次握手）
 *  服务器接受连接请求，需要接收 SYN 数据包，然后发送 SYN-ACK 数据包，最后接收 ACK 数据包。
 * @param sockfd  socket 文件描述符
 * @param client_addr  客户端地址
 * @return int  返回 0 表示连接建立成功，返回 -1 表示连接建立失败
 */
int rudp_accept(int sockfd, sockaddr_in& client_addr) {
    Packet pkt;
    while (true) {
        ssize_t n = recvPacket(sockfd, pkt, client_addr);

        // Received SYN from client
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
                return 0; // Connection established
            }
        } else if (n == 0) {
            // Timeout, continue waiting
            continue;
        } else {
            // Error or unexpected packet
            continue;
        }
    }
    return -1; // Should not reach here
}

/**
 * @brief  客户端连接服务器（三次握手）
 *  客户端连接服务器，需要发送 SYN 数据包，然后接收 SYN-ACK 数据包，最后发送 ACK 数据包。
 * @param sockfd  socket 文件描述符
 * @param server_addr  服务器地址
 * @return int  返回 0 表示连接建立成功，返回 -1 表示连接建立失败
 */
int rudp_connect(int sockfd, sockaddr_in& server_addr) {
    Packet pkt;
    Packet recv_pkt;

    // Send SYN
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
            return 0; // Connection established
        } else if (n == 0) {
            // Timeout, resend SYN
            sendPacket(sockfd, pkt, server_addr);
            LOG(WARNING) << "Timeout, resending SYN";
            continue;
        } else {
            // Error or unexpected packet
            continue;
        }
    }
    return -1; // Should not reach here
}

/**
 * @brief  发送数据
 *  发送数据时，需要等待 ACK 数据包，如果超时或者接收到错误的 ACK 数据包，则重发数据。
 * @param sockfd  socket 文件描述符
 * @param data  要发送的数据
 * @param length  数据长度
 * @param addr      目标地址
 * @return ssize_t  返回发送的字节数
 */
ssize_t rudp_send_data(int sockfd, const char* data, size_t length, const sockaddr_in& addr, uint32_t& seq_num) {
    Packet data_pkt;
    data_pkt.type = DATA;
    data_pkt.seq = seq_num;
    // Copy data into packet data field
    size_t data_length = (length < DATA_SIZE) ? length : DATA_SIZE;
    memcpy(data_pkt.data, data, data_length);
    data_pkt.data_length = data_length;  // Set the actual length of data
    data_pkt.checksum = 0;  // Ensure checksum is reset

    while (true) {
        sendPacket(sockfd, data_pkt, addr);
        LOG(INFO) << "Sent data packet with seq " << seq_num << " and length " << data_length;
        // Wait for ACK
        Packet pkt;
        ssize_t n = recvPacket(sockfd, pkt, const_cast<sockaddr_in&>(addr));
        if (n > 0 && pkt.type == DATA_ACK && pkt.seq == seq_num) {
            LOG(INFO) << "Received ACK for seq " << seq_num;
            seq_num = (seq_num + 1) % 2;  // 根据停等协议，在收到 ACK 后切换序号
            return data_length;
        } else if (n == 0) {
            // Timeout, resend data
            LOG(WARNING) << "Timeout, resending data packet";
            continue;
        } else {
            LOG(WARNING) << "No ACK or wrong ACK received, resending packet";
            // Resend the packet
            continue;
        }
    }
    return -1; // Should not reach here
}

/**
 * @brief  接收数据
 *  接收数据时，需要等待数据包，然后发送 ACK 数据包。
 * @param sockfd  socket 文件描述符
 * @param buffer  接收数据的缓冲区
 * @param max_length  缓冲区最大长度
 * @param addr      发送方地址
 * @return ssize_t  返回接收的字节数
 */
ssize_t rudp_receive_data(int sockfd, char* buffer, size_t max_length, sockaddr_in& addr, uint32_t& expected_seq) {
    while (true) {
        Packet pkt;
        ssize_t n = recvPacket(sockfd, pkt, addr);
        if (n > 0 && pkt.type == DATA) {
            if (pkt.seq == expected_seq) {
                LOG(INFO) << "Received data packet with seq " << pkt.seq << " and length " << pkt.data_length;
                // Send DATA_ACK
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = pkt.seq;
                sendPacket(sockfd, ack_pkt, addr);
                LOG(INFO) << "Sent ACK for seq " << pkt.seq;
                // Copy data to buffer
                size_t data_length = (pkt.data_length < max_length) ? pkt.data_length : max_length;
                memcpy(buffer, pkt.data, data_length);
                expected_seq = (expected_seq + 1) % 2;  // Alternate expected sequence number
                return data_length;
            } else {
                // Send ACK for last received packet
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = (expected_seq + 1) % 2;
                sendPacket(sockfd, ack_pkt, addr);
                LOG(WARNING) << "Unexpected seq. Expected " << expected_seq
                             << ", but got " << pkt.seq;
            }
        } else if (n == 0) {
            // Timeout, continue waiting
            continue;
        } else {
            // Error or unexpected packet, continue waiting
            continue;
        }
    }
    return -1; // Should not reach here
}

/**
 * @brief 关闭连接（四次挥手）
 *  关闭连接时，需要发送 FIN 数据包，然后等待 FIN-ACK 数据包。
 * @param sockfd  socket 文件描述符
 * @param addr  目标地址
 * @return int  返回 0 表示连接关闭成功，返回 -1 表示连接关闭失败
 */
int rudp_close_connection(int sockfd, sockaddr_in& addr) {
    // Send FIN
    Packet fin_pkt;
    fin_pkt.type = FIN;
    sendPacket(sockfd, fin_pkt, addr);
    LOG(INFO) << "Sent FIN";

    // Wait for FIN-ACK
    while (true) {
        Packet pkt;
        ssize_t n = recvPacket(sockfd, pkt, addr);
        if (n > 0 && pkt.type == FIN_ACK) {
            LOG(INFO) << "Received FIN-ACK";
            return 0; // Connection closed
        } else if (n == 0) {
            // Timeout, resend FIN
            sendPacket(sockfd, fin_pkt, addr);
            LOG(WARNING) << "Timeout, resending FIN";
            continue;
        } else {
            // Error or unexpected packet
            continue;
        }
    }
    return -1; // Should not reach here
}

/**
 * @brief  等待关闭连接（四次挥手）
 *  等待关闭连接时，需要等待 FIN 数据包，然后发送 FIN-ACK 数据包。
 * @param sockfd  socket 文件描述符
 * @param addr  发送方地址
 * @return int  返回 0 表示连接关闭成功，返回 -1 表示连接关闭失败
 */
int rudp_wait_close(int sockfd, sockaddr_in& addr) {
    while (true) {
        Packet pkt;
        ssize_t n = recvPacket(sockfd, pkt, addr);
        if (n > 0 && pkt.type == FIN) {
            LOG(INFO) << "Received FIN";
            // Send FIN-ACK
            Packet fin_ack_pkt;
            fin_ack_pkt.type = FIN_ACK;
            sendPacket(sockfd, fin_ack_pkt, addr);
            LOG(INFO) << "Sent FIN-ACK";
            return 0; // Connection closed
        } else if (n == 0) {
            // Timeout, continue waiting
            continue;
        } else {
            // Error or unexpected packet
            continue;
        }
    }
    return -1; // Should not reach here
}

#endif // RUDP_H
