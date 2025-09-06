#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

inline size_t file_size(int fd) {
    struct stat fileInfo;
    fstat(fd, &fileInfo);
    return fileInfo.st_size;
}

inline int file_append(const char* dst_path, const char* src_path) {
    int dst_fd = open(dst_path, O_RDWR | O_CREAT, 0666);
    if (dst_fd < 0) {
        perror("open dst");
        return -1;
    }

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        perror("open src");
        close(dst_fd);
        return -1;
    }

    auto old_size = file_size(dst_fd);
    auto append_size = file_size(src_fd);

    if (ftruncate(dst_fd, old_size + append_size) < 0) {
        perror("ftrucate");
        close(dst_fd);
        close(src_fd);
        return -1;
    }

    void* dst_map = mmap(nullptr, old_size + append_size, PROT_READ | PROT_WRITE, MAP_SHARED, dst_fd, 0);
    if (dst_map == MAP_FAILED) {
        perror("mmap dst");
        close(dst_fd);
        close(src_fd);
        return -1;
    }

    void* src_map = mmap(nullptr, append_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
    if (src_map == MAP_FAILED) {
        perror("mmap src");
        munmap(dst_map, old_size + append_size);
        close(dst_fd);
        close(src_fd);
        return -1;
    }

    memcpy((char*)dst_map + old_size, src_map, append_size);

    if (msync((char*)dst_map,old_size + append_size, MS_SYNC) < 0) {
        perror("msync");
    }

    munmap(src_map, append_size);
    munmap(dst_map, old_size + append_size);
    close(src_fd);
    close(dst_fd);
    return 0;
}
