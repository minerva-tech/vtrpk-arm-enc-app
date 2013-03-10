#include <stdlib.h>
#include <malloc.h>

#include "types.h"
#include "tables.h"

#ifndef AVCENC_BS_H
#define AVCENC_BS_H

#define AVCENC_MAX(a, b)            ( (a) > (b) ? (a) : (b) )
#define AVCENC_MIN(a, b)            ( (a) > (b) ? (b) : (a) )
#define AVCENC_AVG2(a, b)           ( ((a) + (b) + 1) >> 1)
#define AVCENC_AVG4(a, b, c, d)     ( ((a) + (b) + (c) + (d) + 2) >> 2)
#define AVCENC_ABS(v)               ( ((v) ^ AVCENC_SIGN(v)) - AVCENC_SIGN(v) )
#define AVCENC_SIGN(v)              ( (v) >> (sizeof(v) * 8 - 1) )
#define AVCENC_RANGE(v, a, b)       ( ((v) < (a)) ? (a) : ((b) < (v)) ? (b) : (v) )

#define UMC_CHECK(EXPRESSION, ERR_CODE) { \
  if (!(EXPRESSION)) { \
    return ERR_CODE; \
  } \
}

#define UMC_FREE(PTR) { \
  if (PTR) { \
    free(PTR); \
    PTR = NULL; \
  } \
}

#define UMC_ALLOC_ARR(PTR, TYPE, NUM) { \
  UMC_FREE(PTR);    \
  PTR = (TYPE*)malloc((NUM)*sizeof(TYPE)); \
  UMC_CHECK(PTR != NULL, -883); \
}

// low level routines
void    bs_create(bs_t * bs);
void    bs_delete(bs_t * bs);
Status  bs_resize(bs_t * bs, uint32_t size);
void    bs_update(bs_t * bs, uint8_t * stream);

#define bs_put_dword_macro(bs, bitbuf)\
    *(uint32_t *) (bs).bs_pos = bitbuf;



static void bs_reset(bs_t * bs)
{
    bs->bs_pos = bs->stream;
    bs->bitbuf = 0;
    bs->left = 32;
}




#endif
