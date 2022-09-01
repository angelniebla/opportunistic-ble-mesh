/* Copyright (c) 2010 - 2017, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef simple_message_COMMON_H__
#define simple_message_COMMON_H__

#include <stdint.h>

/**
 * @defgroup simple_message_MODEL Simple Message model
 * Example model implementing basic behavior for sending on and off messages.
 * @ingroup md_examples_models_README
 * @{
 * @defgroup simple_message_COMMON Common Simple Message definitions
 * @{
 */

/** Simple Message opcodes. */
typedef enum
{
    simple_message_OPCODE_SEND = 0xC1,            /**< Simple Message Send. */
    simple_message_OPCODE_SEND_UNRELIABLE = 0xC2,            /**< Simple Message Send Unreliable. */
    simple_message_OPCODE_GET_STATUS = 0xC3, /**< Simple Message Get Status. */
    simple_message_OPCODE_STATUS = 0xC4          /**< Simple Message Status. */
} simple_message_opcode_t;

/** Message format for the Simple Message Set message. */
typedef struct __attribute((packed))
{
   uint8_t data[10];
} simple_message_msg;

/** Message format for th Simple Message Set Unreliable message. */
typedef struct __attribute((packed))
{
    uint8_t on_off; /**< State to set. */
    uint8_t tid;    /**< Transaction number. */
} simple_message_msg_set_unreliable_t;

/** Message format for the Simple Message Status message. */
typedef struct __attribute((packed))
{
    uint8_t present_on_off; /**< Current state. */
} simple_message_msg_status_t;

/** @} end of simple_message_COMMON */
/** @} end of simple_message_MODEL */
#endif /* simple_message_COMMON_H__ */
