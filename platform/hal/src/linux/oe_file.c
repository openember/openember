#define _POSIX_C_SOURCE 200809L

#include "openember/hal/file.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    FILE *fp;
    int inited;
} oe_file_impl_t;

_Static_assert(sizeof(oe_file_impl_t) <= sizeof(oe_file_t), "oe_file_t opaque too small");

static oe_file_impl_t *impl(oe_file_t *f)
{
    return (oe_file_impl_t *)(void *)f;
}

oe_result_t oe_file_open(oe_file_t *f, const char *path, const char *mode)
{
    FILE *fp;

    if (!f || !path || !mode) {
        return OE_ERR_INVALID_ARG;
    }

    memset(f, 0, sizeof(*f));
    fp = fopen(path, mode);
    if (!fp) {
        return OE_ERR_INTERNAL;
    }
    impl(f)->fp = fp;
    impl(f)->inited = 1;
    return OE_OK;
}

oe_result_t oe_file_close(oe_file_t *f)
{
    int r;

    if (!f) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(f)->inited || !impl(f)->fp) {
        memset(f, 0, sizeof(*f));
        return OE_OK;
    }

    r = fclose(impl(f)->fp);
    memset(f, 0, sizeof(*f));
    if (r != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_file_read(oe_file_t *f, void *buf, size_t len, size_t *out_read)
{
    size_t n;

    if (!f || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(f)->inited || !impl(f)->fp) {
        return OE_ERR_INVALID_ARG;
    }

    if (len == 0) {
        if (out_read) {
            *out_read = 0;
        }
        return OE_OK;
    }

    n = fread(buf, 1, len, impl(f)->fp);
    if (out_read) {
        *out_read = n;
    }
    if (n == 0 && ferror(impl(f)->fp)) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_file_write(oe_file_t *f, const void *buf, size_t len, size_t *out_written)
{
    size_t n;

    if (!f || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(f)->inited || !impl(f)->fp) {
        return OE_ERR_INVALID_ARG;
    }

    if (len == 0) {
        if (out_written) {
            *out_written = 0;
        }
        return OE_OK;
    }

    n = fwrite(buf, 1, len, impl(f)->fp);
    if (out_written) {
        *out_written = n;
    }
    if (n != len) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_file_flush(oe_file_t *f)
{
    if (!f) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(f)->inited || !impl(f)->fp) {
        return OE_ERR_INVALID_ARG;
    }

    if (fflush(impl(f)->fp) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_file_seek(oe_file_t *f, int64_t offset, oe_file_whence_t whence,
                         int64_t *out_pos)
{
    int w;
    off_t pos;

    if (!f) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(f)->inited || !impl(f)->fp) {
        return OE_ERR_INVALID_ARG;
    }

    switch (whence) {
    case OE_FILE_SEEK_SET:
        w = SEEK_SET;
        break;
    case OE_FILE_SEEK_CUR:
        w = SEEK_CUR;
        break;
    case OE_FILE_SEEK_END:
        w = SEEK_END;
        break;
    default:
        return OE_ERR_INVALID_ARG;
    }

    if (fseeko(impl(f)->fp, (off_t)offset, w) != 0) {
        return OE_ERR_INTERNAL;
    }

    pos = ftello(impl(f)->fp);
    if (pos == (off_t)-1) {
        return OE_ERR_INTERNAL;
    }

    if (out_pos) {
        *out_pos = (int64_t)pos;
    }
    return OE_OK;
}
