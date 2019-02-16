#pragma once

#include <archive.h>

struct deb
{
    deb(const char *filename);
    ~deb();
    void check_deb_binary();
    void read_control();
    void read_control_inner();
    void read_data();
    void read_data_inner();

    const char *filename;
    struct archive *ar; // ar
    struct archive *ac; // control.tar.gz, data.tar.gz
    struct archive *aw; // file being extracted
    int fd;
    void* ar_mem;
    size_t len;
};
