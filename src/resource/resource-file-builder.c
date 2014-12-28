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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bsd.h"
#include "../alloc.h"
#include "asn1/ResourceFile.h"
#include "resource-file-builder.h"

static ResourceFile_t rf;

static void oom_if(int status) {
  if (status)
    errx(EX_UNAVAILABLE, "out of memory");
}

static void sets(OCTET_STRING_t* s, const char* str) {
  if (str)
    oom_if(OCTET_STRING_fromString(s, str));
}

static void seta85(OCTET_STRING_t* dst, const unsigned char* bin,
                   size_t szbin) {
  size_t sz85 = (szbin + 3)/4 * 5;
  unsigned dword, i, j;
  char* d;

  if (!bin) return;

  dst->buf = xmalloc(1 + sz85);
  dst->size = sz85;
  dst->buf[sz85] = 0;
  d = (char*)dst->buf;

  for (i = 0; i < szbin; i += 4) {
    dword = 0;
    for (j = i; j < i+4 && j < szbin; ++j) {
      dword <<= 8;
      dword |= bin[j];
    }

    for (j = 0; j < 5; ++j) {
      *d++ = '!' + (dword % 85);
      dword /= 85;
    }
  }
}

static void setrl(ResourceLocator_t* rl, const char* orig_str) {
  char* str;
  const char* module = (const char*)rf.modulename.buf;
  char* next_colon, * next_comma, * curr;
  ResourceIdentifier_t* ri;

  if (!strchr(orig_str, ',') && !strchr(orig_str, ':')) {
    rl->present = ResourceLocator_PR_mono;
    sets(&rl->choice.mono, orig_str);
    return;
  }

  if (!(str = strdup(orig_str))) errx(EX_UNAVAILABLE, "out of memory");

  rl->present = ResourceLocator_PR_poly;
  curr = str;
  while (curr) {
    next_comma = strchr(curr, ',');
    if (next_comma)
      *next_comma = 0;

    next_colon = strchr(curr, ':');
    if (next_colon) {
      *next_colon = 0;
      module = curr;
      curr = next_colon + 1;
    }

    ri = zxmalloc(sizeof(ResourceIdentifier_t));
    sets(&ri->module, module);
    sets(&ri->symbol, curr);
    oom_if(asn_sequence_add(&rl->choice.poly, ri));

    curr = next_comma? next_comma + 1 : NULL;
  }

  free(str);
}

static Resource_t* push_resource(Resource_PR type) {
  Resource_t* r = zxmalloc(sizeof(Resource_t));
  r->present = type;
  oom_if(asn_sequence_add(&rf.resources, r));
  return r;
}

void resource_file_builder_init(
  const char* modulename,
  const char* friendlyname,
  const char* version,
  const char* description,
  const char* author,
  const char* license,
  const char* info
) {
  (*asn_DEF_ResourceFile.free_struct)(&asn_DEF_ResourceFile, &rf, 1);
  sets(&rf.modulename, modulename);
  sets(&rf.friendlyname, friendlyname);
  sets(&rf.version, version);
  sets(&rf.description, description);
  sets(&rf.author, author);
  sets(&rf.license, license);
  if (info) {
    rf.info = zxmalloc(sizeof(OCTET_STRING_t));
    sets(rf.info, info);
  }
}

void resource_file_builder_write(const char* outfile) {
  FILE* out;
  char errbuf[128];
  size_t errlen = sizeof(errbuf);

  if (asn_check_constraints(&asn_DEF_ResourceFile, &rf, errbuf, &errlen))
    errx(EX_SOFTWARE, "Resource file is invalid: %s", errbuf);

  out = fopen(outfile, "w");
  if (!out)
    err(EX_OSERR, "Unable to open %s for writing", outfile);

  xer_fprint(out, &asn_DEF_ResourceFile, &rf);
  fclose(out);
}

void resource_file_builder_add_note(const char* text) {
  Resource_t* r = push_resource(Resource_PR_notes);
  sets(&r->choice.notes, text);
}

void resource_file_builder_add_texture(
  const char* export,
  const void* d64, const void* d32, const void* d16,
  const void* d8 , const void* d4 , const void* d2 ,
  const void* d1
) {
  Resource_t* r = push_resource(Resource_PR_texture);
  setrl(&r->choice.texture.export, export);
  seta85(&r->choice.texture.d64, d64, 64*64*3);
  seta85(&r->choice.texture.d32, d32, 32*32*3);
  seta85(&r->choice.texture.d16, d16, 16*16*3);
  seta85(&r->choice.texture.d8 , d8 ,  8* 8*3);
  seta85(&r->choice.texture.d4 , d4 ,  4* 4*3);
  seta85(&r->choice.texture.d2 , d2 ,  2* 2*3);
  seta85(&r->choice.texture.d1 , d1 ,  1* 1*3);
}

void resource_file_builder_add_palette(
  const char* export,
  unsigned colours,
  unsigned rows,
  const void* data
) {
  Resource_t* r = push_resource(Resource_PR_palette);
  setrl(&r->choice.palette.export, export);
  r->choice.palette.colours = colours;
  seta85(&r->choice.palette.data, data, colours*rows*4);
}

void resource_file_builder_add_plane(
  const char* export,
  const char* texture,
  const char* palette,
  signed sscale,
  signed tscale
) {
  Resource_t* r = push_resource(Resource_PR_plane);
  setrl(&r->choice.plane.export, export);
  setrl(&r->choice.plane.texture, texture);
  setrl(&r->choice.plane.palette, palette);
  r->choice.plane.sscale = sscale;
  r->choice.plane.tscale = tscale;
}

void resource_file_builder_add_voxel_graphic(
  const char* export,
  const char* x,
  const char* y,
  const char* z
) {
  Resource_t* r = push_resource(Resource_PR_voxelgraph);
  setrl(&r->choice.voxelgraph.export, export);
  if (x) {
    r->choice.voxelgraph.x = zxmalloc(sizeof(ResourceLocator_t));
    setrl(r->choice.voxelgraph.x, x);
  }
  if (y) {
    r->choice.voxelgraph.y = zxmalloc(sizeof(ResourceLocator_t));
    setrl(r->choice.voxelgraph.y, y);
  }
  if (z) {
    r->choice.voxelgraph.z = zxmalloc(sizeof(ResourceLocator_t));
    setrl(r->choice.voxelgraph.z, z);
  }
}

void resource_file_builder_add_voxel(
  const char* export,
  unsigned sensitivity,
  const char* typename
) {
  Resource_t* r = push_resource(Resource_PR_voxel);
  setrl(&r->choice.voxel.export, export);
  r->choice.voxel.sensitivity = sensitivity;
  sets(&r->choice.voxel.typename, typename);
}

static Resource_t* get_last_voxel(const char* fun) {
  Resource_t* r;
  if (0 == rf.resources.list.count)
    errx(EX_SOFTWARE, "rfb.%s: No voxels defined yet", fun);

  r = rf.resources.list.array[rf.resources.list.count-1];
  if (Resource_PR_voxel != r->present)
    errx(EX_SOFTWARE, "rfb.%s: Last resource is not a voxel", fun);

  return r;
}

void resource_file_builder_voxel_add_family(
  const char* import
) {
  Resource_t* r = get_last_voxel("voxel_add_family");
  ResourceLocator_t* rl = zxmalloc(sizeof(ResourceLocator_t));
  setrl(rl, import);
  oom_if(asn_sequence_add(&r->choice.voxel.family, rl));
}

void resource_file_builder_voxel_add_context(
  unsigned context,
  const char* import
) {
  Resource_t* r = get_last_voxel("voxel_add_context");
  VoxelResourceContext_t* vrc = zxmalloc(sizeof(VoxelResourceContext_t));
  vrc->context = context;
  setrl(&vrc->graphic, import);
  oom_if(asn_sequence_add(&r->choice.voxel.contexts, vrc));
}

#define MAX_LLUA_SIZE (1024*1024)

void resource_file_builder_add_script(const char* filename) {
  Resource_t* r = push_resource(Resource_PR_llua);
  FILE* in;
  /* Just keep it simple for now. There's a maximum resource file size anyway
   * for sanity, so there's no reason to do anything complicated here.
   */
  char* data = xmalloc(MAX_LLUA_SIZE);
  const char* basename_fs, * basename_bs;
  size_t nread;

  in = fopen(filename, "r");
  if (!in)
    err(EX_OSERR, "Unable to open %s for reading", filename);

  nread = fread(data, 1, MAX_LLUA_SIZE, in);
  if (nread <= 0) {
    if (feof(in))
      errx(EX_DATAERR, "Llua script file %s is empty?", filename);
    else
      err(EX_OSERR, "Error reading from %s", filename);
  }

  if (EOF != fgetc(in))
    errx(EX_DATAERR, "Llua script file %s is too big", filename);

  fclose(in);

  basename_fs = strrchr(filename, '/');
  basename_bs = strrchr(filename, '\\');
  basename_fs = basename_fs? basename_fs+1 : filename;
  basename_bs = basename_bs? basename_bs+1 : filename;
  sets(&r->choice.llua.filename, basename_fs > basename_bs?
       basename_fs : basename_bs);
  oom_if(OCTET_STRING_fromBuf(&r->choice.llua.program, data, nread));
}
