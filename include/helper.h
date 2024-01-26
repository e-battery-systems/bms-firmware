/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HELPER_H
#define HELPER_H

/** @file
 *
 * @brief
 * General helper functions
 */

#include <stdint.h>
#include <string.h>

#ifdef __INTELLISENSE__
/*
 * VS Code intellisense can't cope with all the Zephyr macro layers for logging, so provide it
 * with something more simple and make it silent.
 */

#define LOG_DBG(...) printf(__VA_ARGS__)

#define LOG_INF(...) printf(__VA_ARGS__)

#define LOG_WRN(...) printf(__VA_ARGS__)

#define LOG_ERR(...) printf(__VA_ARGS__)

#define LOG_MODULE_REGISTER(...)

#else

#include <zephyr/logging/log.h>

#endif /* VSCODE_INTELLISENSE_HACK */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Interpolation in a look-up table. Values of a must be monotonically increasing/decreasing
 *
 * @returns interpolated value of array b at position value_a
 */
float interpolate(const float a[], const float b[], size_t size, float value_a);

/**
 * Convert byte to bit-string
 *
 * Attention: Uses static buffer, not thread-safe
 *
 * @returns pointer to bit-string (8 characters + null-byte)
 */
const char *byte2bitstr(uint8_t b);

/**
 * @brief Check if a buffer is entirely zeroed.
 *
 * This function checks if all elements in a buffer are zero. It first casts the buffer
 * to a size_t pointer and checks the first size_t-sized block. If this block is zero,
 * it then compares the rest of the buffer with its initial part using memcmp.
 * The function requires that the buffer size is at least as large as size_t.
 * If the buffer size is less than size_t, the function returns EINVAL.
 *
 * @param buf A pointer to the buffer (uint8_t*) to be checked.
 * @param size The size of the buffer in bytes.
 * @return Returns 1 (true) if the buffer is entirely zeroed, 0 (false) if not,
 *         or EINVAL if the buffer size is less than size_t.
 */
static inline int is_empty(uint8_t *buf, size_t size)
{
    if (size < sizeof(size_t)) {
        return -EINVAL;
    }

    return !(*(size_t *)buf || !!memcmp(buf, buf + sizeof(size_t), size - sizeof(size_t)));
}

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
