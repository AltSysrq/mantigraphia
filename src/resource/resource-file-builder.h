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
#ifndef RESOURCE_RESOURCE_FILE_BUILDER_H_
#define RESOURCE_RESOURCE_FILE_BUILDER_H_

/**
 * @file
 *
 * Provides a simple state-machine interface for privelaged (ie, non-sandboxed)
 * Llua scripts to build resource files.
 *
 * The builder operates on a single, global value, which all operations
 * implicitly affect. Most subfields are constructed in one call; for those
 * that have nested collections, a single call is necessary to initialise a new
 * element, and further calls to expand the collections.
 *
 * Imports and exports are both specified with a string syntax. If the string
 * contains no colon nor comma, the string is used as a local import or natural
 * export. Otherwise, the string specifies a list of fully-qualified names
 * separated by commata. Names with a colon specify the module followed by the
 * symbol name; names omitting the module (ie, have no colon) use the same
 * module name as the previous one. The module name defaults to that of the
 * module being built.
 *
 * Fields encoded in ASCII85 are transparently encoded from binary.
 *
 * Any errors abort the process.
 *
 * No attempt is made to make this bullet-proof. In particular, the palette
 * function blindly trusts that the Llua script passes a string of the length
 * it claims to.
 */

void resource_file_builder_init(
  const char* modulename,
  const char* friendlyname,
  const char* version,
  const char* description,
  const char* author,
  const char* license,
  const char* info);
void resource_file_builder_write(const char* filename);

void resource_file_builder_add_note(const char*);

void resource_file_builder_add_texture(
  const char* export,
  const void* d64,
  const void* d32,
  const void* d16,
  const void* d8,
  const void* d4,
  const void* d2,
  const void* d1);

void resource_file_builder_add_palette(
  const char* export,
  unsigned colours,
  unsigned rows,
  const void* data);

void resource_file_builder_add_plane(
  const char* export,
  const char* texture,
  const char* palette,
  signed sscale,
  signed tscale);

void resource_file_builder_add_voxel_graphic(
  const char* export,
  const char* x,
  const char* y,
  const char* z);

void resource_file_builder_add_voxel(
  const char* export,
  unsigned sensitivity,
  const char* typename);

void resource_file_builder_voxel_add_family(
  const char* import);

void resource_file_builder_voxel_add_context(
  unsigned context,
  const char* import);

void resource_file_builder_add_script(
  const char* infile);

#endif /* RESOURCE_RESOURCE_FILE_BUILDER_H_ */
