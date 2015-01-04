/*-
 * Copyright (c) 2015 Jason Lingle
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
#ifndef WORLD_VMAP_PAINTER_H_
#define WORLD_VMAP_PAINTER_H_

#include "../math/coords.h"
#include "env-vmap.h"

/**
 * @file
 *
 * The vmap renderer provides infrastructure for populating env_vmaps by safely
 * distributing work across uMP threads.
 *
 * Population is realised as a sequence of paint operations. Each operation is
 * bound to a rectangle on the (X,Z) plane; the painting function can safely
 * assume that it has exclusive control of that rectangle during its operation.
 * Furthermore, for any sequence of paint operations, the end result is
 * guaranteed to be consistent, though the operations are not necessarily
 * applied in the exact order requested.
 */

typedef struct vmap_paint_operation_s vmap_paint_operation;

/**
 * Performs a voxel paint operation into the given vmap.
 *
 * The function MUST NOT read from or write to the vmap at any coordinate
 * outside of the bounding box defined in the vmap operation (which is
 * guaranteed not to wrap, even if the vmap is toroidal). The function may read
 * and write the vmap inside the bounding box however it pleases. The function
 * MUST NOT take input in any form other than the permitted region of the vmap
 * and the parms member of the operation, unless it can coordinate with other
 * components to ensure the consistency of such inputs.
 */
typedef void (*vmap_paint_f)(env_vmap*, const vmap_paint_operation*);

struct vmap_paint_operation_s {
  /**
   * The function to apply to effect this operation.
   */
  vmap_paint_f f;
  /**
   * The bounding box for this operation. The function is assumed to neither
   * touch nor observe any voxels outside of this region. The region is from
   * (x,z) inclusive to (x+w,z+h) exclusive.
   *
   * vmap_paint_operations passed into vmap_paint_f never have wrapping
   * coordinates.
   */
  unsigned short x, z, w, h;
  /**
   * Arbitrary parameters for use by the paint function.
   */
  coord parms[4];
};

/**
 * Initialises the vmap painter with the given vmap.
 *
 * From the time this is called to the next call to vmap_painter_flush(), the
 * contents of the vmap should be considered undefined.
 *
 * The vmap MUST remain valid until the next call to vmap_painter_flush() or
 * vmap_painter_abort().
 */
void vmap_painter_init(env_vmap*);
/**
 * Enqueues the given operation, which is copied into memory private to the
 * vmap painter.
 *
 * If the vmap is non-toroidal and the given operation references voxels
 * outside of the permissible dimensions, this call has no effect.
 *
 * Has no effect if the vmap painter is currently uninitialised.
 */
void vmap_painter_add(const vmap_paint_operation*);
/**
 * Blocks until any new operations enqueued into the vmap painter are
 * guaranteed to run after all operations enqueued so far have executed.
 *
 * Has no effect if the vmap painter is currently uninitialised.
 */
void vmap_painter_barrier(void);
/**
 * Blocks until all operations enqueued in the vmap painter have executed, then
 * deinitialises the vmap painter. After this call returns, the vmap is safe to
 * manipulate or destroy.
 *
 * Has no effect if the vmap painter is currently uninitialised.
 */
void vmap_painter_flush(void);
/**
 * Blocks until the vmap is safe to free, then deinitialises the vmap painter.
 *
 * Has no effect if the vmap painter is currently uninitialised.
 */
void vmap_painter_abort(void);

#endif /* WORLD_VMAP_PAINTER_H_ */
