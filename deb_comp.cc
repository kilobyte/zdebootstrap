#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "deb_comp.h"

#include <stdio.h>
#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)

static la_ssize_t comp_read(struct archive *arc, void *c_data, const void **buf)
{
    deb_ar *ar = static_cast<deb_ar*>(c_data);

    const void *ibuf;
    size_t len;
    off_t offset;
    int err = archive_read_data_block(ar->arc, &ibuf, &len, &offset);
    if (err > ARCHIVE_EOF)
    {
        ERR("can't read deb control from '%s': %s\n", ar->filename,
            archive_error_string(arc));
    }

    *buf = ibuf;
    return len;
}

deb_comp::deb_comp(deb_ar *parent_ar)
{
    ar = parent_ar;

    if (!(arc = archive_read_new()))
        ERR("can't create a new libarchive object: %s\n", archive_error_string(arc));

    archive_read_support_filter_gzip(arc);
    archive_read_support_filter_bzip2(arc);
    archive_read_support_filter_lzma(arc);
    archive_read_support_filter_xz(arc);
    archive_read_support_format_tar(arc);

    if (archive_read_open(arc, ar, 0, comp_read, 0))
    {
        ERR("can't open deb control: '%s: %s'\n", ar->filename,
            archive_error_string(arc));
    }

    struct archive_entry *ent;
    int err;
    while (!(err = archive_read_next_header(arc, &ent)))
    {
        printf("%s\n", archive_entry_pathname(ent));
        archive_read_data_skip(arc);
    }

    if (err != ARCHIVE_EOF)
    {
        ERR("can't list deb control: '%s': %s\n", ar->filename,
            archive_error_string(arc));
    }
}

deb_comp::~deb_comp()
{
    archive_read_free(arc);
}
