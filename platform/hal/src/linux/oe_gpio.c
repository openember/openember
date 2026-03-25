#define _POSIX_C_SOURCE 200809L

#include "openember/hal/gpio.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    uint32_t gpio_num;
    int inited;
    int exported_by_us;
    int dir_fd;
    int value_fd;
} oe_gpio_impl_t;

_Static_assert(sizeof(oe_gpio_impl_t) <= sizeof(oe_gpio_t), "oe_gpio_t opaque too small");

static oe_gpio_impl_t *impl(oe_gpio_t *g)
{
    return (oe_gpio_impl_t *)(void *)g;
}

static oe_result_t sysfs_export(uint32_t gpio_num)
{
    int fd;
    char buf[32];
    ssize_t n;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        return OE_ERR_UNSUPPORTED;
    }

    n = snprintf(buf, sizeof(buf), "%u", gpio_num);
    if (n < 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    if (write(fd, buf, (size_t)n) < 0) {
        if (errno == EBUSY || errno == EINVAL) {
            /* Already exported / invalid gpio for this device */
            close(fd);
            return OE_OK;
        }
        close(fd);
        return OE_ERR_INTERNAL;
    }

    close(fd);
    return OE_OK;
}

static oe_result_t sysfs_try_open_paths(uint32_t gpio_num, int *out_dir_fd, int *out_value_fd)
{
    char dir_path[128];
    char value_path[128];
    int dir_fd = -1;
    int value_fd = -1;

    snprintf(dir_path, sizeof(dir_path), "/sys/class/gpio/gpio%u/direction", gpio_num);
    snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%u/value", gpio_num);

    dir_fd = open(dir_path, O_RDWR);
    if (dir_fd < 0) {
        return OE_ERR_INTERNAL;
    }
    value_fd = open(value_path, O_RDWR);
    if (value_fd < 0) {
        close(dir_fd);
        return OE_ERR_INTERNAL;
    }

    *out_dir_fd = dir_fd;
    *out_value_fd = value_fd;
    return OE_OK;
}

oe_result_t oe_gpio_query_caps(oe_gpio_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->direction_mask = OE_GPIO_CAPS_DIR_IN | OE_GPIO_CAPS_DIR_OUT;
    return OE_OK;
}

oe_result_t oe_gpio_open(oe_gpio_t *g, uint32_t gpio_num)
{
    oe_result_t r;
    int dir_fd = -1;
    int value_fd = -1;

    if (!g) {
        return OE_ERR_INVALID_ARG;
    }

    memset(g, 0, sizeof(*g));
    impl(g)->gpio_num = gpio_num;

    /* Try open existing sysfs first. */
    r = sysfs_try_open_paths(gpio_num, &dir_fd, &value_fd);
    if (r != OE_OK) {
        r = sysfs_export(gpio_num);
        if (r != OE_OK) {
            return r;
        }
        r = sysfs_try_open_paths(gpio_num, &dir_fd, &value_fd);
        if (r != OE_OK) {
            return r;
        }
        impl(g)->exported_by_us = 1;
    }

    impl(g)->dir_fd = dir_fd;
    impl(g)->value_fd = value_fd;
    impl(g)->inited = 1;
    return OE_OK;
}

static oe_result_t write_string_fd(int fd, const char *s)
{
    size_t len;
    if (lseek(fd, 0, SEEK_SET) < 0) {
        return OE_ERR_INTERNAL;
    }
    len = strlen(s);
    if (write(fd, s, len) != (ssize_t)len) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_gpio_set_direction(oe_gpio_t *g, oe_gpio_direction_t dir)
{
    oe_gpio_impl_t *i;

    if (!g) {
        return OE_ERR_INVALID_ARG;
    }
    i = impl(g);
    if (!i->inited || i->dir_fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    switch (dir) {
    case OE_GPIO_DIR_IN:
        return write_string_fd(i->dir_fd, "in");
    case OE_GPIO_DIR_OUT:
        return write_string_fd(i->dir_fd, "out");
    default:
        return OE_ERR_INVALID_ARG;
    }
}

oe_result_t oe_gpio_read(oe_gpio_t *g, uint8_t *out_value)
{
    char ch;
    ssize_t n;
    oe_gpio_impl_t *i;

    if (!g || !out_value) {
        return OE_ERR_INVALID_ARG;
    }

    i = impl(g);
    if (!i->inited || i->value_fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    if (lseek(i->value_fd, 0, SEEK_SET) < 0) {
        return OE_ERR_INTERNAL;
    }
    n = read(i->value_fd, &ch, 1);
    if (n <= 0) {
        return OE_ERR_INTERNAL;
    }

    *out_value = (ch == '0') ? 0 : 1;
    return OE_OK;
}

oe_result_t oe_gpio_write(oe_gpio_t *g, uint8_t value)
{
    oe_gpio_impl_t *i;
    char ch = (value == 0) ? '0' : '1';

    if (!g) {
        return OE_ERR_INVALID_ARG;
    }

    i = impl(g);
    if (!i->inited || i->value_fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    if (lseek(i->value_fd, 0, SEEK_SET) < 0) {
        return OE_ERR_INTERNAL;
    }

    if (write(i->value_fd, &ch, 1) != 1) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_gpio_close(oe_gpio_t *g)
{
    oe_gpio_impl_t *i;
    int dir_fd;
    int value_fd;

    if (!g) {
        return OE_ERR_INVALID_ARG;
    }

    i = impl(g);
    dir_fd = i->dir_fd;
    value_fd = i->value_fd;
    memset(g, 0, sizeof(*g));

    if (dir_fd >= 0) {
        close(dir_fd);
    }
    if (value_fd >= 0) {
        close(value_fd);
    }
    return OE_OK;
}

