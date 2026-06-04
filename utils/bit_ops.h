/**
 * Copyright (c) 2026 Edmund C
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/**
 * @file    bit_ops.h
 * @brief   Simple, safe bit/byte/word manipulation for embedded & systems code.
 *
 * All functions operate on unsigned integers (uint32_t, uint16_t, etc.)
 * to avoid sign-extension surprises. Every function is `static inline`
 * so the header can be included in multiple translation units without
 * linker conflicts.
 *
 * @note    The bit numbering is **0-based** (LSb = bit 0, MSb = bit 31).
 *           Byte and word indices count from the **least significant**
 *           position (index 0 = lowest byte/word).
 */

#ifndef BIT_OPS_H_
#define BIT_OPS_H_

#include <stdbool.h>    /* bool, true, false */
#include <stdint.h>     /* uint8_t, uint16_t, uint32_t, uint64_t */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 *  Bit operations
 * ------------------------------------------------------------------------- */

/**
 * @brief  Read the value of a single bit.
 *
 * @param[in] value   The integer to examine.
 * @param[in] bit     Bit position (0 = least significant, 31 = most significant).
 *                    Must be 0..31.
 *
 * @return 1 if the bit is set, 0 if it is clear.
 */
static inline bool bit_get(uint32_t value, uint8_t bit)
{
    return (value >> bit) & 1U;
}

/**
 * @brief  Return a new value with a specific bit set or cleared.
 *
 * @param[in] value   Original value.
 * @param[in] bit     Bit position (0..31).
 * @param[in] val     Desired bit state (0 or 1).
 *
 * @return Modified value with the bit forced to @p val.
 */
static inline uint32_t bit_set(uint32_t value, uint8_t bit, bool val)
{
    if (val) {
        return value | (1UL << bit);
    } else {
        return value & ~(1UL << bit);
    }
}

/**
 * @brief  Toggle (invert) a single bit.
 *
 * @param[in] value   Original value.
 * @param[in] bit     Bit position (0..31).
 *
 * @return Value with the bit flipped.
 */
static inline uint32_t bit_toggle(uint32_t value, uint8_t bit)
{
    return value ^ (1UL << bit);
}

/* -------------------------------------------------------------------------
 *  Byte operations (on 32-bit values)
 * ------------------------------------------------------------------------- */

/**
 * @brief  Extract a byte from a 32-bit value.
 *
 * Byte 0 is the least significant byte (bits 7..0), byte 3 the most
 * significant (bits 31..24).
 *
 * @param[in] value      Source value.
 * @param[in] byte_idx   Byte index (0..3).
 *
 * @return The requested byte (0..255).
 */
static inline uint8_t byte_get(uint32_t value, uint8_t byte_idx)
{
    return (uint8_t)(value >> (byte_idx * 8U)) & 0xFFU;
}

/**
 * @brief  Replace a single byte within a 32-bit value.
 *
 * @param[in] value      Original value.
 * @param[in] byte_idx   Byte index (0..3) to replace.
 * @param[in] new_byte   New byte value.
 *
 * @return Value with the chosen byte replaced, all other bytes unchanged.
 */
static inline uint32_t byte_set(uint32_t value, uint8_t byte_idx, uint8_t new_byte)
{
    uint32_t mask = (uint32_t)0xFFU << (byte_idx * 8U);
    uint32_t shifted = (uint32_t)new_byte << (byte_idx * 8U);
    return (value & ~mask) | shifted;
}

/**
 * @brief  Mask (isolate) a single byte, zeroing all other bytes.
 *
 * This is a convenience wrapper that returns only the selected byte
 * (in its original position).
 *
 * @param[in] value      Source value.
 * @param[in] byte_idx   Byte index (0..3).
 *
 * @return Value with only the chosen byte preserved (e.g., 0x0000AB00).
 */
static inline uint32_t mask_byte(uint32_t value, uint8_t byte_idx)
{
    return value & ((uint32_t)0xFFU << (byte_idx * 8U));
}

/* -------------------------------------------------------------------------
 *  Word (16-bit) operations on 32-bit values
 * ------------------------------------------------------------------------- */

/**
 * @brief  Extract a 16-bit word from a 32-bit value.
 *
 * Word 0 is the lower 16 bits (bits 15..0), word 1 is the upper
 * 16 bits (bits 31..16).
 *
 * @param[in] value      Source value.
 * @param[in] word_idx   Word index (0 = low word, 1 = high word).
 *
 * @return The requested 16-bit value.
 */
static inline uint16_t word_get(uint32_t value, uint8_t word_idx)
{
    return (uint16_t)(value >> (word_idx * 16U)) & 0xFFFFU;
}

/**
 * @brief  Replace a 16-bit word within a 32-bit value.
 *
 * @param[in] value      Original value.
 * @param[in] word_idx   Word index (0 or 1).
 * @param[in] new_word   New 16-bit value.
 *
 * @return Value with the chosen word replaced.
 */
static inline uint32_t word_set(uint32_t value, uint8_t word_idx, uint16_t new_word)
{
    uint32_t mask = (uint32_t)0xFFFFU << (word_idx * 16U);
    uint32_t shifted = (uint32_t)new_word << (word_idx * 16U);
    return (value & ~mask) | shifted;
}

/**
 * @brief  Mask (isolate) a 16-bit word, zeroing all other bits.
 *
 * @param[in] value      Source value.
 * @param[in] word_idx   Word index (0 or 1).
 *
 * @return Value with only the chosen word preserved.
 */
static inline uint32_t mask_word(uint32_t value, uint8_t word_idx)
{
    return value & ((uint32_t)0xFFFFU << (word_idx * 16U));
}

/* -------------------------------------------------------------------------
 *  Endianness swapping
 * ------------------------------------------------------------------------- */

/**
 * @brief  Swap the byte order of a 16-bit value (big ↔ little endian).
 *
 * @param[in] value   Original 16-bit value.
 *
 * @return The byte-swapped value.
 */
static inline uint16_t endian_swap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

/**
 * @brief  Swap the byte order of a 32-bit value.
 *
 * @param[in] value   Original 32-bit value.
 *
 * @return The byte-swapped value.
 */
static inline uint32_t endian_swap32(uint32_t value)
{
    return  ((value >> (8*3)) & 0x000000FFUL) |
            ((value >> (8*1)) & 0x0000FF00UL) |
            ((value << (8*1)) & 0x00FF0000UL) |
            ((value << (8*3)) & 0xFF000000UL);
}

/**
 * @brief  Swap the byte order of a 64-bit value.
 *
 * @param[in] value   Original 64-bit value.
 *
 * @return The byte-swapped value.
 */
static inline uint64_t endian_swap64(uint64_t value)
{
    return  ((value >> (8*7)) & 0x00000000000000FFULL) |
            ((value >> (8*5)) & 0x000000000000FF00ULL) |
            ((value >> (8*3)) & 0x0000000000FF0000ULL) |
            ((value >> (8*1)) & 0x00000000FF000000ULL) |
            ((value << (8*1)) & 0x000000FF00000000ULL) |
            ((value << (8*3)) & 0x0000FF0000000000ULL) |
            ((value << (8*5)) & 0x00FF000000000000ULL) |
            ((value << (8*7)) & 0xFF00000000000000ULL);
}

/**
 * @brief  Check if a value is a power of two.
 *
 * A power of two has exactly one bit set (e.g., 1, 2, 4, 8, …).
 * **Zero** returns `false` (0 is not a power of two).
 *
 * @param[in] value  Unsigned 32-bit integer to test.
 *
 * @return `true` if @p value is a non-zero power of two, `false` otherwise.
 */
static inline bool is_pow_2(uint32_t value)
{
    return (value != 0U) && ((value & (value - 1U)) == 0U);
}

#ifdef __cplusplus
}
#endif

#endif /* BIT_OPS_H_ */