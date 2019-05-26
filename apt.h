#pragma once
#include <plf_colony.h>

plf::colony<std::string> apt_sim(const plf::colony<const char*> &goals);
bool find_deb(int dir, size_t len, const char *pav, int *fd);