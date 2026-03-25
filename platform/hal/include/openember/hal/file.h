/* OpenEmber HAL — 存储：文件（C ABI） */
#ifndef OPENEMBER_HAL_FILE_H_
#define OPENEMBER_HAL_FILE_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 不透明句柄，内嵌 FILE*（Linux 实现） */
typedef struct oe_file {
    uint8_t opaque[128];
} oe_file_t;

/* Capability flags */
typedef enum oe_file_caps_flag {
    OE_FILE_CAPS_CAN_READ = 1u << 0,
    OE_FILE_CAPS_CAN_WRITE = 1u << 1,
    OE_FILE_CAPS_CAN_SEEK = 1u << 2,
    OE_FILE_CAPS_CAN_FLUSH = 1u << 3,
} oe_file_caps_flag_t;

typedef struct oe_file_caps {
    uint32_t flags;          /* bitmask of oe_file_caps_flag_t */
    size_t max_read_bytes;  /* 0 means unknown / unlimited */
    size_t max_write_bytes; /* 0 means unknown / unlimited */
} oe_file_caps_t;

typedef enum oe_file_whence {
    OE_FILE_SEEK_SET = 0,
    OE_FILE_SEEK_CUR = 1,
    OE_FILE_SEEK_END = 2,
} oe_file_whence_t;

/** mode：与 fopen 一致，如 "r"、"w"、"a"、"rb" */
oe_result_t oe_file_open(oe_file_t *f, const char *path, const char *mode);
oe_result_t oe_file_close(oe_file_t *f);

/** 成功时 *out_read 为实际读取字节数；EOF 且未读数据时为 0 且 OE_OK */
oe_result_t oe_file_read(oe_file_t *f, void *buf, size_t len, size_t *out_read);
oe_result_t oe_file_write(oe_file_t *f, const void *buf, size_t len, size_t *out_written);

oe_result_t oe_file_flush(oe_file_t *f);

oe_result_t oe_file_query_caps(oe_file_caps_t *out_caps);

/** 与 fseeko 语义一致；成功时可选返回当前位置 */
oe_result_t oe_file_seek(oe_file_t *f, int64_t offset, oe_file_whence_t whence,
                         int64_t *out_pos);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_FILE_H_ */
