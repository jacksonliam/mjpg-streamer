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
#include "../mmal.h"
#include "mmal_encodings.h"
#include "mmal_util.h"
#include "mmal_logging.h"
#include <string.h>
#include <stdio.h>

#define STATUS_TO_STR(x) { MMAL_##x, #x }

static struct {
   MMAL_STATUS_T status;
   const char *str;
} status_to_string_map[] =
{
   STATUS_TO_STR(SUCCESS),
   STATUS_TO_STR(ENOMEM),
   STATUS_TO_STR(ENOSPC),
   STATUS_TO_STR(EINVAL),
   STATUS_TO_STR(ENOSYS),
   STATUS_TO_STR(ENOENT),
   STATUS_TO_STR(ENXIO),
   STATUS_TO_STR(EIO),
   STATUS_TO_STR(ESPIPE),
   STATUS_TO_STR(ECORRUPT),
   STATUS_TO_STR(ENOTREADY),
   STATUS_TO_STR(ECONFIG),
   {0, 0}
};

const char *mmal_status_to_string(MMAL_STATUS_T status)
{
   unsigned i;

   for (i=0; status_to_string_map[i].str; i++)
      if (status_to_string_map[i].status == status)
         break;

   return status_to_string_map[i].str ? status_to_string_map[i].str : "UNKNOWN";
}

static struct {
   uint32_t encoding;
   uint32_t pitch_num;
   uint32_t pitch_den;
} pixel_pitch[] =
{
   {MMAL_ENCODING_I420,  1, 1},
   {MMAL_ENCODING_YV12,  1, 1},
   {MMAL_ENCODING_I422,  1, 1},
   {MMAL_ENCODING_NV21,  1, 1},
   {MMAL_ENCODING_NV12,  1, 1},
   {MMAL_ENCODING_ARGB,  4, 1},
   {MMAL_ENCODING_RGBA,  4, 1},
   {MMAL_ENCODING_RGB32, 4, 1},
   {MMAL_ENCODING_ABGR,  4, 1},
   {MMAL_ENCODING_BGRA,  4, 1},
   {MMAL_ENCODING_BGR32, 4, 1},
   {MMAL_ENCODING_RGB16, 2, 1},
   {MMAL_ENCODING_RGB24, 3, 1},
   {MMAL_ENCODING_BGR16, 2, 1},
   {MMAL_ENCODING_BGR24, 3, 1},
   /* {MMAL_ENCODING_YUVUV128, 1, 1}, That's a special case which must not be included */
   {MMAL_ENCODING_UNKNOWN, 0, 0}
};

uint32_t mmal_encoding_stride_to_width(uint32_t encoding, uint32_t stride)
{
   unsigned int i;

   for(i = 0; pixel_pitch[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(pixel_pitch[i].encoding == encoding) break;

   if(pixel_pitch[i].encoding == MMAL_ENCODING_UNKNOWN)
      return 0;

   return pixel_pitch[i].pitch_den * stride / pixel_pitch[i].pitch_num;
}

uint32_t mmal_encoding_width_to_stride(uint32_t encoding, uint32_t width)
{
   unsigned int i;

   for(i = 0; pixel_pitch[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(pixel_pitch[i].encoding == encoding) break;

   if(pixel_pitch[i].encoding == MMAL_ENCODING_UNKNOWN)
      return 0;

   return pixel_pitch[i].pitch_num * width / pixel_pitch[i].pitch_den;
}

const char* mmal_port_type_to_string(MMAL_PORT_TYPE_T type)
{
   const char *str;

   switch (type)
   {
   case MMAL_PORT_TYPE_INPUT:   str = "in";  break;
   case MMAL_PORT_TYPE_OUTPUT:  str = "out"; break;
   case MMAL_PORT_TYPE_CLOCK:   str = "clk"; break;
   case MMAL_PORT_TYPE_CONTROL: str = "ctr"; break;
   default:                     str = "invalid"; break;
   }

   return str;
}

MMAL_PARAMETER_HEADER_T *mmal_port_parameter_alloc_get(MMAL_PORT_T *port,
   uint32_t id, uint32_t size, MMAL_STATUS_T *p_status)
{
   MMAL_PARAMETER_HEADER_T *param = NULL;
   MMAL_STATUS_T status = MMAL_ENOSYS;

   if (size < sizeof(MMAL_PARAMETER_HEADER_T))
      size = sizeof(MMAL_PARAMETER_HEADER_T);

   if ((param = vcos_calloc(1, size, "mmal_port_param_get")) == NULL)
   {
      status = MMAL_ENOMEM;
      goto error;
   }

   param->id = id;
   param->size = size;

   if ((status = mmal_port_parameter_get(port, param)) == MMAL_ENOSPC)
   {
      /* We need to reallocate to get enough space for all parameter data */
      size = param->size;
      vcos_free(param);
      if ((param = vcos_calloc(1, size, "mmal_port_param_get")) == NULL)
      {
         status = MMAL_ENOMEM;
         goto error;
      }

      /* Now retrieve it again */
      param->id = id;
      param->size = size;
      status = mmal_port_parameter_get(port, param);
   }

   if (status != MMAL_SUCCESS)
      goto error;

end:
   if (p_status) *p_status = status;
   return param;
error:
   if (param) vcos_free(param);
   param = NULL;
   goto end;
}

void mmal_port_parameter_free(MMAL_PARAMETER_HEADER_T *param)
{
   vcos_free(param);
}

/** Copy buffer header metadata from source to dest
 */
void mmal_buffer_header_copy_header(MMAL_BUFFER_HEADER_T *dest, const MMAL_BUFFER_HEADER_T *src)
{
   dest->cmd    = src->cmd;
   dest->offset = src->offset;
   dest->length = src->length;
   dest->flags  = src->flags;
   dest->pts    = src->pts;
   dest->dts    = src->dts;
   *dest->type = *src->type;
}

/** Create a pool of MMAL_BUFFER_HEADER_T */
MMAL_POOL_T *mmal_port_pool_create(MMAL_PORT_T *port, unsigned int headers, uint32_t payload_size)
{
   if (!port || !port->priv)
      return NULL;

   LOG_TRACE("%s(%i:%i) port %p, headers %u, size %i", port->component->name,
             (int)port->type, (int)port->index, port, headers, (int)payload_size);

   /* Create a pool and ask the port for some memory */
   return mmal_pool_create_with_allocator(headers, payload_size, (void *)port,
                                          (mmal_pool_allocator_alloc_t)mmal_port_payload_alloc,
                                          (mmal_pool_allocator_free_t)mmal_port_payload_free);
}

/** Destroy a pool of MMAL_BUFFER_HEADER_T */
void mmal_port_pool_destroy(MMAL_PORT_T *port, MMAL_POOL_T *pool)
{
   if (!port || !port->priv || !pool)
      return;

   LOG_TRACE("%s(%i:%i) port %p, pool %p", port->component->name,
             (int)port->type, (int)port->index, port, pool);

   if (!vcos_verify(!port->is_enabled))
   {
      LOG_ERROR("port %p, pool %p destroyed while port enabled", port, pool);
      mmal_port_disable(port);
   }

   mmal_pool_destroy(pool);
}

/*****************************************************************************/
void mmal_log_dump_port(MMAL_PORT_T *port)
{
   if (!port)
      return;

   LOG_DEBUG("%s(%p)", port->name, port);

   mmal_log_dump_format(port->format);

   LOG_DEBUG(" buffers num: %i(opt %i, min %i), size: %i(opt %i, min: %i), align: %i",
            port->buffer_num, port->buffer_num_recommended, port->buffer_num_min,
            port->buffer_size, port->buffer_size_recommended, port->buffer_size_min,
            port->buffer_alignment_min);
}

/*****************************************************************************/
void mmal_log_dump_format(MMAL_ES_FORMAT_T *format)
{
   const char *name_type;

   if (!format)
      return;

   switch(format->type)
   {
   case MMAL_ES_TYPE_AUDIO: name_type = "audio"; break;
   case MMAL_ES_TYPE_VIDEO: name_type = "video"; break;
   case MMAL_ES_TYPE_SUBPICTURE: name_type = "subpicture"; break;
   default: name_type = "unknown"; break;
   }

   LOG_DEBUG("type: %s, fourcc: %4.4s", name_type, (char *)&format->encoding);
   LOG_DEBUG(" bitrate: %i, framed: %i", format->bitrate,
            !!(format->flags & MMAL_ES_FORMAT_FLAG_FRAMED));
   LOG_DEBUG(" extra data: %i, %p", format->extradata_size, format->extradata);
   switch(format->type)
   {
   case MMAL_ES_TYPE_AUDIO:
      LOG_DEBUG(" samplerate: %i, channels: %i, bps: %i, block align: %i",
               format->es->audio.sample_rate, format->es->audio.channels,
               format->es->audio.bits_per_sample, format->es->audio.block_align);
      break;

   case MMAL_ES_TYPE_VIDEO:
      LOG_DEBUG(" width: %i, height: %i, (%i,%i,%i,%i)",
               format->es->video.width, format->es->video.height,
               format->es->video.crop.x, format->es->video.crop.y,
               format->es->video.crop.width, format->es->video.crop.height);
      LOG_DEBUG(" pixel aspect ratio: %i/%i, frame rate: %i/%i",
               format->es->video.par.num, format->es->video.par.den,
               format->es->video.frame_rate.num, format->es->video.frame_rate.den);
      break;

   case MMAL_ES_TYPE_SUBPICTURE:
      break;

   default: break;
   }
}

MMAL_PORT_T *mmal_util_get_port(MMAL_COMPONENT_T *comp, MMAL_PORT_TYPE_T type, unsigned index)
{
   unsigned num;
   MMAL_PORT_T **list;

   switch (type)
   {
   case MMAL_PORT_TYPE_INPUT:
      num = comp->input_num;
      list = comp->input;
      break;

   case MMAL_PORT_TYPE_OUTPUT:
      num = comp->output_num;
      list = comp->output;
      break;

   case MMAL_PORT_TYPE_CLOCK:
      num = comp->clock_num;
      list = comp->clock;
      break;

   case MMAL_PORT_TYPE_CONTROL:
      num = 1;
      list = &comp->control;
      break;

   default:
      vcos_assert(0);
      return NULL;
   }
   if (index < num)
      return list[index];
   else
      return NULL;
}

char *mmal_4cc_to_string(char *buf, size_t len, uint32_t fourcc)
{
   char *src = (char*)&fourcc;
   vcos_assert(len >= 5);
   if (len < 5)
   {
      buf[0] = '\0';
   }
   else if (fourcc)
   {
      memcpy(buf, src, 4);
      buf[4] = '\0';
   }
   else
   {
      snprintf(buf, len, "<0>");
   }
   return buf;
}

