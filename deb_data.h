#pragma once

#include "deb_ar.h"

struct deb_data
{
    deb_data(deb_ar *ar);
    ~deb_data();

private:
    deb_ar *ar;
    struct archive *arc, *aw;
};
