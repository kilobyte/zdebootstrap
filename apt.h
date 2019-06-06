#pragma once
#include <plf_colony.h>

plf::colony<std::string> apt_sim(const plf::colony<const char*> &goals);
bool find_deb(int dir, size_t len, const char *pav, int *fd);
void apt_avail(void);
void apt_download();
size_t deb_avail_size(const char *pav);
