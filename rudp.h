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
const int HEADER_SIZE = 12; // type (4 bytes) + seq (4 bytes) + checksum (4 bytes)
const int DATA_SIZE = MAX_BUFFER_SIZE - HEADER_SIZE;

// Message Types
enum MessageType {
    SYN = 1,
    SYN_ACK,
    ACK,
    DATA,
    DATA_ACK,
    FIN,
    FIN_ACK
};

// Packet Structure
struct Packet {
    uint32_t type;
    uint32_t seq;
    uint32_t checksum;
    char data[DATA_SIZE];

    Packet() : type(0), seq(0), checksum(0) {
        memset(data, 0, DATA_SIZE);
    }
};

/**
 * @brief 计算校验和
 *  这里实现一个最简单的校验和计算方法，即将所有字段相加。
 * @param pkt  要计算校验和的数据包
 * @return uint32_t  返回计算得到的校验和
 */
uint32_t calculateChecksum(Packet& pkt) {
    uint32_t sum = pkt.type + pkt.seq;
    for (int i = 0; i < DATA_SIZE; ++i) {
        sum += static_cast<uint8_t>(pkt.data[i]);
    }
    return sum;
}

// Function to send a packet
ssize_t sendPacket(int sockfd, const Packet& pkt, const sockaddr_in& addr) {
    Packet send_pkt = pkt;
    send_pkt.checksum = calculateChecksum(send_pkt);
    ssize_t bytes_sent = sendto(sockfd, &send_pkt, sizeof(send_pkt), 0,
                                (const struct sockaddr*)&addr, sizeof(addr));
    return bytes_sent;
}

// Function to receive a packet with timeout
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

// Function for server to accept connection (Three-way handshake)
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

// Function for client to connect to server (Three-way handshake)
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

// Function to send data
ssize_t rudp_send_data(int sockfd, const char* data, size_t length, const sockaddr_in& addr) {
    uint32_t seq_num = 0;
    Packet data_pkt;
    data_pkt.type = DATA;
    data_pkt.seq = seq_num;
    memset(data_pkt.data, 0, DATA_SIZE);
    // Copy data into packet data field
    size_t data_length = (length < DATA_SIZE) ? length : DATA_SIZE;
    memcpy(data_pkt.data, data, data_length);

    while (true) {
        sendPacket(sockfd, data_pkt, addr);
        LOG(INFO) << "Sent data packet with seq " << seq_num;
        // Wait for ACK
        Packet pkt;
        ssize_t n = recvPacket(sockfd, pkt, const_cast<sockaddr_in&>(addr));
        if (n > 0 && pkt.type == DATA_ACK && pkt.seq == seq_num) {
            LOG(INFO) << "Received ACK for seq " << seq_num;
            seq_num++;
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

// Function to receive data
ssize_t rudp_receive_data(int sockfd, char* buffer, size_t max_length, sockaddr_in& addr) {
    uint32_t expected_seq = 0;
    while (true) {
        Packet pkt;
        ssize_t n = recvPacket(sockfd, pkt, addr);
        if (n > 0 && pkt.type == DATA) {
            if (pkt.seq == expected_seq) {
                LOG(INFO) << "Received data packet with seq " << pkt.seq;
                // Send DATA_ACK
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = pkt.seq;
                sendPacket(sockfd, ack_pkt, addr);
                LOG(INFO) << "Sent ACK for seq " << pkt.seq;
                // Copy data to buffer
                size_t data_length = (max_length < DATA_SIZE) ? max_length : DATA_SIZE;
                memcpy(buffer, pkt.data, data_length);
                expected_seq++;
                return data_length;
            } else {
                // Send ACK for last received packet
                Packet ack_pkt;
                ack_pkt.type = DATA_ACK;
                ack_pkt.seq = expected_seq - 1;
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

// Function to close connection (Four-way handshake)
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

// Function to wait for close request and respond (Four-way handshake)
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
