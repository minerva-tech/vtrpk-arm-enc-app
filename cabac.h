#include "types.h"
#include "tables.h"
//////////////////////////////////////////////////////////////////////////
//
// binary arithmetic coder

#define BAC_BITS        10
#define BAC_HALF        (1 << (BAC_BITS - 1))
#define BAC_QUARTER     (1 << (BAC_BITS - 2))

#define BAC_HOLD_BITS   15
#define BAC_RANGE_BITS  (8 + BAC_HOLD_BITS)
#define BAC_LOAD_BITS   8
#define BAC_LOAD_MASK   ((1 << BAC_LOAD_BITS) - 1)
#define BAC_MAX_LEFT    (BAC_LOAD_BITS + BAC_HOLD_BITS - 1)
#define BAC_MAX_BITS    (BAC_RANGE_BITS + BAC_LOAD_BITS)
#define BAC_MAX_MASK    ((uint32_t) ~0 >> (32 - BAC_MAX_BITS))

#define bac_begin(bac)\
    uint32_t low;\
	uint32_t range;\
	uint32_t chunks;\
	int32_t left;\
	uint32_t bins;\
	uint8_t * bs_pos;\
	low = (bac).low;\
    range = (bac).range;\
    chunks = (bac).chunks;\
    left = (bac).left;\
    bins = (bac).bins;\
    bs_pos = (bac).bs_pos;\
    bac_renorm

#define bac_finish(bac)\
    (bac).low = low;\
    (bac).range = range;\
    (bac).chunks = chunks;\
    (bac).left = left;\
    (bac).bins = bins;\
    (bac).bs_pos = bs_pos;

#define bac_init {\
    low = 0;\
    range = BAC_HALF - 2;\
    left = BAC_MAX_LEFT;\
    chunks = 0;\
}

#define bac_renorm {\
	uint32_t code;\
	uint32_t carry;\
    if (left < BAC_HOLD_BITS) do {\
        code = low >> BAC_RANGE_BITS;\
        low = (low << BAC_LOAD_BITS) & BAC_MAX_MASK;\
\
        if (code ^ BAC_LOAD_MASK) {\
            carry = code >> BAC_LOAD_BITS;\
            code &= BAC_LOAD_MASK;\
            bs_pos[0] += carry--;\
            if (chunks) do {\
                *++bs_pos = carry;\
            } while (--chunks);\
        }\
\
        if (code ^ BAC_LOAD_MASK)\
            *++bs_pos = code;\
        else\
            chunks++;\
\
        left += BAC_LOAD_BITS;\
    } while (left < BAC_HOLD_BITS);\
}


#define bac_bypass(sym) {\
    bins++;\
    left--;\
    low += (sym) ? range << left : 0;\
}

#define bac_align {\
    low |= 0x80 << left;\
    left -= BAC_HOLD_BITS;\
    bac_renorm\
    bs_pos += (BAC_MAX_LEFT + 2 - left) >> 3;\
}

#define bac_pcm(pcm, cnt) {\
    pixel_t * p = (pixel_t *) bs_pos;\
    for (int32_t i = 0; i < cnt; i++)\
        *++p = (pixel_t) pcm[i];\
    bs_pos = (uint8_t *) p;\
}

#define bac_written(bac)    (int32_t) ((bac.bs_pos - bac.stream) * 8 + (bac.chunks + 1) * BAC_LOAD_BITS + BAC_MAX_LEFT - bac.left);




//////////////////////////////////////////////////////////////////////////
//
// CABAC



static const int32_t c1_map[] = { 0, 2, 3, 4, 4 };
static const int32_t c2_map[] = { 1, 2, 3, 4, 4 };

static const int32_t pos2ctx_map8x8[2][64] =
{{
     0,  1,  2,  3,  4,  5,  5,  4,  4,  3,  3,  4,  4,  4,  5,  5,
     4,  4,  4,  4,  3,  3,  6,  7,  7,  7,  8,  9, 10,  9,  8,  7,
     7,  6, 11, 12, 13, 11,  6,  7,  8,  9, 14, 10,  9,  8,  6, 11,
    12, 13, 11,  6,  9, 14, 10,  9, 11, 12, 13, 11, 14, 10, 12, 14
},{
    0,  1,  1,  2,  2,  3,  3,  4,  5,  6,  7,  7,  7,  8,  4,  5,
    6,  9, 10, 10,  8, 11, 12, 11,  9,  9, 10, 10,  8, 11, 12, 11,
    9,  9, 10, 10,  8, 11, 12, 11,  9,  9, 10, 10,  8, 13, 13,  9,
    9, 10, 10,  8, 13, 13,  9,  9, 10, 10, 14, 14, 14, 14, 14, 14
}};

static const int32_t pos2ctx_last8x8[64] =
{
    0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,
    5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8
};



//////////////////////////////////////////////////////////////////////////
//
// initialization

static void cabac_ctx_init(cabac_ctx_t * ctx)
{
    int32_t inter = 0;
	uint8_t my_qp=16;


#define INIT_CTX_TABLE1(ctx, init_ctx, qp) {\
	int32_t i;\
	int32_t c;\
    for ( i = 0; i < sizeof(init_ctx) / sizeof(init_ctx[0]); i++) {\
        c = AVCENC_RANGE((init_ctx[i].v * qp >> 4) + init_ctx[i].c, 1, 126);\
        ctx[i] = ((c & 63) << 1) | (c >> 6);\
    }\
}

#define INIT_CTX_TABLE2(ctx, init_ctx, qp) {\
	int32_t j;\
    for ( j = 0; j < sizeof(init_ctx) / sizeof(init_ctx[0]); j++)\
        INIT_CTX_TABLE1(ctx[j], init_ctx[j], qp)\
}

    INIT_CTX_TABLE1(ctx->delta_qp,  ctx_delta_qp,       my_qp);

#undef INIT_CTX_TABLE1
#undef INIT_CTX_TABLE2
}

static void cabac_init_encoding(entropy_ctx_t * entropy_ctx, bs_t * bs)
{
	uint32_t low;
	uint32_t range;
	uint32_t chunks;
	int32_t left;
	uint32_t bins;
	uint8_t * bs_pos;

    entropy_ctx->cabac_ctx.bac.stream = (uint8_t *) bs->bs_pos;
    entropy_ctx->cabac_ctx.bac.bs_pos = entropy_ctx->cabac_ctx.bac.stream - 1;
    entropy_ctx->cabac_ctx.bac.bins = 0;
    entropy_ctx->cabac_ctx.bac.left = 32; // to skip uninitialized renormalization loop

    //bac_begin(entropy_ctx->cabac_ctx.bac)
	
	low = entropy_ctx->cabac_ctx.bac.low;
    range = entropy_ctx->cabac_ctx.bac.range;
    chunks = entropy_ctx->cabac_ctx.bac.chunks;
    left = entropy_ctx->cabac_ctx.bac.left;
    bins = entropy_ctx->cabac_ctx.bac.bins;
    bs_pos = entropy_ctx->cabac_ctx.bac.bs_pos;
    bac_renorm
    bac_init
    bac_finish(entropy_ctx->cabac_ctx.bac)
}

static void cabac_done_encoding(entropy_ctx_t * entropy_ctx, bs_t * bs)
{
    //bac_begin(entropy_ctx->cabac_ctx.bac)
	uint32_t low;
	uint32_t range;
	uint32_t chunks;
	int32_t left;
	uint32_t bins;
	uint8_t * bs_pos;
	bac_ctx_t tmp = 0;
	low = entropy_ctx->cabac_ctx.bac.low;
    range = entropy_ctx->cabac_ctx.bac.range;
    chunks = entropy_ctx->cabac_ctx.bac.chunks;
    left = entropy_ctx->cabac_ctx.bac.left;
    bins = entropy_ctx->cabac_ctx.bac.bins;
    bs_pos = entropy_ctx->cabac_ctx.bac.bs_pos;
    
	bac_renorm
    bac_align
    bac_finish(entropy_ctx->cabac_ctx.bac)

    bs->bs_pos = entropy_ctx->cabac_ctx.bac.bs_pos;

}

static void cabac_write_mode(entropy_ctx_t * entropy_ctx, uint8_t sym)
{
	
	bac_ctx_t tmp = (entropy_ctx->cabac_ctx).delta_qp[0];

    //bac_begin(entropy_ctx->cabac_ctx.bac)
	uint32_t low;
	uint32_t range;
	uint32_t chunks;
	int32_t left;
	uint32_t bins;
	uint8_t * bs_pos;
	low = (entropy_ctx->cabac_ctx.bac).low;
    range = (entropy_ctx->cabac_ctx.bac).range;
    chunks = (entropy_ctx->cabac_ctx.bac).chunks;
    left = (entropy_ctx->cabac_ctx.bac).left;
    bins = (entropy_ctx->cabac_ctx.bac).bins;
    bs_pos = (entropy_ctx->cabac_ctx.bac).bs_pos;
    //bac_renorm
	{
	uint32_t code;
	uint32_t carry;
    if (left < BAC_HOLD_BITS) do {
        code = low >> BAC_RANGE_BITS;
        low = (low << BAC_LOAD_BITS) & BAC_MAX_MASK;

        if (code ^ BAC_LOAD_MASK) {
            carry = code >> BAC_LOAD_BITS;
            code &= BAC_LOAD_MASK;
            bs_pos[0] += carry--;
            if (chunks) do {
                *++bs_pos = carry;
            } while (--chunks);
        }

        if (code ^ BAC_LOAD_MASK)
            *++bs_pos = code;
        else
            chunks++;

        left += BAC_LOAD_BITS;
    } while (left < BAC_HOLD_BITS);
}
    
    //bac_decision(sym, tmp)
{
    uint32_t v;
    uint32_t old_ctx;
	uint8_t range_lps;
	uint8_t renorm;
	
	v = sym;
	old_ctx = tmp;
	range_lps = g_bac_range_lps[old_ctx][(range >> 6) - 4];
	(entropy_ctx->cabac_ctx).delta_qp[0] = g_bac_transition[old_ctx][v];

    range -= range_lps;

    if (v != (old_ctx & 1)) {
        low += range << left;
        range = range_lps;
    }

    renorm = g_bac_renorm[range];
    range <<= renorm;
    left -= renorm;
    bins++;
}
	//bac_renorm
{
	uint32_t code;
	uint32_t carry;
    if (left < BAC_HOLD_BITS) do {
        code = low >> BAC_RANGE_BITS;
        low = (low << BAC_LOAD_BITS) & BAC_MAX_MASK;

        if (code ^ BAC_LOAD_MASK) {
            carry = code >> BAC_LOAD_BITS;
            code &= BAC_LOAD_MASK;
            bs_pos[0] += carry--;
            if (chunks) do {
                *++bs_pos = carry;
            } while (--chunks);
        }

        if (code ^ BAC_LOAD_MASK)
            *++bs_pos = code;
        else
            chunks++;

        left += BAC_LOAD_BITS;
    } while (left < BAC_HOLD_BITS);
}
    //bac_finish(entropy_ctx->cabac_ctx.bac)
	(entropy_ctx->cabac_ctx.bac).low = low;
    (entropy_ctx->cabac_ctx.bac).range = range;
    (entropy_ctx->cabac_ctx.bac).chunks = chunks;
    (entropy_ctx->cabac_ctx.bac).left = left;
    (entropy_ctx->cabac_ctx.bac).bins = bins;
    (entropy_ctx->cabac_ctx.bac).bs_pos = bs_pos;
}

int encode_frame(uint8_t *inp, bs_t *output, uint32_t fsize)
{
	entropy_ctx_t           entropy_ctx;
	uint16_t k;
	uint16_t shift;
	uint8_t sym;
	uint32_t i;
	
	*output->bs_pos++ = 0xAA;

	cabac_ctx_init(&entropy_ctx.cabac_ctx);
	cabac_init_encoding(&entropy_ctx, output);

	for(i=0; i<fsize*8; i++) { // i+=2
		k = i>>3;
		shift = i & 0x07;
		sym = (inp[k] & (0x01<<shift)) ? 1:0;
		cabac_write_mode(&entropy_ctx, sym);
	}

	cabac_done_encoding(&entropy_ctx, output);
	return (int32_t)output->bs_pos - (int32_t)output->stream;
}