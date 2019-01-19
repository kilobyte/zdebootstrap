#pragma once

#include <archive.h>

struct deb_ar
{
    deb_ar(const char *filename);
    ~deb_ar();
    void check_deb_binary();
    void read_control();

    const char *filename;
    struct archive *arc;
    int fd;
    void* mem;
    size_t len;
};
