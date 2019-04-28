#pragma once

#include <vector>
#include <string>
#include <archive.h>
#include <string>
#include "822.h"

struct deb
{
    deb(const char *filename);
    ~deb();
    void unpack();

    void open_file();
    void check_deb_binary();
    void read_control();
    void read_control_inner();
    void slurp_control_file();
    void read_data();
    void read_data_inner();
    void write_list();

    const char *filename;
    std::string basename;
    struct archive *ar; // ar
    struct archive *ac; // control.tar.gz, data.tar.gz
    struct archive *aw; // file being extracted
    void* ar_mem;
    size_t len;

    std::vector<std::string> contents;
    deb822 control;
};
