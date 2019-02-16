#pragma once

#include <archive.h>

struct deb_ar
{
    deb_ar(const char *filename);
    ~deb_ar();
    void check_deb_binary();
    void read_control();
    void read_data();

    const char *filename;
    struct archive *arc;
    int fd;
    void* mem;
    size_t len;
};

struct deb_control
{
    deb_control(deb_ar *ar);
    ~deb_control();

private:
    deb_ar *ar;
    struct archive *arc;
};

struct deb_data
{
    deb_data(deb_ar *ar);
    ~deb_data();

private:
    deb_ar *ar;
    struct archive *arc, *aw;
};
