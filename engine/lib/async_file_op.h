#pragma once

#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

inline int waitForEvent(int fd, uint32_t events, int timeout_ms) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        return -1;
    }

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(epoll_fd);
        return -1;
    }

    struct epoll_event ev_out;
    int ret = epoll_wait(epoll_fd, &ev_out, 1, timeout_ms);
    close(epoll_fd);
    return ret;
}
