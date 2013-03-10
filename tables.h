#include "types.h"
#ifndef AVCENC_TABLES_H
#define AVCENC_TABLES_H

//
// CABAC
extern const uint8_t g_bac_range_lps[128][4];
extern const uint8_t g_bac_transition[128][2];
extern const uint8_t g_bac_renorm[512];
extern const int32_t g_bac_entropy[128][2];

extern const int32_t g_log2_plus_one[256];

extern const bac_init_ctx_t ctx_mb_type[2][2][10];
extern const bac_init_ctx_t ctx_b8_type[2][4];
extern const bac_init_ctx_t ctx_mvd[2][8];
extern const bac_init_ctx_t ctx_idx[6];
extern const bac_init_ctx_t ctx_mb_aff[2][3];
extern const bac_init_ctx_t ctx_dct_size[2][3];
extern const bac_init_ctx_t ctx_delta_qp[4];
extern const bac_init_ctx_t ctx_yip[2];
extern const bac_init_ctx_t ctx_cip[4];
extern const bac_init_ctx_t ctx_cbp[2][3][4];
extern const bac_init_ctx_t ctx_bcbp[2][6][4];
extern const bac_init_ctx_t ctx_one[2][6][5];
extern const bac_init_ctx_t ctx_abs[2][6][5];
extern const bac_init_ctx_t ctx_frm_map[2][6][15];
extern const bac_init_ctx_t ctx_fld_map[2][6][15];
extern const bac_init_ctx_t ctx_frm_last[2][6][15];
extern const bac_init_ctx_t ctx_fld_last[2][6][15];

#endif
