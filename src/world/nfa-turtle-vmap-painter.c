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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "../math/coords.h"
#include "../math/rand.h"
#include "env-vmap.h"
#include "vmap-painter.h"
#include "nfa-turtle-vmap-painter.h"

#define MAX_NFAS 256
#define MAX_STATES 256
#define MAX_TRANSITIONS 8

typedef struct {
  unsigned char to_state;
  signed char dx, dy, dz;
} ntvp_state_transition;

typedef struct {
  env_voxel_type from_type, to_type;
  unsigned char branch_count;
  unsigned char branch_to_state;
  unsigned char num_transitions;
  ntvp_state_transition transitions[MAX_TRANSITIONS];
} ntvp_state;

typedef struct {
  int is_frozen;
  ntvp_state states[MAX_STATES];
} ntvp_nfa;

static void ntvp_do_paint(env_vmap*, const vmap_paint_operation*);

static ntvp_nfa ntvp_nfas[MAX_NFAS];
static unsigned ntvp_num_nfas = 1;

#define CKIX(ix,max) do { if (!(ix) || (ix) >= (max)) return 0; } while (0)
#define CKNF(nfa) do { if (ntvp_nfas[nfa].is_frozen) return 0; } while (0)

void ntvp_clear_all(void) {
  ntvp_num_nfas = 1;
  memset(ntvp_nfas, 0, sizeof(ntvp_nfas));
}

unsigned ntvp_new(void) {
  CKIX(ntvp_num_nfas, MAX_NFAS);
  return ntvp_num_nfas++;
}

unsigned ntvp_put_voxel(unsigned nfa, unsigned char state,
                        env_voxel_type from,
                        env_voxel_type to) {
  CKIX(nfa, ntvp_num_nfas);
  CKNF(nfa);
  ntvp_nfas[nfa].states[state].from_type = from;
  ntvp_nfas[nfa].states[state].to_type = to;
  return 1;
}

unsigned ntvp_transition(unsigned nfa,
                         unsigned char from_state, unsigned char to_state,
                         signed char movex, signed char movey,
                         signed char movez) {
  ntvp_state_transition* t;

  CKIX(nfa, ntvp_num_nfas);
  CKNF(nfa);
  if (ntvp_nfas[nfa].states[from_state].num_transitions >= MAX_TRANSITIONS)
    return 0;

  t = ntvp_nfas[nfa].states[from_state].transitions +
    ntvp_nfas[nfa].states[from_state].num_transitions++;
  t->to_state = to_state;
  t->dx = movex;
  t->dy = movey;
  t->dz = movez;

  return 1;
}

unsigned ntvp_branch(unsigned nfa,
                     unsigned char from_state,
                     unsigned char to_state,
                     unsigned char branch_count) {
  CKIX(nfa, ntvp_num_nfas);
  CKNF(nfa);

  ntvp_nfas[nfa].states[from_state].branch_count = branch_count;
  ntvp_nfas[nfa].states[from_state].branch_to_state = to_state;

  return 1;
}

unsigned ntvp_paint(unsigned nfa,
                    coord seed_x, coord seed_y, coord seed_z,
                    unsigned short x, unsigned short z,
                    unsigned short w, unsigned short h,
                    unsigned short max_iterations) {
  vmap_paint_operation op = {
    ntvp_do_paint,
    x, z, w, h,
    { seed_x, seed_y, seed_z,
      (max_iterations << 16) | nfa }
  };

  CKIX(nfa, ntvp_num_nfas);

  ntvp_nfas[nfa].is_frozen = 1;
  vmap_painter_add(&op);

  return max_iterations? max_iterations : 1;
}

static void ntvp_do_paint(env_vmap* vmap,
                          const vmap_paint_operation* op) {
  unsigned short iterations_left = (op->parms[3] >> 16) & 0xFFFF;
  const ntvp_state* nfa = ntvp_nfas[op->parms[3] & 0xFF].states;
  /* Each transition or branch writes its new state at states[head++]; each
   * iteration advances tail, and operates on states[tail++]. Thus, we get
   * cheap breadth-first evaluation with a hard limit on recursion depth.
   */
  struct {
    unsigned short x, y, z;
    unsigned char state;
  } states[256];
  unsigned char head, tail;
  unsigned short xmin, xmax, zmin, zmax, xmask, zmask;
  unsigned i, rnd, s;

  states[0].x = op->parms[0];
  states[0].y = op->parms[1];
  states[0].z = op->parms[2];
  states[0].state = 0;

  xmin = op->x;
  xmax = umax(op->x + op->w - 1, vmap->xmax - 1);
  xmask = vmap->is_toroidal? vmap->xmax - 1 : 0xFFFF;
  zmin = op->z;
  zmax = umax(op->z + op->w - 1, vmap->zmax - 1);
  zmask = vmap->is_toroidal? vmap->zmax - 1 : 0xFFFF;

  rnd = 0;
  for (i = 0; i < 4; ++i)
    rnd = chaos_accum(rnd, op->parms[i]);
  rnd = chaos_of(rnd);

  for (tail = 0, head = 1; head != tail && iterations_left;
       ++tail, --iterations_left) {
    s = states[tail].state;

    if (states[tail].x >= xmin && states[tail].x <= xmax &&
        states[tail].z >= zmin && states[tail].z <= zmax &&
        states[tail].y < ENV_VMAP_H &&
        vmap->voxels[env_vmap_offset(vmap, states[tail].x,
                                     states[tail].y,
                                     states[tail].z)] == nfa[s].from_type)
      vmap->voxels[env_vmap_offset(vmap, states[tail].x,
                                   states[tail].y,
                                   states[tail].z)] = nfa[s].to_type;

    for (i = 0; i < nfa[s].branch_count; ++i) {
      states[head] = states[tail];
      states[head].state = nfa[s].branch_to_state;
      ++head;
    }

    if (nfa[s].num_transitions) {
      i = lcgrand(&rnd) % nfa[s].num_transitions;
      states[head] = states[tail];
      states[head].x += (signed short)nfa[s].transitions[i].dx;
      states[head].x &= xmask;
      states[head].y += (signed short)nfa[s].transitions[i].dy;
      states[head].z += (signed short)nfa[s].transitions[i].dz;
      states[head].z &= zmask;
      states[head].state = nfa[s].transitions[i].to_state;
      ++head;
    }
  }
}
