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
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/vc/mmal_vc_shm.h"

#ifdef ENABLE_MMAL_VCSM
# include "user-vcsm.h"
#endif /* ENABLE_MMAL_VCSM */

#define MMAL_VC_PAYLOAD_ELEM_MAX 512

typedef struct MMAL_VC_PAYLOAD_ELEM_T
{
   struct MMAL_VC_PAYLOAD_ELEM_T *next;
   void *handle;
   void *vc_handle;
   uint8_t *mem;
   MMAL_BOOL_T in_use;
} MMAL_VC_PAYLOAD_ELEM_T;

typedef struct MMAL_VC_PAYLOAD_LIST_T
{
   MMAL_VC_PAYLOAD_ELEM_T list[MMAL_VC_PAYLOAD_ELEM_MAX];
   VCOS_MUTEX_T lock;
} MMAL_VC_PAYLOAD_LIST_T;

static int mmal_vc_shm_initialised;
static MMAL_VC_PAYLOAD_LIST_T mmal_vc_payload_list;
static VCOS_ONCE_T once = VCOS_ONCE_INIT;
static VCOS_MUTEX_T refcount_lock;

static void mmal_vc_shm_init_once(void)
{
   vcos_mutex_create(&refcount_lock, VCOS_FUNCTION);
}

static void mmal_vc_payload_list_init()
{
   vcos_mutex_create(&mmal_vc_payload_list.lock, "mmal_vc_payload_list");
}

static void mmal_vc_payload_list_exit()
{
   vcos_mutex_delete(&mmal_vc_payload_list.lock);
}

static MMAL_VC_PAYLOAD_ELEM_T *mmal_vc_payload_list_get()
{
   MMAL_VC_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&mmal_vc_payload_list.lock);
   for (i = 0; i < MMAL_VC_PAYLOAD_ELEM_MAX; i++)
   {
      if (mmal_vc_payload_list.list[i].in_use)
         continue;
      elem = &mmal_vc_payload_list.list[i];
      elem->in_use = 1;
      break;
   }
   vcos_mutex_unlock(&mmal_vc_payload_list.lock);

   return elem;
}

static void mmal_vc_payload_list_release(MMAL_VC_PAYLOAD_ELEM_T *elem)
{
   vcos_mutex_lock(&mmal_vc_payload_list.lock);
   elem->handle = elem->vc_handle = 0;
   elem->mem = 0;
   elem->in_use = 0;
   vcos_mutex_unlock(&mmal_vc_payload_list.lock);
}

static MMAL_VC_PAYLOAD_ELEM_T *mmal_vc_payload_list_find_mem(uint8_t *mem)
{
   MMAL_VC_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&mmal_vc_payload_list.lock);
   for (i = 0; i < MMAL_VC_PAYLOAD_ELEM_MAX; i++)
   {
      if (!mmal_vc_payload_list.list[i].in_use)
         continue;
      if (mmal_vc_payload_list.list[i].mem != mem)
         continue;
      elem = &mmal_vc_payload_list.list[i];
      break;
   }
   vcos_mutex_unlock(&mmal_vc_payload_list.lock);

   return elem;
}

static MMAL_VC_PAYLOAD_ELEM_T *mmal_vc_payload_list_find_handle(uint8_t *mem)
{
   MMAL_VC_PAYLOAD_ELEM_T *elem = 0;
   unsigned int i;

   vcos_mutex_lock(&mmal_vc_payload_list.lock);
   for (i = 0; i < MMAL_VC_PAYLOAD_ELEM_MAX; i++)
   {
      if (!mmal_vc_payload_list.list[i].in_use)
         continue;
      if (mmal_vc_payload_list.list[i].vc_handle != (void *)mem)
         continue;
      elem = &mmal_vc_payload_list.list[i];
      break;
   }
   vcos_mutex_unlock(&mmal_vc_payload_list.lock);

   return elem;
}

/** Initialise the shared memory system */
MMAL_STATUS_T mmal_vc_shm_init(void)
{
   MMAL_STATUS_T ret = MMAL_SUCCESS;
   vcos_once(&once, mmal_vc_shm_init_once);

   vcos_mutex_lock(&refcount_lock);
   mmal_vc_shm_initialised++;
   if (mmal_vc_shm_initialised > 1)
      goto unlock;

#ifdef ENABLE_MMAL_VCSM
   if (vcsm_init() != 0)
   {
      LOG_ERROR("could not initialize vc shared memory service");
      ret = MMAL_EIO;
      goto unlock;
   }
#endif /* ENABLE_MMAL_VCSM */

   mmal_vc_payload_list_init();
unlock:
   vcos_mutex_unlock(&refcount_lock);
   return ret;
}

void mmal_vc_shm_exit(void)
{
   if (mmal_vc_shm_initialised <= 0)
      goto unlock;

   mmal_vc_shm_initialised--;
   if (mmal_vc_shm_initialised != 0)
      goto unlock;

#ifdef ENABLE_MMAL_VCSM
   vcsm_exit();
#endif

   mmal_vc_payload_list_exit();
unlock:
   vcos_mutex_unlock(&refcount_lock);
}

/** Allocate a shared memory buffer */
uint8_t *mmal_vc_shm_alloc(uint32_t size)
{
   uint8_t *mem = NULL;

   MMAL_VC_PAYLOAD_ELEM_T *payload_elem = mmal_vc_payload_list_get();
   if (!payload_elem)
   {
      LOG_ERROR("could not get a free slot in the payload list");
      return NULL;
   }

#ifdef ENABLE_MMAL_VCSM
   unsigned int vcsm_handle = vcsm_malloc_cache(size, VCSM_CACHE_TYPE_HOST, "mmal_vc_port buffer");
   unsigned int vc_handle = vcsm_vc_hdl_from_hdl(vcsm_handle);
   mem = (uint8_t *)vcsm_lock( vcsm_handle );
   if (!mem || !vc_handle)
   {
      LOG_ERROR("could not allocate %i bytes of shared memory (handle %x) - mem %p, vc_hdl %08X",
                (int)size, vcsm_handle, mem, vc_handle);
      if (mem)
         vcsm_unlock_hdl(vcsm_handle);
      if (vcsm_handle)
         vcsm_free(vcsm_handle);
      mmal_vc_payload_list_release(payload_elem);
      return NULL;
   }

   /* The memory area is automatically mem-locked by vcsm's fault
    * handler when it is next used. So leave it unlocked until it
    * is needed.
    */
   vcsm_unlock_hdl(vcsm_handle);

   payload_elem->mem = mem;
   payload_elem->handle = (void *)(intptr_t)vcsm_handle;
   payload_elem->vc_handle = (void *)(intptr_t)vc_handle;
#else /* ENABLE_MMAL_VCSM */
   MMAL_PARAM_UNUSED(size);
   mmal_vc_payload_list_release(payload_elem);
#endif /* ENABLE_MMAL_VCSM */

   return mem;
}

/** Free a shared memory buffer */
MMAL_STATUS_T mmal_vc_shm_free(uint8_t *mem)
{
   MMAL_VC_PAYLOAD_ELEM_T *payload_elem = mmal_vc_payload_list_find_mem(mem);
   if (payload_elem)
   {
#ifdef ENABLE_MMAL_VCSM
      vcsm_free((uintptr_t)payload_elem->handle);
#endif /* ENABLE_MMAL_VCSM */
      mmal_vc_payload_list_release(payload_elem);
      return MMAL_SUCCESS;
   }

   return MMAL_EINVAL;
}

/** Lock a shared memory buffer */
uint8_t *mmal_vc_shm_lock(uint8_t *mem, uint32_t workaround)
{
   /* Zero copy stuff */
   MMAL_VC_PAYLOAD_ELEM_T *elem = mmal_vc_payload_list_find_handle(mem);
   MMAL_PARAM_UNUSED(workaround);

   if (elem) {
      mem = elem->mem;
#ifdef ENABLE_MMAL_VCSM
      void *p = vcsm_lock((uintptr_t)elem->handle);
      if (!p)
         assert(0);
#endif /* ENABLE_MMAL_VCSM */
   }

   return mem;
}

/** Unlock a shared memory buffer */
uint8_t *mmal_vc_shm_unlock(uint8_t *mem, uint32_t *length, uint32_t workaround)
{
   /* Zero copy stuff */
   MMAL_VC_PAYLOAD_ELEM_T *elem = mmal_vc_payload_list_find_mem(mem);
   MMAL_PARAM_UNUSED(workaround);

   if (elem)
   {
      *length = 0;
      mem = (uint8_t *)elem->vc_handle;
#ifdef ENABLE_MMAL_VCSM
      vcsm_unlock_ptr(elem->mem);
#endif /* ENABLE_MMAL_VCSM */
   }

   return mem;
}
