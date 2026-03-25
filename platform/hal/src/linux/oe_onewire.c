#define _GNU_SOURCE

#include "openember/hal/onewire.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
    char w1_slave_path[256];
    int inited;
} oe_onewire_impl_t;

_Static_assert(sizeof(oe_onewire_impl_t) <= sizeof(((oe_onewire_t *)0)->opaque),
               "oe_onewire_t opaque too small");

static oe_onewire_impl_t *impl(oe_onewire_t *w)
{
    return (oe_onewire_impl_t *)(void *)w->opaque;
}

oe_result_t oe_onewire_query_caps(oe_onewire_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_temperature = 1;
    out_caps->supports_raw_read = 1;
    return OE_OK;
}

oe_result_t oe_onewire_open(oe_onewire_t *w, const char *w1_slave_path)
{
    oe_onewire_impl_t *p;

    if (!w || !w1_slave_path) {
        return OE_ERR_INVALID_ARG;
    }

    memset(w, 0, sizeof(*w));
    p = impl(w);
    strncpy(p->w1_slave_path, w1_slave_path, sizeof(p->w1_slave_path) - 1);
    p->inited = 1;

    /* Optional existence check */
    if (access(p->w1_slave_path, R_OK) != 0) {
        return OE_ERR_UNSUPPORTED;
    }
    return OE_OK;
}

oe_result_t oe_onewire_close(oe_onewire_t *w)
{
    oe_onewire_impl_t *p;

    if (!w) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(w);
    memset(w, 0, sizeof(*w));
    return OE_OK;
}

static oe_result_t read_all_text(const char *path, char *out_buf, size_t cap, size_t *out_len)
{
    int fd;
    ssize_t n;
    size_t total = 0;

    if (!path || !out_buf || cap == 0) {
        return OE_ERR_INVALID_ARG;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return OE_ERR_UNSUPPORTED;
    }

    while (total + 1 < cap) {
        n = read(fd, out_buf + total, cap - 1 - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return OE_ERR_INTERNAL;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }

    out_buf[total] = '\0';
    if (out_len) {
        *out_len = total;
    }
    close(fd);
    return OE_OK;
}

oe_result_t oe_onewire_read_raw(oe_onewire_t *w, char *out_buf, size_t cap, size_t *out_len)
{
    oe_onewire_impl_t *p;

    if (!w || !out_buf || cap == 0) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(w);
    if (!p->inited || p->w1_slave_path[0] == '\0') {
        return OE_ERR_INVALID_ARG;
    }

    return read_all_text(p->w1_slave_path, out_buf, cap, out_len);
}

static oe_result_t parse_temperature(const char *text, oe_onewire_temp_t *out_temp)
{
    const char *yes_pos = strstr(text, "YES");
    const char *no_pos = strstr(text, "NO");
    const char *t_pos = strstr(text, "t=");
    char *endptr = NULL;
    long t_milli = 0;

    if (!text || !out_temp) {
        return OE_ERR_INVALID_ARG;
    }

    memset(out_temp, 0, sizeof(*out_temp));

    if (yes_pos && (!no_pos || yes_pos < no_pos)) {
        out_temp->crc_ok = 1;
    } else {
        out_temp->crc_ok = 0;
    }

    if (!t_pos) {
        return OE_ERR_INTERNAL;
    }

    t_pos += 2; /* skip "t=" */
    t_milli = strtol(t_pos, &endptr, 10);
    if (endptr == t_pos) {
        return OE_ERR_INTERNAL;
    }

    out_temp->temperature_milli_c = (int32_t)t_milli;
    out_temp->temperature_c = ((float)t_milli) / 1000.0f;
    return OE_OK;
}

oe_result_t oe_onewire_read_temperature(oe_onewire_t *w, oe_onewire_temp_t *out_temp)
{
    char buf[256];
    size_t len = 0;
    oe_result_t r;

    if (!w || !out_temp) {
        return OE_ERR_INVALID_ARG;
    }

    r = oe_onewire_read_raw(w, buf, sizeof(buf), &len);
    if (r != OE_OK) {
        return r;
    }

    return parse_temperature(buf, out_temp);
}

