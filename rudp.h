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

// Function to calculate checksum
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
        perror("select"); // error occurred
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

#endif // RUDP_H

