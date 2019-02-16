#pragma once

#include <archive.h>

struct deb
{
    deb(const char *filename);
    ~deb();
    void check_deb_binary();
    void read_control();
    void read_data();

    const char *filename;
    struct archive *ar;
    int fd;
    void* ar_mem;
    size_t len;
};

struct deb_control
{
    deb_control(deb *ar);
    ~deb_control();

private:
    deb *ar;
    struct archive *arc;
};

struct deb_data
{
    deb_data(deb *ar);
    ~deb_data();

private:
    deb *ar;
    struct archive *arc, *aw;
};
