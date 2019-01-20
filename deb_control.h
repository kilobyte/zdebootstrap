#pragma once

#include "deb_ar.h"

struct deb_control
{
    deb_control(deb_ar *ar);
    ~deb_control();

private:
    deb_ar *ar;
    struct archive *arc;
};
