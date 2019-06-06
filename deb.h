#pragma once

#include <plf_colony.h>
#include <archive.h>
#include <string>
#include "822.h"

struct control_info
{
    control_info(std::string _filename, bool _x, time_t sec, long nsec,
        std::string _contents) :
        filename(_filename), x(_x), mtime({sec, nsec}), contents(_contents) {};
    std::string filename;
    bool x;
    struct timespec mtime;
    std::string contents;
};

struct deb
{
    deb(const char *filename);
    ~deb();
    void unpack();

    void open_file(int fd=-1);
    void check_deb_binary();
    void read_control();
    void read_control_inner();
    void slurp_control_file();
    void slurp_control_info(const char *name, bool x, time_t sec, long nsec);
    void read_data();
    void read_data_inner();
    void write_list();
    void write_info();
    const std::string& field(const std::string& name);
    const std::string& field(const std::string& name, const std::string& none);

    const char *filename;
    std::string basename;
    struct archive *ar; // ar
    struct archive *ac; // control.tar.gz, data.tar.gz
    struct archive *aw; // file being extracted
    char *ar_mem, *c_mem, *d_mem;
    size_t len;

    plf::colony<std::string> contents;
    deb822 control;
    plf::colony<control_info> info;
};
