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

#ifndef simple_message_SERVER_H__
#define simple_message_SERVER_H__

#include <stdint.h>
#include <stdbool.h>
#include "access.h"

/**
 * @defgroup simple_message_SERVER Simple Message Server
 * @ingroup simple_message_MODEL
 * This module implements a simple proprietary Simple Message Server.
 * @{
 */

/** Simple Message Server model ID. */
#define simple_message_SERVER_MODEL_ID (0x00)

/** Forward declaration. */
typedef struct __simple_message_server simple_message_server_t;

/**
 * Get callback type.
 * @param[in] p_self Pointer to the Simple Message Server context structure.
 * @returns @c true if the state is On, @c false otherwise.
 */
typedef bool (*simple_message_get_cb_t)(const simple_message_server_t * p_self,nrf_mesh_address_t src,  uint8_t *data, uint8_t length);

/**
 * Set callback type.
 * @param[in] p_self Pointer to the Simple Message Server context structure.
 * @param[in] on_off Desired state
 * @returns @c true if the set operation was successful, @c false otherwise.
 */
typedef bool (*simple_message_set_cb_t)(const simple_message_server_t * p_server, nrf_mesh_address_t src, nrf_mesh_address_t dst,uint8_t *data, uint8_t length);

/** Simple Message Server state structure. */
struct __simple_message_server
{
    /** Model handle assigned to the server. */
    access_model_handle_t model_handle;
    /** Get callback. */
    simple_message_get_cb_t get_cb;
    /** Set callback. */
    simple_message_set_cb_t set_cb;

    uint8_t name[4];
};

/**
 * Initializes the Simple Message server.
 *
 * @note This function should only be called _once_.
 * @note The server handles the model allocation and adding.
 *
 * @param[in] p_server      Simple Message Server structure pointer.
 * @param[in] element_index Element index to add the server model.
 *
 * @retval NRF_SUCCESS         Successfully added server.
 * @retval NRF_ERROR_NULL      NULL pointer supplied to function.
 * @retval NRF_ERROR_NO_MEM    No more memory available to allocate model.
 * @retval NRF_ERROR_FORBIDDEN Multiple model instances per element is not allowed.
 * @retval NRF_ERROR_NOT_FOUND Invalid element index.
 */
uint32_t simple_message_server_init(simple_message_server_t * p_server, uint16_t element_index);

/** @} end of simple_message_SERVER */

#endif /* simple_message_SERVER_H__ */
