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
#ifndef WORLD_NFA_TURTLE_VMAP_PAINTER_H_
#define WORLD_NFA_TURTLE_VMAP_PAINTER_H_

#include "../math/coords.h"
#include "env-vmap.h"

/**
 * @file
 *
 * Implements vmap painting via iterating a non-deterministic finite-state
 * automaton to move a "turtle" and populate voxels which match an expected
 * type.
 *
 * Each NFA has up to 256 states. Each state has the following properties:
 *
 * - A "from" and "to" voxel type. On each iteration, if the voxel under the
 *   turtle matches the "from" type, it is changed to the "to" type. By
 *   default, this is 0->0.
 *
 * - Some (small) number of transitions. A state with no transitions terminates
 *   the automaton. When at least one transition exists, the automaton choses
 *   one at random. Each transition indicates the new state, as well as how to
 *   move the turtle. By default, states are terminal.
 *
 * - An optional branch count and branch state. If the branch count is
 *   non-zero, the evaluator will fork $count copies of itself, which each
 *   immediately transition to the branch state. Branches are evaluated in a
 *   breadth-first manner.
 *
 * Evaluations of the NFA have caps as to iteration count and branch
 * complexity.
 *
 * Once an NFA has been used to paint something, it becomes frozen; any further
 * attempts to edit it will fail.
 *
 * Most of this interface is designed to be directly invoked from Llua, so it
 * primarily pushes integers around and acts on global state. The
 * implementation is backed by vmap_painter, and so is dependent on its
 * (necessarily global) state as well.
 */

/**
 * Clears all NFA state and unfreezes all NFAs.
 */
void ntvp_clear_all(void);

/**
 * Returns the index of the next unused NFA (starting from 1), or 0 if none are
 * available.
 */
unsigned ntvp_new(void);
/**
 * Sets the from->to voxel change associated with the given state of the given
 * nfa.
 *
 * @param nfa The unfrozen NFA to edit.
 * @param state The state whose voxel change is to be editted.
 * @param from The voxel type that the source voxel must match in order for the
 * change to apply.
 * @param to The voxel type to which the voxel is to be changed.
 * @return 1 if successful, 0 on failure (ie, invalid or frozen NFA).
 */
unsigned ntvp_put_voxel(unsigned nfa, unsigned char state,
                        env_voxel_type from,
                        env_voxel_type to);
/**
 * Adds a transition to the given from_state in the given nfa.
 *
 * @param nfa The unfrozen NFA to edit.
 * @param from_state The state to which the new transition is being added.
 * @param to_state The state to which the automaton switches upon executing
 * this transition.
 * @param movex The amount by which to move the X coordinate of the turtle upon
 * executing this transition.
 * @param movey The amount by which to move the Y coordinate of the turtle upon
 * executing this transition.
 * @param movez The amount by which to move the Z coordinate of the turtle upon
 * executing this transition.
 * @return 1 if successful, 0 on failure (ie, invalid or frozen NFA, too many
 * transitions for from_state).
 */
unsigned ntvp_transition(unsigned nfa,
                         unsigned char from_state, unsigned char to_state,
                         signed char movex, signed char movey,
                         signed char movez);
/**
 * Changes the branch for the given state of the given nfa.
 *
 * @param nfa The unfrozen NFA to edit.
 * @param from_state The state whose branch is to be changed.
 * @param to_state The state to which evaluator forks switch upon forking.
 * @param branch_count The number of forks created each iteration an evaluator
 * is within from_state.
 * @return 1 if successful, 0 on failure (ie, invalid or frozen NFA)
 */
unsigned ntvp_branch(unsigned nfa,
                     unsigned char from_state,
                     unsigned char to_state,
                     unsigned char branch_count);

/**
 * Enqueues a painting of the given NFA, starting with state 0, at a certain
 * point in the vmap with the given boundaries.
 *
 * The NFA will be frozen by this call.
 *
 * @param nfa The NFA to render.
 * @param seed_x The X coordinate at which the turtle starts.
 * @param seed_y The Y coordinate at which the turtle starts.
 * @param seed_z The Z coordinate at which the turtle starts.
 * @param x The minimum inclusive X coordinate of the bounding box.
 * @param z The minimum inclusive Z coordinate of the bounding box.
 * @param w The X dimension of the bounding box.
 * @param h The Z dimension of the bounding box.
 * @param max_iterations The maximum number of iterations to evaluate the NFA
 * until evaluation is abandoned. This directly affects the reported cost of
 * evaluation.
 * @return Some non-zero cost of evaluation on success, 0 on failure (ie,
 * invalid NFA).
 */
unsigned ntvp_paint(unsigned nfa,
                    coord seed_x, coord seed_y, coord seed_z,
                    unsigned short x, unsigned short z,
                    unsigned short w, unsigned short h,
                    unsigned short max_iterations);

#endif /* WORLD_NFA_TURTLE_VMAP_PAINTER_H_ */
