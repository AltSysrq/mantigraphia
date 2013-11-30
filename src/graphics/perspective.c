/*
  Copyright (c) 2013 Jason Lingle
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the author nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

     THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
     FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     SUCH DAMAGE.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../coords.h"
#include "canvas.h"
#include "perspective.h"

void perspective_init(perspective* this,
                      const canvas* screen,
                      angle fov) {
  signed fcos, fsin;

  this->sxo = screen->w / 2;
  this->syo = screen->h / 2;

  fcos = zo_cos(fov/2);
  fsin = zo_sin(fov/2);
  /* Asume square pixels. Need to multiply numerator by ZO_SCALING_FACTOR_MAX
   * because the one implicit in fcos is cancelled out by fsin, and we need to
   * account for the screen division as well.
   */
  this->zscale = fcos
               * ZO_SCALING_FACTOR_MAX / fsin
               / screen->w;
}

int perspective_proj(vo3 dst,
                     const vc3 src,
                     const perspective* this) {
  vo3 tx, rty, rtx;

  /* Translate */
  vc3dist(tx, src, this->camera, this->torus_w, this->torus_h);
  /* Rotate Y */
  rty[0] = zo_scale(tx[0], this->yrot_cos) - zo_scale(tx[2], this->yrot_sin);
  rty[1] = tx[1];
  rty[2] = zo_scale(tx[2], this->yrot_cos) + zo_scale(tx[0], this->yrot_sin);
  /* Rotate X */
  rtx[0] = rty[0];
  rtx[1] = zo_scale(rty[1], this->rxrot_cos) - zo_scale(rty[2], this->rxrot_sin);
  rtx[2] = zo_scale(rty[2], this->rxrot_cos) + zo_scale(rty[1], this->rxrot_sin);
  /* Scale Z for FOV and screen, invert Z axis to match screen coords */
  rtx[2] = -zo_scale(rtx[2], this->zscale);
  /* Discard if in front of the near clipping plane */
  if (rtx[2] <= this->near_clipping_plane) return 0;

  /* Scale X and Y by Z */
  rtx[0] /= rtx[2];
  rtx[1] /= rtx[2];

  /* Translate into screen coordinates */
  dst[0] = this->sxo + rtx[0];
  /* Subtract to invert the Y axis for screen coords */
  dst[1] = this->syo - rtx[1];
  dst[2] = rtx[2];

  return 1;
}
