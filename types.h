

#ifndef AVCENC_TYPES_H
#define AVCENC_TYPES_H

typedef unsigned char   Ipp8u;
typedef unsigned short  Ipp16u;
typedef unsigned int    Ipp32u;

typedef signed char    Ipp8s;
typedef signed short   Ipp16s;
typedef signed int     Ipp32s;
typedef float   Ipp32f;
typedef double  Ipp64f;


typedef enum {
    AVC_CAVLC = 0,
    AVC_CABAC
} avc_entropy_e;

typedef Ipp8s       int8_t;
typedef Ipp8u       uint8_t;
typedef Ipp16s      int16_t;
typedef Ipp16u      uint16_t;
typedef Ipp32s      int32_t;
typedef Ipp32u      uint32_t;

typedef Ipp32s Status;

    typedef uint8_t     pixel_t;

//typedef struct {
//    uint8_t     off_skip;
//    uint8_t     off_dct;
//    uint8_t     off_bcbp_dc[3];
//    uint8_t     off_bcbp_ac[3][4][4];
//    uint8_t     off_idx[2][2][2];
//    uint8_t     off_mvd[2][2][2][2];
//    avc_neib_t  neib_a[4];
//    avc_neib_t  neib_b[4];
//} cabac_off_t;

typedef struct
{
    uint8_t *   stream;
    uint8_t *   bs_pos;
    uint32_t    bitbuf;
    uint32_t    left;
    uint32_t    size;
} bs_t;

typedef struct {
    int8_t v;
    int8_t c;
} bac_init_ctx_t;

typedef uint8_t bac_ctx_t;

typedef struct {
    uint32_t    low;
    uint32_t    range;
    uint32_t    chunks;
    int32_t     left;
    uint32_t    bins;
    uint8_t *   bs_pos;
    uint8_t *   stream;
} bac_t;

typedef struct {
    bac_t       bac;
    bac_ctx_t   delta_qp[1];
} cabac_ctx_t;

typedef struct {
    
    cabac_ctx_t     cabac_ctx;

    int32_t         last_qp;
    int32_t         delta_qp;
    int32_t         prev_off;
    int32_t         bits;

} entropy_ctx_t;

#endif
