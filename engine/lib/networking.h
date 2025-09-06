#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>

// TODO: make this buffer in userspace
inline void recv_looping_crashing(int sock, char* buf, size_t n)  {
    if(recv(sock, buf, n, MSG_WAITALL) == -1)
        throw std::runtime_error("unable to receive from network");
}

// TODO:: make this buffer in userspace
inline void send_looping_crashing(int sock, const char* mesg, size_t n) {
    size_t sent_total = 0;
    while(sent_total != n) {
        size_t sent = send(sock, mesg + sent_total, n - sent_total, 0);
        if(sent == -1) 
            throw std::runtime_error("unable to send to network");
        else
            sent_total += sent;
    }
}

#endif /* NETWORKING_H */