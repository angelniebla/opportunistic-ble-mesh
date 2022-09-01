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

#include "simple_message_server.h"
#include "simple_message_common.h"
#include "log.h"
#include <stdint.h>
#include <stddef.h>

#include "access.h"
#include "nrf_mesh_assert.h"

/*****************************************************************************
 * Static functions
 *****************************************************************************/

static void reply_status(const simple_message_server_t * p_server,
                         const access_message_rx_t * p_message)
{
  
    access_message_tx_t reply;
    reply.opcode.opcode = simple_message_OPCODE_STATUS;
    reply.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    reply.p_buffer =p_server->name;
    reply.length = sizeof(p_server->name);
    (void) access_model_reply(p_server->model_handle, p_message, &reply);
}


static void publish_state(simple_message_server_t * p_server, bool value)
{
    access_message_tx_t msg;
    msg.opcode.opcode = simple_message_OPCODE_STATUS;
    msg.opcode.company_id = ACCESS_COMPANY_ID_NORDIC;
    msg.p_buffer = p_server->name;
    msg.length = sizeof( p_server->name);
    (void) access_model_publish(p_server->model_handle, &msg);
}
/*****************************************************************************
 * Opcode handler callbacks
 *****************************************************************************/

static void rx_set_cb(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    simple_message_server_t * p_server = p_args;
    NRF_MESH_ASSERT(p_server->set_cb != NULL);
    p_server->set_cb(p_server, p_message->meta_data.src, p_message->meta_data.dst,p_message->p_data,p_message->length);
   
}
static void rx_status_cb(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    simple_message_server_t * p_server = p_args;
    NRF_MESH_ASSERT(p_server->get_cb != NULL);
    p_server->get_cb(p_server, p_message->meta_data.src, p_message->p_data,p_message->length);
 
}
static void rx_get_cb(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    simple_message_server_t * p_server = p_args;
    NRF_MESH_ASSERT(p_server->get_cb != NULL);
      __LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Get--");
    reply_status(p_server, p_message);
}

static void rx_set_unreliable_cb(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args)
{
    simple_message_server_t * p_server = p_args;
    NRF_MESH_ASSERT(p_server->set_cb != NULL);
    bool value = (((simple_message_msg_set_unreliable_t*) p_message->p_data)->on_off) > 0;
    publish_state(p_server, value);
}

static const access_opcode_handler_t m_opcode_handlers[] =
{
    {{simple_message_OPCODE_GET_STATUS,            ACCESS_COMPANY_ID_NORDIC}, rx_get_cb},
    {{simple_message_OPCODE_SEND,            ACCESS_COMPANY_ID_NORDIC}, rx_set_cb},
    {{simple_message_OPCODE_STATUS,            ACCESS_COMPANY_ID_NORDIC}, rx_status_cb},
    
};

/*****************************************************************************
 * Public API
 *****************************************************************************/

uint32_t simple_message_server_init(simple_message_server_t * p_server, uint16_t element_index)
{
    if (p_server == NULL ||
        p_server->get_cb == NULL ||
        p_server->set_cb == NULL)
    {
        return NRF_ERROR_NULL;
    }

    access_model_add_params_t init_params;
    init_params.element_index =  element_index;
    init_params.model_id.model_id = simple_message_SERVER_MODEL_ID;
    init_params.model_id.company_id = ACCESS_COMPANY_ID_NORDIC;
    init_params.p_opcode_handlers = &m_opcode_handlers[0];
    init_params.opcode_count = sizeof(m_opcode_handlers) / sizeof(m_opcode_handlers[0]);
    init_params.p_args = p_server;
    init_params.publish_timeout_cb = NULL;
    return access_model_add(&init_params, &p_server->model_handle);
}

