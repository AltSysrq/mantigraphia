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
#ifndef GL_MARSHAL_H_
#define GL_MARSHAL_H_

#include <stdlib.h>

/**
 * @file
 * The GL marshalling infrastructure allows multiple threads to independently
 * generate geometry to send to OpenGL, while keeping the potentially large
 * memory transfers asynchronous from the generation process.
 *
 * The single primitive operation in the system is the enqueueing of a function
 * to execute on the OpenGL thread. This is primarily used for state changes;
 * geometry is always done by using the slab system built on top of it.
 *
 * A "slab group" is a set of identical per-thread slabs.
 *
 * A slab is a combination of an activation function, activation data, and up
 * to 65536 vertics and indices. When a slab is eventually flushed to OpenGL,
 * its activation function is run, then all data initialised within the slab
 * are rendered.
 */

typedef struct glm_slab_group_s glm_slab_group;
typedef struct glm_slab_s glm_slab;

/**
 * Allocates a new slab group with the given activation function and
 * userdata. This is not safe to call when marshalling may be active.
 *
 * @param activate The activation function run on the OpenGL thread when
 * derived slabs are to be rendered.
 * @param deactivate Function to call after drawing the data with OpenGL. NULL
 * indicates to do nothing.
 * @param userdata Parameter to pass to activate and deactivate. It is the
 * caller's responsibility to ensure the continued validity of this pointer.
 * @param configure Function to call after sending data to OpenGL, but before
 * drawing it, to configure the VBO.
 * @param vertex_size The size of a vertex, to determine slab data size.
 * @return The newly-allocated slab group.
 */
glm_slab_group* glm_slab_group_new(void (*activate)(void*),
                                   void (*deactivate)(void*),
                                   void* userdata,
                                   void (*configure)(void),
                                   size_t vertex_size);
/**
 * Frees the given slab group and all slabs derived therefrom. This is not safe
 * to call when marshalling may be active.
 */
void glm_slab_group_delete(glm_slab_group*);

/**
 * Sets the primitive type used for drawing elements in the given slab. The
 * default is GL_TRIANGLES.
 *
 * The effect of calling this when any operations are pending is undefined.
 */
void glm_slab_group_set_primitive(glm_slab_group*,int);

/**
 * Returns the slab associated with the current thread and the given slab
 * group. Repeated calls by the same thread with the same input return the same
 * value. The returned pointer is managed by the slab group.
 *
 * A thread which calls this function MUST call glm_finish_thread().
 */
glm_slab* glm_slab_get(glm_slab_group*);

/**
 * Allocates space within the given slab for some number of vertices and
 * indices. The pointers produced by this call are only valid until the next
 * call to glm_slab_alloc() on the same slab or the next call to
 * glm_finish_thread(), whichever comes first.
 *
 * @param data The pointer this value references is set to base of the first
 * vertex that was allocated.
 * @param indices The pointer this value references is set to the first index
 * that was allocated.
 * @param slab The slab from which to allocate memory.
 * @param data_size The amount of data to allocate for the vertices, in bytes.
 * @param num_vertices The number of vertices being allocated.
 * @param num_indices The number of indices to allocate.
 * @return The index of the first allocated vector.
 */
unsigned short glm_slab_alloc(void** data, unsigned short** indices,
                              glm_slab* slab,
                              unsigned data_size,
                              unsigned num_vertices,
                              unsigned short num_indices);
/**
 * Simplified interface to glm_slab_alloc().
 *
 * @param data Same as in glm_slab_alloc(), but implicitly cast to void**.
 * @param indices Same as in glm_slab_alloc().
 * @param slab Same as in glm_slab_alloc().
 * @param nv The number of vertices to allocate. The size is determined by
 * dereferencing data twice.
 * @param ni The number of indices to allocate.
 */
#define GLM_ALLOC(data, indices, slab, nv, ni)   \
  glm_slab_alloc((void**)(data), indices, slab,  \
                 nv*sizeof(**data), nv, ni)

/**
 * Flushes all slabs belonging to the current thread, making them ready for
 * drawing.
 */
void glm_finish_thread(void);
/**
 * Enqueues a task which will cause glm_main() to return when executed.
 */
void glm_done(void);
/**
 * Enqueues the given function to be executed on the OpenGL thread, with the
 * given parameter. Calls to this function by the same thread are guaranteed to
 * be executed in the order provided; ordering between threads is only
 * guaranteed only as much as the caller can guarantee non-concurrent execution
 * of this function.
 */
void glm_do(void (*)(void*), void*);

/**
 * Executes tasks enqueued for OpenGL marshalling. Returns after executing a
 * task enqueued by glm_do().
 */
void glm_main(void);

/**
 * Initialises global data needed by the OpenGL marshaller. This must be called
 * exactly once before any other glm function.
 */
void glm_init(void);

#endif /* GL_MARSHAL_H_ */
