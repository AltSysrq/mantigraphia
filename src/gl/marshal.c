/*-
 * Copyright (c) 2014 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL.h>
#include <glew.h>
#include "../bsd.h"

#include "../alloc.h"
#include "marshal.h"

struct glm_slab_group_s {
  void (*activate)(void*);
  void (*deactivate)(void*);
  void* userdata;
  void (*configure)(void);
  unsigned data_size;
  SDL_TLSID slab;
  SLIST_ENTRY(glm_slab_group_s) next;
};

static SLIST_HEAD(, glm_slab_group_s) slab_groups =
  SLIST_HEAD_INITIALIZER(slab_groups);

struct glm_slab_s {
  glm_slab_group* group;
  void* data;
  unsigned short* indices;

  unsigned data_off, data_max;
  unsigned short index_off, vertex_off;
};

struct glm_queued_item {
  void (*exec)(void*);
  void* userdata;

  STAILQ_ENTRY(glm_queued_item) next;
};

static SDL_sem* queue_size;
static SDL_SpinLock queue_lock;
static STAILQ_HEAD(, glm_queued_item) queue =
  STAILQ_HEAD_INITIALIZER(queue);
static GLuint vertex_buffer, index_buffer;

/* SDL doesn't provide a way to free TLS objects, so store unused TLSIDs in
 * this stack. We can safely assume that any reused TLS objects have NULL
 * values in them, as the glm_finish_thread() call (whose actions must complete
 * before freeing a slab group is permissible) will result in nulling the
 * entries in the object.
 */
struct unused_tls {
  SDL_TLSID tls;
  SLIST_ENTRY(unused_tls) next;
};
static SLIST_HEAD(, unused_tls) unused_tls_stack =
  SLIST_HEAD_INITIALIZER(unused_tls_stack);

void glm_init(void) {
  queue_size = SDL_CreateSemaphore(0);
  if (!queue_size)
    errx(EX_SOFTWARE, "Unable to create GL marshaller semaphore: %s",
         SDL_GetError());

  glGenBuffers(1, &vertex_buffer);
  glGenBuffers(1, &index_buffer);
}

glm_slab_group* glm_slab_group_new(void (*activate)(void*),
                                   void (*deactivate)(void*),
                                   void* userdata,
                                   void (*configure)(void),
                                   size_t vertex_size) {
  glm_slab_group* this;
  struct unused_tls* tls;

  this = xmalloc(sizeof(glm_slab_group));
  this->activate = activate;
  this->deactivate = deactivate;
  this->userdata = userdata;
  this->configure = configure;
  this->data_size = 65536 * vertex_size;

  if (SLIST_EMPTY(&unused_tls_stack)) {
    this->slab = SDL_TLSCreate();
    if (!this->slab)
      errx(EX_SOFTWARE, "Unable to allocate TLS object: %s",
           SDL_GetError());
  } else {
    tls = SLIST_FIRST(&unused_tls_stack);
    SLIST_REMOVE_HEAD(&unused_tls_stack, next);
    this->slab = tls->tls;
    free(tls);
  }

  SLIST_INSERT_HEAD(&slab_groups, this, next);
  return this;
}

void glm_slab_group_delete(glm_slab_group* this) {
  struct unused_tls* tls;

  tls = xmalloc(sizeof(struct unused_tls));
  tls->tls = this->slab;
  SLIST_INSERT_HEAD(&unused_tls_stack, tls, next);
  free(this);
}

glm_slab* glm_slab_get(glm_slab_group* group) {
  glm_slab* slab;

  slab = SDL_TLSGet(group->slab);
  if (!slab) {
    /* Align vertex buffer to cache line size to ensure it has whatever
     * alignment may be required.
     */
    slab = xmalloc(sizeof(glm_slab));
    slab->group = group;
    slab->indices = xmalloc(group->data_size + 65536*sizeof(short));
    slab->data = slab->indices + 65536;
    slab->data_off = slab->index_off = slab->vertex_off = 0;
    slab->data_max = group->data_size;

    SDL_TLSSet(group->slab, slab, NULL);
  }

  return slab;
}

static void flush_slab(glm_slab*, int reallocate);

unsigned short glm_slab_alloc(void** data, unsigned short** indices,
                              glm_slab* this,
                              unsigned data_size,
                              unsigned num_vertices,
                              unsigned short num_indices) {
  /* Flush if insufficient space */
  if (this->index_off > 65536 - num_indices ||
      this->data_off + data_size > this->data_max)
    flush_slab(this, 1);

  *data = ((char*)this->data) + this->data_off;
  *indices = this->indices + this->index_off;
  this->data_off += data_size;
  this->vertex_off += num_vertices;
  this->index_off += num_indices;

  return this->vertex_off - num_vertices;
}

void glm_finish_thread(void) {
  glm_slab_group* group;
  glm_slab* slab;

  SLIST_FOREACH(group, &slab_groups, next) {
    slab = SDL_TLSGet(group->slab);
    if (slab) {
      flush_slab(slab, 0);
      free(slab);
      SDL_TLSSet(group->slab, NULL, NULL);
    }
  }
}

static void execute_slab(glm_slab*);
static void flush_slab(glm_slab* this, int reallocate) {
  glm_slab* clone;

  if (this->index_off) {
    clone = xmalloc(sizeof(glm_slab));
    memcpy(clone, this, sizeof(glm_slab));
    glm_do((void(*)(void*))execute_slab, clone);

    if (reallocate) {
      this->indices = xmalloc(this->data_max + 65536*sizeof(short));
      this->data = this->indices + 65536;
      this->data_off = this->index_off = this->vertex_off = 0;
    }
  } else {
    /* Need to free the memory, even though it currently contains nothing,
     * unless the caller has requested allocated memory to be present.
     */
    if (!reallocate)
      free(this->indices);
  }
}

static void execute_slab(glm_slab* this) {
  int error;
  (*this->group->activate)(this->group->userdata);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
  glBufferData(GL_ARRAY_BUFFER, this->data_off, this->data, GL_STREAM_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->index_off*sizeof(short),
               this->indices, GL_STREAM_DRAW);
  (*this->group->configure)();
  glDrawElements(GL_TRIANGLES, this->index_off, GL_UNSIGNED_SHORT,
                 /* With an element array buffer, indicates the *offset* from
                  * the start of that buffer. We want to start at the
                  * beginning, so use zero.
                  */
                 (GLvoid*)0);

  error = glGetError();
  if (error)
    warnx("GL Error: %d (%s)", error, gluErrorString(error));

  if (this->group->deactivate)
    (*this->group->deactivate)(this->group->userdata);

  free(this->indices /* includes data */);
  free(this);
}

static void execute_set_done(void*);
void glm_done(void) {
  glm_do(execute_set_done, NULL);
}

void glm_do(void (*f)(void*), void* ud) {
  struct glm_queued_item* entry;

  entry = xmalloc(sizeof(struct glm_queued_item));
  entry->exec = f;
  entry->userdata = ud;

  SDL_AtomicLock(&queue_lock);
  STAILQ_INSERT_TAIL(&queue, entry, next);
  SDL_AtomicUnlock(&queue_lock);

  SDL_SemPost(queue_size);
}

static int is_done;
static void execute_set_done(void* ignored) {
  is_done = 1;
}

void glm_main(void) {
  struct glm_queued_item* entry;

  is_done = 0;
  while (!is_done) {
    SDL_SemWait(queue_size);

    SDL_AtomicLock(&queue_lock);
    entry = STAILQ_FIRST(&queue);
    STAILQ_REMOVE_HEAD(&queue, next);
    SDL_AtomicUnlock(&queue_lock);

    (*entry->exec)(entry->userdata);
    free(entry);
  }
}
