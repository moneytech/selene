/*
 * Licensed to Paul Querna under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * Paul Querna licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sln_brigades.h"
#include "sln_types.h"
#include "sln_assert.h"

#include <string.h>

selene_error_t*
sln_brigade_create(sln_brigade_t **out_bb)
{
  sln_brigade_t* bb = calloc(1, sizeof(sln_brigade_t));

  SLN_RING_INIT(&bb->list, sln_bucket_t, link);

  *out_bb = bb;

  return SELENE_SUCCESS;
}

void
sln_brigade_destroy(sln_brigade_t *bb)
{
  sln_brigade_clear(bb);

  free(bb);
}

void
sln_brigade_clear(sln_brigade_t *bb)
{
  sln_bucket_t *e;

  while (!SLN_BRIGADE_EMPTY(bb)) {
      e = SLN_BRIGADE_FIRST(bb);
      sln_bucket_destroy(e);
  }
}

size_t
sln_brigade_size(sln_brigade_t *bb)
{
  /* TODO: cache current value inside the sln_brigade_t structure */
  size_t total = 0;
  sln_bucket_t *b;

  SLN_RING_FOREACH(b, &(bb)->list, sln_bucket_t, link)
  {
    total += b->size;
  }

  return total;
}

selene_error_t*
sln_brigade_flatten(sln_brigade_t *bb, char *c, size_t *len)
{
  /**
   * This is very similiar to APR's, and based upon apr_brigade_flatten.
   * The fundamental difference is that we consume buckets as they are 
   * stored into the output buffer.
   */

  size_t actual = 0;
  sln_bucket_t *b;
  sln_bucket_t *iter;

  SLN_RING_FOREACH_SAFE(b, iter, &(bb)->list, sln_bucket_t, link)
  {
    size_t data_len = b->size;

    /* If we would overflow. */
    if (data_len + actual > *len) {
      data_len = *len - actual;
    }

    /* XXX: It appears that overflow of the final bucket
     * is DISCARDED without any warning to the caller.
     *
     * No, we only copy the data up to their requested size.  -- jre
     */
    memcpy(c, b->data, data_len);
    c += data_len;
    actual += data_len;

    SLN_BUCKET_REMOVE(b);

    if (b->size != data_len) {
      sln_bucket_t *tmpe;
      SLN_ASSERT(actual < *len);

      SELENE_ERR(sln_bucket_create_copy_bytes(&tmpe, b->data + data_len, b->size - data_len));

      SLN_BRIGADE_INSERT_HEAD(bb, tmpe);
    }

    sln_bucket_destroy(b);

    /* This could probably be actual == *len, but be safe from stray
     * photons. */
    if (actual >= *len) {
        break;
    }
  }

  *len = actual;

  return SELENE_SUCCESS;
}

