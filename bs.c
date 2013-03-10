#include <string.h>

#include "bs.h"

//////////////////////////////////////////////////////////////////////////
// low level routines
void bs_create(bs_t * bs)
{
    memset(bs, 0, sizeof(bs_t));
}

void bs_delete(bs_t * bs)
{
    if (bs->size)
        UMC_FREE(bs->stream);
}

Status bs_resize(bs_t * bs, uint32_t size)
{
    if (bs->size < size)
    {
        if (bs->size)
            UMC_FREE(bs->stream);

        bs->size = size;
        UMC_ALLOC_ARR(bs->stream, uint8_t, bs->size);
    }

    bs_reset(bs);

    return 0;
}

void bs_update(bs_t * bs, uint8_t * stream)
{
    if (bs->size)
        UMC_FREE(bs->stream);

    bs->stream = stream;
    bs->size = 0;

    bs_reset(bs);
}




