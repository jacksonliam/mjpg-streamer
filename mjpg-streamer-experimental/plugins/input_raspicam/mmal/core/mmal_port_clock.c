/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mmal_clock.h"
#include "mmal_logging.h"
#include "core/mmal_clock_private.h"
#include "core/mmal_port_private.h"
#include "util/mmal_util.h"

#ifdef __VIDEOCORE__
# include "vcfw/rtos/common/rtos_common_mem.h"
#endif

/** Minimum number of buffers required on a clock port */
#define MMAL_PORT_CLOCK_BUFFERS_MIN  8

/** Private clock port context */
typedef struct MMAL_PORT_MODULE_T
{
   MMAL_PORT_CLOCK_EVENT_CB event_cb; /**< callback for notifying the component of clock events */
   MMAL_QUEUE_T *queue;               /**< queue for empty buffers sent to the port */
   MMAL_CLOCK_T *clock;               /**< clock module for scheduling requests */
   MMAL_BOOL_T is_reference;          /**< TRUE -> clock port is a reference, therefore
                                           will forward time updates */
} MMAL_PORT_MODULE_T;

/*****************************************************************************
 * Private functions
 *****************************************************************************/
#ifdef __VIDEOCORE__
/* FIXME: mmal_buffer_header_mem_lock() assumes that payload memory is on the
 * relocatable heap when on VC. However that is not always the case. The MMAL
 * framework will allocate memory from the normal heap when ports are connected.
 * To work around this, override the default behaviour by providing a payload
 * allocator for clock ports which always allocates from the relocatable heap. */
static uint8_t* mmal_port_clock_payload_alloc(MMAL_PORT_T *port, uint32_t payload_size)
{
   int alignment = port->buffer_alignment_min;
   uint8_t *mem;

   if (!alignment)
      alignment = 32;
   vcos_assert((alignment & (alignment-1)) == 0);

   mem = (uint8_t*)mem_alloc(payload_size, alignment, MEM_FLAG_DIRECT, port->name);
   if (!mem)
   {
      LOG_ERROR("could not allocate %u bytes", payload_size);
      return NULL;
   }
   return mem;
}

static void mmal_port_clock_payload_free(MMAL_PORT_T *port, uint8_t *payload)
{
   MMAL_PARAM_UNUSED(port);
   mem_release((MEM_HANDLE_T)payload);
}
#endif


/* Callback invoked by the clock module in response to a client request */
static void mmal_port_clock_request_cb(MMAL_CLOCK_T* clock, int64_t media_time, void *cb_data, MMAL_CLOCK_VOID_FP cb)
{
   MMAL_PORT_CLOCK_REQUEST_CB cb_client = (MMAL_PORT_CLOCK_REQUEST_CB)cb;

   /* Forward to the client */
   cb_client((MMAL_PORT_T*)clock->user_data, media_time, cb_data);
}

/* Process buffers received from other clock ports */
static MMAL_STATUS_T mmal_port_clock_process_buffer(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_CLOCK_PAYLOAD_T payload;

   if (buffer->length != sizeof(MMAL_CLOCK_PAYLOAD_T))
   {
      LOG_ERROR("invalid buffer length %d", buffer->length);
      return MMAL_EINVAL;
   }

   mmal_buffer_header_mem_lock(buffer);
   memcpy(&payload, buffer->data, sizeof(MMAL_CLOCK_PAYLOAD_T));
   mmal_buffer_header_mem_unlock(buffer);

   if (payload.magic != MMAL_CLOCK_PAYLOAD_MAGIC)
   {
      LOG_ERROR("buffer corrupt (magic %4.4s)", (char*)&payload.magic);
      return MMAL_EINVAL;
   }

   LOG_TRACE("port %s length %d id %4.4s time %"PRIi64,
         port->name, buffer->length, (char*)&payload.id, payload.time);

   switch (payload.id)
   {
   case MMAL_CLOCK_PAYLOAD_TIME:
      mmal_clock_media_time_set(port->priv->module->clock, payload.time);
      break;
   case MMAL_CLOCK_PAYLOAD_SCALE:
      mmal_clock_scale_set(port->priv->module->clock, payload.data.scale);
      break;
   default:
      LOG_ERROR("invalid id %4.4s", (char*)&payload.id);
      status = MMAL_EINVAL;
      break;
   }

   /* Finished with the buffer, so return it */
   buffer->length = 0;
   mmal_port_buffer_header_callback(port, buffer);

   return status;
}

static MMAL_STATUS_T mmal_port_clock_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;

   if (buffer->length)
      return mmal_port_clock_process_buffer(port, buffer);

   /* Queue empty buffers to be used later when forwarding clock updates */
   mmal_queue_put(module->queue, buffer);

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_clock_flush(MMAL_PORT_T *port)
{
   MMAL_BUFFER_HEADER_T *buffer;

   /* Flush empty buffers */
   buffer = mmal_queue_get(port->priv->module->queue);
   while (buffer)
   {
      mmal_port_buffer_header_callback(port, buffer);
      buffer = mmal_queue_get(port->priv->module->queue);
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_clock_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_CLOCK_PAYLOAD_T event;

   switch (param->id)
   {
      case MMAL_PARAMETER_CLOCK_REFERENCE:
      {
         const MMAL_PARAMETER_BOOLEAN_T *p = (const MMAL_PARAMETER_BOOLEAN_T*)param;
         module->is_reference = p->enable;
         event.id = MMAL_CLOCK_PAYLOAD_REFERENCE;
         event.time = mmal_clock_media_time_get(module->clock);
         event.data.enable = p->enable;
      }
      break;
      case MMAL_PARAMETER_CLOCK_ACTIVE:
      {
         const MMAL_PARAMETER_BOOLEAN_T *p = (const MMAL_PARAMETER_BOOLEAN_T*)param;
         status = mmal_clock_active_set(module->clock, p->enable);
         event.id = MMAL_CLOCK_PAYLOAD_ACTIVE;
         event.time = mmal_clock_media_time_get(module->clock);
         event.data.enable = p->enable;
      }
      break;
      case MMAL_PARAMETER_CLOCK_SCALE:
      {
         const MMAL_PARAMETER_RATIONAL_T *p = (const MMAL_PARAMETER_RATIONAL_T*)param;
         status = mmal_port_clock_scale_set(port, p->value);
         event.id = MMAL_CLOCK_PAYLOAD_SCALE;
         event.time = mmal_clock_media_time_get(module->clock);
         event.data.scale = p->value;
      }
      break;
      case MMAL_PARAMETER_CLOCK_TIME:
      {
         const MMAL_PARAMETER_INT64_T *p = (const MMAL_PARAMETER_INT64_T*)param;
         status = mmal_port_clock_media_time_set(port, p->value);
         event.id = MMAL_CLOCK_PAYLOAD_INVALID;
      }
      break;
      case MMAL_PARAMETER_CLOCK_TIME_OFFSET:
      {
         const MMAL_PARAMETER_INT64_T *p = (const MMAL_PARAMETER_INT64_T*)param;
         status = mmal_port_clock_media_time_offset_set(port, p->value);
         event.id = MMAL_CLOCK_PAYLOAD_INVALID;
      }
      break;
      case MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD:
      {
         const MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T *p = (MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T *)param;
         status = mmal_clock_update_threshold_set(module->clock, p);
         event.id = MMAL_CLOCK_PAYLOAD_INVALID;
      }
      break;
      case MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD:
      {
         const MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T *p = (MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T *)param;
         status = mmal_clock_discont_threshold_set(module->clock, p);
         event.id = MMAL_CLOCK_PAYLOAD_INVALID;
      }
      break;
      case MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD:
      {
         const MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T *p = (MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T *)param;
         status = mmal_clock_request_threshold_set(module->clock, p);
         event.id = MMAL_CLOCK_PAYLOAD_INVALID;
      }
      break;
      default:
         return MMAL_ENOSYS;
   }

   /* Notify the component */
   if (module->event_cb && status == MMAL_SUCCESS && event.id != MMAL_CLOCK_PAYLOAD_INVALID)
      module->event_cb(port, &event);

   return status;
}

static MMAL_STATUS_T mmal_port_clock_parameter_get(MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;

   switch (param->id)
   {
      case MMAL_PARAMETER_CLOCK_REFERENCE:
      {
         MMAL_PARAMETER_BOOLEAN_T *p = (MMAL_PARAMETER_BOOLEAN_T*)param;
         p->enable = module->is_reference;
      }
      break;
      case MMAL_PARAMETER_CLOCK_ACTIVE:
      {
         MMAL_PARAMETER_BOOLEAN_T *p = (MMAL_PARAMETER_BOOLEAN_T*)param;
         p->enable = mmal_clock_is_active(module->clock);
      }
      break;
      case MMAL_PARAMETER_CLOCK_SCALE:
      {
         MMAL_PARAMETER_RATIONAL_T *p = (MMAL_PARAMETER_RATIONAL_T*)param;
         p->value = mmal_clock_scale_get(module->clock);
      }
      break;
      case MMAL_PARAMETER_CLOCK_TIME:
      {
         MMAL_PARAMETER_INT64_T *p = (MMAL_PARAMETER_INT64_T*)param;
         p->value = mmal_clock_media_time_get(module->clock);
      }
      break;
      case MMAL_PARAMETER_CLOCK_TIME_OFFSET:
      {
         MMAL_PARAMETER_INT64_T *p = (MMAL_PARAMETER_INT64_T*)param;
         p->value = mmal_clock_media_time_offset_get(module->clock);
      }
      break;
      case MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD:
      {
         MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T *p = (MMAL_PARAMETER_CLOCK_UPDATE_THRESHOLD_T *)param;
         mmal_clock_update_threshold_get(module->clock, p);
      }
      break;
      case MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD:
      {
         MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T *p = (MMAL_PARAMETER_CLOCK_DISCONT_THRESHOLD_T *)param;
         mmal_clock_discont_threshold_get(module->clock, p);
      }
      break;
      case MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD:
      {
         MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T *p = (MMAL_PARAMETER_CLOCK_REQUEST_THRESHOLD_T *)param;
         mmal_clock_request_threshold_get(module->clock, p);
      }
      break;
      default:
         return MMAL_ENOSYS;
   }
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_clock_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_clock_disable(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;

   if (mmal_clock_is_active(module->clock))
      mmal_clock_active_set(module->clock, MMAL_FALSE);

   mmal_port_clock_flush(port);

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_clock_set_format(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_clock_connect(MMAL_PORT_T *port, MMAL_PORT_T *other_port)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(other_port);
   return MMAL_ENOSYS;
}

/* Send a payload buffer to a connected port/client */
static MMAL_STATUS_T mmal_port_clock_forward_payload(MMAL_PORT_T *port, const MMAL_CLOCK_PAYLOAD_T *payload)
{
   MMAL_STATUS_T status;
   MMAL_BUFFER_HEADER_T *buffer;

   buffer = mmal_queue_get(port->priv->module->queue);
   if (!buffer)
   {
      LOG_ERROR("no free buffers available");
      return MMAL_ENOSPC;
   }

   status = mmal_buffer_header_mem_lock(buffer);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to lock buffer %s", mmal_status_to_string(status));
      mmal_queue_put_back(port->priv->module->queue, buffer);
      goto end;
   }
   buffer->length = sizeof(MMAL_CLOCK_PAYLOAD_T);
   memcpy(buffer->data, payload, buffer->length);
   mmal_buffer_header_mem_unlock(buffer);

   mmal_port_buffer_header_callback(port, buffer);

end:
   return status;
}

/* Send a clock time update to a connected port/client */
static MMAL_STATUS_T mmal_port_clock_forward_media_time(MMAL_PORT_T *port, int64_t media_time)
{
   MMAL_CLOCK_PAYLOAD_T payload;

   payload.id = MMAL_CLOCK_PAYLOAD_TIME;
   payload.magic = MMAL_CLOCK_PAYLOAD_MAGIC;
   payload.time = media_time;

   return mmal_port_clock_forward_payload(port, &payload);
}

/* Send a clock scale update to a connected port/client */
static MMAL_STATUS_T mmal_port_clock_forward_scale(MMAL_PORT_T *port, MMAL_RATIONAL_T scale)
{
   MMAL_CLOCK_PAYLOAD_T payload;

   payload.id = MMAL_CLOCK_PAYLOAD_SCALE;
   payload.magic = MMAL_CLOCK_PAYLOAD_MAGIC;
   payload.time = mmal_clock_media_time_get(port->priv->module->clock);
   payload.data.scale = scale;

   return mmal_port_clock_forward_payload(port, &payload);
}

/* Initialise all callbacks and setup internal resources */
static MMAL_STATUS_T mmal_port_clock_setup(MMAL_PORT_T *port, MMAL_PORT_CLOCK_EVENT_CB event_cb)
{
   MMAL_STATUS_T status;

   status = mmal_clock_create(&port->priv->module->clock);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to create clock module on port %s (%s)", port->name, mmal_status_to_string(status));
      return status;
   }
   port->priv->module->clock->user_data = port;

   port->buffer_size = sizeof(MMAL_CLOCK_PAYLOAD_T);
   port->buffer_size_min = sizeof(MMAL_CLOCK_PAYLOAD_T);
   port->buffer_num_min = MMAL_PORT_CLOCK_BUFFERS_MIN;
   port->buffer_num_recommended = MMAL_PORT_CLOCK_BUFFERS_MIN;

   port->priv->module->event_cb = event_cb;
   port->priv->module->queue = mmal_queue_create();
   if (!port->priv->module->queue)
   {
      mmal_clock_destroy(port->priv->module->clock);
      return MMAL_ENOMEM;
   }

   port->priv->pf_set_format = mmal_port_clock_set_format;
   port->priv->pf_enable = mmal_port_clock_enable;
   port->priv->pf_disable = mmal_port_clock_disable;
   port->priv->pf_send = mmal_port_clock_send;
   port->priv->pf_flush = mmal_port_clock_flush;
   port->priv->pf_parameter_set = mmal_port_clock_parameter_set;
   port->priv->pf_parameter_get = mmal_port_clock_parameter_get;
   port->priv->pf_connect = mmal_port_clock_connect;
#ifdef __VIDEOCORE__
   port->priv->pf_payload_alloc = mmal_port_clock_payload_alloc;
   port->priv->pf_payload_free = mmal_port_clock_payload_free;
   port->capabilities = MMAL_PORT_CAPABILITY_ALLOCATION;
#endif

   return status;
}

/* Release all internal resources */
static void mmal_port_clock_teardown(MMAL_PORT_T *port)
{
   if (!port)
      return;
   mmal_queue_destroy(port->priv->module->queue);
   mmal_clock_destroy(port->priv->module->clock);
}

/*****************************************************************************
 * Public functions
 *****************************************************************************/
/* Allocate a clock port */
MMAL_PORT_T* mmal_port_clock_alloc(MMAL_COMPONENT_T *component, MMAL_PORT_CLOCK_EVENT_CB event_cb)
{
   MMAL_PORT_T *port;

   port = mmal_port_alloc(component, MMAL_PORT_TYPE_CLOCK, sizeof(MMAL_PORT_MODULE_T));
   if (!port)
      return NULL;

   if (mmal_port_clock_setup(port, event_cb) != MMAL_SUCCESS)
   {
      mmal_port_free(port);
      return NULL;
   }

   return port;
}

/* Free a clock port */
void mmal_port_clock_free(MMAL_PORT_T *port)
{
   mmal_port_clock_teardown(port);
   mmal_port_free(port);
}

/* Allocate an array of clock ports */
MMAL_PORT_T **mmal_ports_clock_alloc(MMAL_COMPONENT_T *component, unsigned int ports_num, MMAL_PORT_CLOCK_EVENT_CB event_cb)
{
   unsigned int i;
   MMAL_PORT_T **ports;

   ports = mmal_ports_alloc(component, ports_num, MMAL_PORT_TYPE_CLOCK, sizeof(MMAL_PORT_MODULE_T));
   if (!ports)
      return NULL;

   for (i = 0; i < ports_num; i++)
   {
      if (mmal_port_clock_setup(ports[i], event_cb) != MMAL_SUCCESS)
         break;
   }

   if (i != ports_num)
   {
      for (ports_num = i, i = 0; i < ports_num; i++)
         mmal_port_clock_free(ports[i]);
      vcos_free(ports);
      return NULL;
   }

   return ports;
}

/* Free an array of clock ports */
void mmal_ports_clock_free(MMAL_PORT_T **ports, unsigned int ports_num)
{
   unsigned int i;

   for (i = 0; i < ports_num; i++)
      mmal_port_clock_free(ports[i]);
   vcos_free(ports);
}

/* Register a callback request */
MMAL_STATUS_T mmal_port_clock_request_add(MMAL_PORT_T *port, int64_t media_time, int64_t offset,
      MMAL_PORT_CLOCK_REQUEST_CB cb, void *cb_data)
{
   return mmal_clock_request_add(port->priv->module->clock, media_time, offset,
                                 mmal_port_clock_request_cb, cb_data, (MMAL_CLOCK_VOID_FP)cb);
}

/* Flush all pending clock requests */
MMAL_STATUS_T mmal_port_clock_request_flush(MMAL_PORT_T *port)
{
   return mmal_clock_request_flush(port->priv->module->clock);
}

/* Set the media-time on the clock port */
MMAL_STATUS_T mmal_port_clock_media_time_set(MMAL_PORT_T *port, int64_t media_time)
{
   MMAL_STATUS_T status;

   status = mmal_clock_media_time_set(port->priv->module->clock, media_time);
   if (status != MMAL_SUCCESS)
   {
      LOG_DEBUG("clock update ignored");
      return status;
   }

   /* Only forward time updates if this port is set as a reference clock port */
   if (port->priv->module->is_reference)
      mmal_port_clock_forward_media_time(port, mmal_clock_media_time_get(port->priv->module->clock));

   return status;
}

/* Set the media-time offset on the clock port */
MMAL_STATUS_T mmal_port_clock_media_time_offset_set(MMAL_PORT_T *port, int64_t offset)
{
   MMAL_STATUS_T status;

   status = mmal_clock_media_time_offset_set(port->priv->module->clock, offset);

   /* The media-time has effectively changed, so need to inform connected clock ports */
   if (port->priv->module->is_reference)
      mmal_port_clock_forward_media_time(port, mmal_clock_media_time_get(port->priv->module->clock));

   return status;
}

/* Return the current media-time */
int64_t mmal_port_clock_media_time_get(MMAL_PORT_T *port)
{
   return mmal_clock_media_time_get(port->priv->module->clock);
}

/* Return the media-time offset */
int64_t mmal_port_clock_media_time_offset_get(MMAL_PORT_T *port)
{
   return mmal_clock_media_time_offset_get(port->priv->module->clock);
}

/* Set the clock scale factor */
MMAL_STATUS_T mmal_port_clock_scale_set(MMAL_PORT_T *port, MMAL_RATIONAL_T scale)
{
   MMAL_STATUS_T status;

   status = mmal_clock_scale_set(port->priv->module->clock, scale);

   if (port->priv->module->is_reference)
      mmal_port_clock_forward_scale(port, scale);

   return status;
}

/* Return the clock scale factor */
MMAL_RATIONAL_T mmal_port_clock_scale_get(MMAL_PORT_T *port)
{
   return mmal_clock_scale_get(port->priv->module->clock);
}

/* Return TRUE if clock is running (media-time is advancing) */
MMAL_BOOL_T mmal_port_clock_is_active(MMAL_PORT_T *port)
{
   return mmal_clock_is_active(port->priv->module->clock);
}

