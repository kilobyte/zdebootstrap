#pragma once

#include "deb_ar.h"

struct deb_comp
{
    deb_comp(deb_ar *ar);
    ~deb_comp();

private:
    deb_ar *ar;
    struct archive *arc;
};
