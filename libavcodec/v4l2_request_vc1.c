/*
 * V4L2 Request API VC-1/WMV3 hwaccel
 *
 * Based on the VC-1 hwaccel of Jernej Skrabec's vc1-v3 branch, adapted to the
 * split VC-1 stateless controls (sequence, entry-point header, picture layer,
 * bitplanes, slice parameters), with an explicit description of the reference
 * pictures.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "config_components.h"

#include "libavutil/intreadwrite.h"

#include "hwaccel_internal.h"
#include "hwconfig.h"
#include "internal.h"
#include "v4l2_request.h"
#include "v4l2-stateless-vc1.h"
#include "vc1.h"
#include "vc1_common.h"
#include "vc1data.h"

/* The controls use the numbering of the decoder for these enumerations. */
_Static_assert(MV_PMODE_1MV_HPEL_BILIN == V4L2_VC1_MVMODE_1MV_HPEL_BILIN &&
               MV_PMODE_1MV            == V4L2_VC1_MVMODE_1MV            &&
               MV_PMODE_1MV_HPEL       == V4L2_VC1_MVMODE_1MV_HPEL       &&
               MV_PMODE_MIXED_MV       == V4L2_VC1_MVMODE_MIXED_MV       &&
               MV_PMODE_INTENSITY_COMP == V4L2_VC1_MVMODE_INTENSITY_COMP,
               "MVMODE numbering");
_Static_assert(PROGRESSIVE == V4L2_VC1_FCM_PROGRESSIVE &&
               ILACE_FRAME == V4L2_VC1_FCM_FRAME_INTERLACE &&
               ILACE_FIELD == V4L2_VC1_FCM_FIELD_INTERLACE, "FCM numbering");
_Static_assert(CONDOVER_NONE == V4L2_VC1_CONDOVER_NONE &&
               CONDOVER_ALL == V4L2_VC1_CONDOVER_ALL &&
               CONDOVER_SELECT == V4L2_VC1_CONDOVER_SELECT, "CONDOVER numbering");
_Static_assert(DQPROFILE_FOUR_EDGES == V4L2_VC1_DQPROFILE_ALL_FOUR_EDGES &&
               DQPROFILE_DOUBLE_EDGES == V4L2_VC1_DQPROFILE_DOUBLE_EDGES &&
               DQPROFILE_SINGLE_EDGE == V4L2_VC1_DQPROFILE_SINGLE_EDGE &&
               DQPROFILE_ALL_MBS == V4L2_VC1_DQPROFILE_ALL_MBS, "DQPROFILE numbering");

/* One slice per macroblock row at most; SLICE_ADDR is a 9-bit row address. */
#define VC1_MAX_SLICES 256

typedef struct V4L2RequestContextVC1 {
    V4L2RequestContext base;
    /* Size of the SLICE_PARAMS array of the driver, 0 if it has no such control. */
    unsigned int max_slice_params;
} V4L2RequestContextVC1;

/* Intensity compensations in effect on one field, in order of application */
typedef struct VC1IntCompList {
    uint8_t nb;
    struct v4l2_vc1_intcomp ic[V4L2_VC1_REFERENCE_NUM_INTCOMP + 1];
} VC1IntCompList;

/*
 * What a later picture has to say about this picture when it uses it as a
 * reference (struct v4l2_vc1_reference). Arrays are indexed by field parity:
 * 0 top, 1 bottom.
 */
typedef struct VC1ReferenceState {
    uint8_t fcm;
    uint8_t ptype[2];
    uint8_t refdist;
    uint8_t rangeredfrm;
    uint8_t tff;
    /* applied to a field of this frame by the second field of this frame */
    VC1IntCompList self_ic[2];
    /* applied by this picture to the fields of its forward reference */
    VC1IntCompList fwd_ic[2];
} VC1ReferenceState;

typedef struct V4L2RequestControlsVC1 {
    V4L2RequestPictureContext pic;
    VC1ReferenceState state;
    int skipped;
    struct v4l2_ctrl_vc1_sequence sequence;
    struct v4l2_ctrl_vc1_entrypoint_header entrypoint;
    struct v4l2_ctrl_vc1_picture_layer picture;
    struct v4l2_ctrl_vc1_bitplanes bitplanes;
    struct v4l2_ctrl_vc1_slice_params slices[VC1_MAX_SLICES];
    unsigned int nb_slices;
} V4L2RequestControlsVC1;

static inline int vc1_is_p(const VC1Context *v)
{
    return v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped;
}

static inline int vc1_is_b(const VC1Context *v)
{
    return v->s.pict_type == AV_PICTURE_TYPE_B && !v->bi_type;
}

static inline int vc1_is_i_or_bi(const VC1Context *v)
{
    return v->s.pict_type == AV_PICTURE_TYPE_I ||
           (v->s.pict_type == AV_PICTURE_TYPE_B && v->bi_type);
}

static int vc1_get_ptype(const VC1Context *v)
{
    switch (v->s.pict_type) {
    case AV_PICTURE_TYPE_I:
        return V4L2_VC1_PICTURE_TYPE_I;
    case AV_PICTURE_TYPE_P:
        return V4L2_VC1_PICTURE_TYPE_P;
    case AV_PICTURE_TYPE_B:
        return v->bi_type ? V4L2_VC1_PICTURE_TYPE_BI : V4L2_VC1_PICTURE_TYPE_B;
    }
    return V4L2_VC1_PICTURE_TYPE_I;
}

/* MVMODE only exists in progressive and field P/B pictures. */
static int vc1_has_mvmode(const VC1Context *v)
{
    return v->fcm != ILACE_FRAME && (vc1_is_p(v) || vc1_is_b(v));
}

/* Intensity compensation signalled through MVMODE (progressive and field P). */
static int vc1_has_mvmode_intcomp(const VC1Context *v)
{
    return v->fcm != ILACE_FRAME && vc1_is_p(v) &&
           v->mv_mode == MV_PMODE_INTENSITY_COMP;
}

/* The MV mode in effect, used to tell which tables and bitplanes exist. */
static int vc1_effective_mvmode(const VC1Context *v)
{
    return vc1_has_mvmode_intcomp(v) ? v->mv_mode2 : v->mv_mode;
}

/*
 * The decoder keeps the intensity compensation parameters per reference field:
 * (lumscale, lumshift) for the top field and (lumscale2, lumshift2) for the
 * bottom field, with intcompfield as a bit mask (1 = top, 2 = bottom). The
 * control wants the syntax elements: LUMSCALE1/LUMSHIFT1 is the first pair of
 * the bitstream, which applies to the bottom field when INTCOMPFIELD selects
 * the bottom field only.
 */
static void vc1_fill_intcomp(const VC1Context *v,
                             struct v4l2_ctrl_vc1_picture_layer *pic)
{
    if (!vc1_is_p(v))
        return;

    if (v->fcm == ILACE_FRAME) {
        if (v->intcomp) {
            pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_INTCOMP;
            pic->lumscale = v->lumscale;
            pic->lumshift = v->lumshift;
        }
        return;
    }

    if (v->mv_mode != MV_PMODE_INTENSITY_COMP)
        return;

    if (v->fcm == PROGRESSIVE) {
        pic->lumscale = v->lumscale;
        pic->lumshift = v->lumshift;
        return;
    }

    switch (v->intcompfield) {
    case 1:
        pic->intcompfield = V4L2_VC1_INTCOMPFIELD_TOP;
        pic->lumscale = v->lumscale;
        pic->lumshift = v->lumshift;
        break;
    case 2:
        pic->intcompfield = V4L2_VC1_INTCOMPFIELD_BOTTOM;
        pic->lumscale = v->lumscale2;
        pic->lumshift = v->lumshift2;
        break;
    default:
        pic->intcompfield = V4L2_VC1_INTCOMPFIELD_BOTH;
        pic->lumscale  = v->lumscale;
        pic->lumshift  = v->lumshift;
        pic->lumscale2 = v->lumscale2;
        pic->lumshift2 = v->lumshift2;
        break;
    }
}

static int vc1_get_ttfrm(const VC1Context *v)
{
    switch (v->ttfrm) {
    case TT_8X8: return V4L2_VC1_TTFRM_8X8;
    case TT_8X4: return V4L2_VC1_TTFRM_8X4;
    case TT_4X8: return V4L2_VC1_TTFRM_4X8;
    case TT_4X4: return V4L2_VC1_TTFRM_4X4;
    }
    return V4L2_VC1_TTFRM_8X8;
}

/*
 * Presence of the bitplanes in the picture header, as in vaapi_vc1.c. A
 * bitplane which is present is either raw coded (its bits are in the
 * macroblock layer) or decoded by vc1.c and passed through the control.
 */
static int vc1_bitplanes_present(const VC1Context *v)
{
    int flags = 0;

    if (vc1_is_i_or_bi(v)) {
        if (v->profile == PROFILE_ADVANCED) {
            flags |= V4L2_VC1_BITPLANE_FLAG_ACPRED;
            if (v->overlap && v->pq <= 8 && v->condover == CONDOVER_SELECT)
                flags |= V4L2_VC1_BITPLANE_FLAG_OVERFLAGS;
            if (v->fcm == ILACE_FRAME)
                flags |= V4L2_VC1_BITPLANE_FLAG_FIELDTX;
        }
    } else if (vc1_is_p(v)) {
        if (v->fcm == PROGRESSIVE &&
            vc1_effective_mvmode(v) == MV_PMODE_MIXED_MV)
            flags |= V4L2_VC1_BITPLANE_FLAG_MVTYPEMB;
        if (v->fcm != ILACE_FIELD)
            flags |= V4L2_VC1_BITPLANE_FLAG_SKIPMB;
    } else if (vc1_is_b(v)) {
        if (v->fcm == ILACE_FIELD)
            flags |= V4L2_VC1_BITPLANE_FLAG_FORWARDMB;
        else
            flags |= V4L2_VC1_BITPLANE_FLAG_DIRECTMB |
                     V4L2_VC1_BITPLANE_FLAG_SKIPMB;
    }

    return flags;
}

static int vc1_bitplanes_raw(const VC1Context *v)
{
    int flags = 0;

    if (v->mv_type_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_MVTYPEMB;
    if (v->dmb_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_DIRECTMB;
    if (v->skip_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_SKIPMB;
    if (v->fieldtx_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_FIELDTX;
    if (v->fmb_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_FORWARDMB;
    if (v->acpred_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_ACPRED;
    if (v->overflg_is_raw)
        flags |= V4L2_VC1_RAW_CODING_FLAG_OVERFLAGS;

    return flags;
}

/* One bit per macroblock, raster order, LSB first, no row padding. */
static void vc1_pack_bitplane(uint8_t *dst, const uint8_t *src,
                              const VC1Context *v)
{
    const MpegEncContext *s = &v->s;
    int mb_height = s->mb_height >> v->field_mode;
    int mb_width = s->mb_width;
    int n = 0;

    /* RESPIC: Simple/Main picture coded at half width and/or height */
    if (v->profile < PROFILE_ADVANCED && v->multires) {
        if (v->respic & 1)
            mb_width  = (s->width  / 2 + 15) >> 4;
        if (v->respic & 2)
            mb_height = (s->height / 2 + 15) >> 4;
    }

    memset(dst, 0, V4L2_VC1_BITPLANE_SIZE);

    for (int y = 0; y < mb_height; y++)
        for (int x = 0; x < mb_width; x++, n++)
            dst[n >> 3] |= (src[y * s->mb_stride + x] & 1) << (n & 7);
}

/*
 * Number of emulation prevention bytes in front of the byte which holds bit
 * `bit_offset` of the unescaped payload (same rule as vc1_unescape_buffer()).
 */
static int vc1_count_emulation_bytes(const uint8_t *buf, int size,
                                     int bit_offset)
{
    int target = bit_offset >> 3, unescaped = 0, count = 0;

    for (int i = 0; i < size; i++) {
        if (i >= 2 && buf[i] == 3 && !buf[i - 1] && !buf[i - 2] &&
            i < size - 1 && buf[i + 1] < 4) {
            count++;
            continue;
        }
        if (unescaped == target)
            break;
        unescaped++;
    }

    return count;
}

static void vc1_fill_sequence(const VC1Context *v,
                              struct v4l2_ctrl_vc1_sequence *seq)
{
    const AVCodecContext *avctx = v->s.avctx;
    int advanced = v->profile == PROFILE_ADVANCED;

    *seq = (struct v4l2_ctrl_vc1_sequence) {
        .profile          = v->profile,
        .level            = advanced ? v->level : 0,
        .colordiff_format = 1,
        .maxbframes       = advanced ? 0 : avctx->max_b_frames,
        .frmrtq_postproc  = v->frmrtq_postproc,
        .bitrtq_postproc  = v->bitrtq_postproc,
        .max_coded_width  = advanced ? v->max_coded_width  : avctx->coded_width,
        .max_coded_height = advanced ? v->max_coded_height : avctx->coded_height,
    };

    if (v->finterpflag)
        seq->flags |= V4L2_VC1_SEQUENCE_FLAG_FINTERPFLAG;

    if (advanced) {
        if (v->broadcast)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_PULLDOWN;
        if (v->interlace)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_INTERLACE;
        if (v->tfcntrflag)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_TFCNTRFLAG;
        if (v->psf)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_PSF;
        if (v->postprocflag)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_POSTPROCFLAG;
    } else {
        if (v->res_rtm_flag)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_RES_RTM;
        if (v->multires)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_MULTIRES;
        if (v->resync_marker)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_SYNCMARKER;
        if (v->rangered)
            seq->flags |= V4L2_VC1_SEQUENCE_FLAG_RANGERED;
    }
}

static void vc1_fill_entrypoint(const VC1Context *v,
                                struct v4l2_ctrl_vc1_entrypoint_header *ep)
{
    const AVCodecContext *avctx = v->s.avctx;
    int advanced = v->profile == PROFILE_ADVANCED;

    *ep = (struct v4l2_ctrl_vc1_entrypoint_header) {
        .dquant       = v->dquant,
        .quantizer    = v->quantizer_mode,
        .coded_width  = avctx->coded_width,
        .coded_height = avctx->coded_height,
    };

    /* Common to STRUCT_C and to the entry-point header. */
    if (v->loop_filter)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_LOOPFILTER;
    if (v->fastuvmc)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_FASTUVMC;
    if (v->extended_mv)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_EXTENDED_MV;
    if (v->vstransform)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_VSTRANSFORM;
    if (v->overlap)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_OVERLAP;

    if (!advanced)
        return;

    if (v->broken_link)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_BROKEN_LINK;
    if (v->closed_entry)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_CLOSED_ENTRY;
    if (v->panscanflag)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_PANSCAN;
    if (v->refdist_flag)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_REFDIST;
    if (v->extended_mv && v->extended_dmv)
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_EXTENDED_DMV;
    if (v->range_mapy_flag) {
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPY;
        ep->range_mapy = v->range_mapy;
    }
    if (v->range_mapuv_flag) {
        ep->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPUV;
        ep->range_mapuv = v->range_mapuv;
    }
}

static void vc1_fill_vopdquant(const VC1Context *v,
                               struct v4l2_vc1_vopdquant *dq)
{
    /* VOPDQUANT: P and B pictures, and I/BI pictures of the Advanced profile */
    if (!v->dquant ||
        (vc1_is_i_or_bi(v) && v->profile != PROFILE_ADVANCED))
        return;

    if (v->dquant == 2) {
        /* Only PQDIFF/ABSPQ are coded: ALTPQUANT applies to the four edges. */
        dq->flags     = V4L2_VC1_VOPDQUANT_FLAG_DQUANTFRM;
        dq->dqprofile = V4L2_VC1_DQPROFILE_ALL_FOUR_EDGES;
        dq->altpquant = v->altpq;
        return;
    }

    if (!v->dquantfrm)
        return;

    dq->flags     = V4L2_VC1_VOPDQUANT_FLAG_DQUANTFRM;
    dq->dqprofile = v->dqprofile;
    dq->altpquant = v->altpq;

    switch (v->dqprofile) {
    case DQPROFILE_SINGLE_EDGE:
        dq->dqsbedge = v->dqsbedge;
        break;
    case DQPROFILE_DOUBLE_EDGES:
        dq->dqdbedge = v->dqsbedge;
        break;
    case DQPROFILE_ALL_MBS:
        if (v->dqbilevel)
            dq->flags |= V4L2_VC1_VOPDQUANT_FLAG_DQBILEVEL;
        else
            dq->altpquant = 0; /* no PQDIFF: MQUANT is coded per macroblock */
        break;
    }
}

static void vc1_fill_picture(const VC1Context *v,
                             struct v4l2_ctrl_vc1_picture_layer *pic)
{
    const MpegEncContext *s = &v->s;
    int advanced = v->profile == PROFILE_ADVANCED;
    int inter = vc1_is_p(v) || vc1_is_b(v);
    int interlaced = v->fcm != PROGRESSIVE;
    int has_tff = advanced && v->broadcast && v->interlace && !v->psf;

    *pic = (struct v4l2_ctrl_vc1_picture_layer) {
        .ptype   = vc1_get_ptype(v),
        .fptype  = v->field_mode ? v->fptype : 0,
        .fcm     = v->fcm,
        /* B fields do not code REFDIST: it is in the backward reference */
        .refdist = v->field_mode && s->pict_type != AV_PICTURE_TYPE_B ?
                   v->refdist : 0,
        .rptfrm  = advanced && v->broadcast && !has_tff ? v->rptfrm : 0,
    };

    /* Reference pictures, as for MPEG-2. */
    switch (s->pict_type) {
    case AV_PICTURE_TYPE_B:
        if (s->next_pic.ptr)
            pic->backward_ref_ts =
                ff_v4l2_request_get_capture_timestamp(s->next_pic.ptr->f);
        /* fall through */
    case AV_PICTURE_TYPE_P:
        if (s->last_pic.ptr)
            pic->forward_ref_ts =
                ff_v4l2_request_get_capture_timestamp(s->last_pic.ptr->f);
    }

    /* The top field is first unless TFF is coded and says otherwise. */
    if (!has_tff || v->tff)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_TFF;
    if (has_tff && v->rff)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_RFF;
    if (v->second_field)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_SECOND_FIELD;

    pic->pqindex = v->pqindex;
    pic->pquant  = v->pq;
    pic->transacfrm  = v->c_ac_table_index;
    pic->transacfrm2 = vc1_is_i_or_bi(v) ? v->y_ac_table_index : 0;
    pic->postproc = advanced && v->postprocflag ? v->postproc : 0;
    pic->respic   = !advanced && v->multires ? v->respic : 0;
    pic->condover = advanced && vc1_is_i_or_bi(v) ? v->condover : 0;

    if (s->pict_type == AV_PICTURE_TYPE_B && (vc1_is_b(v) || v->field_mode) &&
        v->bfraction_lut_index < V4L2_VC1_BFRACTION_NUM)
        pic->bfraction = v->bfraction_lut_index;

    if (v->rangered && v->rangeredfrm)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_RANGEREDFRM;
    if (v->halfpq)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_HALFQP;
    if (v->pquantizer)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_PQUANTIZER;
    if (v->dc_table_index)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_TRANSDCTAB;
    if (v->rnd)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_RNDCTRL;
    if (v->finterpflag && v->interpfrm)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_INTERPFRM;
    if (advanced && v->interlace && v->uvsamp)
        pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_UVSAMP;

    /* MVRANGE: every Simple/Main profile picture, Advanced profile P and B */
    if (v->extended_mv && (inter || !advanced))
        pic->mvrange = v->mvrange;

    if (inter) {
        if (vc1_has_mvmode(v)) {
            pic->mvmode = v->mv_mode;
            if (vc1_has_mvmode_intcomp(v))
                pic->mvmode2 = v->mv_mode2;
        }
        vc1_fill_intcomp(v, pic);

        /* TTMBF = 1 with an 8x8 TTFRM stands for VSTRANSFORM = 0. */
        if (v->ttmbf) {
            pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_TTMBF;
            pic->ttfrm = vc1_get_ttfrm(v);
        }

        if (!interlaced) {
            pic->mvtab  = v->mv_table_index;
            pic->cbptab = v->cbptab;
        } else {
            pic->dmvrange  = v->extended_mv && v->extended_dmv ? v->dmvrange : 0;
            pic->mbmodetab = v->mbmodetab;
            pic->imvtab    = v->imvtab;
            pic->icbptab   = v->icbptab;
        }

        if (v->fcm == ILACE_FRAME) {
            pic->twomvbptab = v->twomvbptab;
            if (vc1_is_b(v) || v->fourmvswitch)
                pic->fourmvbptab = v->fourmvbptab;
            if (vc1_is_p(v) && v->fourmvswitch)
                pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_4MVSWITCH;
        } else if (v->fcm == ILACE_FIELD) {
            if (vc1_effective_mvmode(v) == MV_PMODE_MIXED_MV)
                pic->fourmvbptab = v->fourmvbptab;
            /* B field pictures always use two reference fields. */
            if (v->numref || vc1_is_b(v))
                pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_NUMREF;
            else if (v->reffield)
                pic->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_REFFIELD;
        }
    }

    vc1_fill_vopdquant(v, &pic->vopdquant);

    pic->bitplane_flags   = vc1_bitplanes_present(v);
    pic->raw_coding_flags = vc1_bitplanes_raw(v) & pic->bitplane_flags;
    pic->bitplane_flags  &= ~pic->raw_coding_flags;
}

static void vc1_fill_bitplanes(const VC1Context *v, int flags,
                               struct v4l2_ctrl_vc1_bitplanes *bp)
{
    if (flags & V4L2_VC1_BITPLANE_FLAG_MVTYPEMB)
        vc1_pack_bitplane(bp->mvtypemb, v->mv_type_mb_plane, v);
    if (flags & V4L2_VC1_BITPLANE_FLAG_DIRECTMB)
        vc1_pack_bitplane(bp->directmb, v->direct_mb_plane, v);
    if (flags & V4L2_VC1_BITPLANE_FLAG_SKIPMB)
        vc1_pack_bitplane(bp->skipmb, v->s.mbskip_table, v);
    if (flags & V4L2_VC1_BITPLANE_FLAG_FIELDTX)
        vc1_pack_bitplane(bp->fieldtx, v->fieldtx_plane, v);
    if (flags & V4L2_VC1_BITPLANE_FLAG_FORWARDMB)
        vc1_pack_bitplane(bp->forwardmb, v->forward_mb_plane, v);
    if (flags & V4L2_VC1_BITPLANE_FLAG_ACPRED)
        vc1_pack_bitplane(bp->acpred, v->acpred_plane, v);
    if (flags & V4L2_VC1_BITPLANE_FLAG_OVERFLAGS)
        vc1_pack_bitplane(bp->overflags, v->over_flags_plane, v);
}

/*
 * Reference state.
 *
 * The decoding of a picture depends on state of its references which is not in
 * its own header. The decoder has it in a form which suits software (look-up
 * tables for the intensity compensation); the controls want it as the syntax
 * elements which produced it, so it is tracked here, per picture, next to the
 * controls of the picture.
 */

static void vc1_ic_add(VC1IntCompList *list, int lumscale, int lumshift)
{
    if (list->nb < FF_ARRAY_ELEMS(list->ic))
        list->ic[list->nb++] = (struct v4l2_vc1_intcomp) { lumscale, lumshift };
}

static void vc1_ic_append(VC1IntCompList *list, const VC1IntCompList *src)
{
    for (int i = 0; i < src->nb; i++)
        vc1_ic_add(list, src->ic[i].lumscale, src->ic[i].lumshift);
}

static const VC1ReferenceState *vc1_reference_state(const MPVWorkPicture *pic)
{
    static const VC1ReferenceState none;
    const V4L2RequestControlsVC1 *controls;

    if (!pic->ptr || !pic->ptr->hwaccel_picture_private)
        return &none;

    controls = pic->ptr->hwaccel_picture_private;
    return &controls->state;
}

static int vc1_fill_reference(struct v4l2_vc1_reference *ref,
                              const VC1ReferenceState *st,
                              const VC1IntCompList ic[2])
{
    *ref = (struct v4l2_vc1_reference) {
        .fcm     = st->fcm,
        .ptype   = { st->ptype[0], st->ptype[1] },
        .refdist = st->refdist,
    };

    if (st->rangeredfrm)
        ref->flags |= V4L2_VC1_REFERENCE_FLAG_RANGEREDFRM;
    if (st->tff)
        ref->flags |= V4L2_VC1_REFERENCE_FLAG_TFF;

    for (int field = 0; field < 2; field++) {
        /* More than the controls can describe: cannot happen in a stream
         * which keeps its field order, see FFMPEG.md. */
        if (ic[field].nb > V4L2_VC1_REFERENCE_NUM_INTCOMP)
            return AVERROR_PATCHWELCOME;

        ref->num_intcomp[field] = ic[field].nb;
        for (int i = 0; i < ic[field].nb; i++)
            ref->intcomp[field][i] = ic[field].ic[i];
    }

    return 0;
}

/*
 * Describe the references of the current picture. Must be called before
 * vc1_update_state(): the intensity compensation which the current picture
 * signals is not part of the description.
 */
static int vc1_fill_references(const VC1Context *v,
                               const VC1ReferenceState *cur,
                               struct v4l2_ctrl_vc1_picture_layer *pic)
{
    const MpegEncContext *s = &v->s;
    const VC1ReferenceState *fwd = vc1_reference_state(&s->last_pic);
    const VC1ReferenceState *bwd = vc1_reference_state(&s->next_pic);
    VC1IntCompList ic[2];
    int ret;

    if (s->pict_type != AV_PICTURE_TYPE_P && s->pict_type != AV_PICTURE_TYPE_B)
        return 0;

    /*
     * Forward reference: what its own second field did to its first field,
     * then what the P picture which followed it did to it. For a P picture
     * that is the first field of the picture itself (when this is the second
     * field); for a B picture it is the backward reference, decoded earlier.
     */
    for (int field = 0; field < 2; field++) {
        ic[field] = fwd->self_ic[field];
        if (s->pict_type == AV_PICTURE_TYPE_B)
            vc1_ic_append(&ic[field], &bwd->fwd_ic[field]);
        else if (v->second_field)
            vc1_ic_append(&ic[field], &cur->fwd_ic[field]);
    }
    ret = vc1_fill_reference(&pic->forward_ref, fwd, ic);
    if (ret)
        return ret;

    if (s->pict_type == AV_PICTURE_TYPE_B)
        ret = vc1_fill_reference(&pic->backward_ref, bwd, bwd->self_ic);

    return ret;
}

/* Record what later pictures will have to say about the current one. */
static void vc1_update_state(const VC1Context *v, VC1ReferenceState *st)
{
    int has_tff = v->profile == PROFILE_ADVANCED && v->broadcast &&
                  v->interlace && !v->psf;
    int ptype = vc1_get_ptype(v);

    if (!v->second_field)
        memset(st, 0, sizeof(*st));

    st->fcm         = v->fcm;
    st->refdist     = v->field_mode ? v->refdist : 0;
    st->rangeredfrm = v->rangered && v->rangeredfrm;
    st->tff         = !has_tff || v->tff;

    if (v->field_mode)
        st->ptype[v->cur_field_type] = ptype;
    else
        st->ptype[0] = st->ptype[1] = ptype;

    if (!vc1_is_p(v))
        return;

    if (v->fcm == ILACE_FRAME ? v->intcomp :
                                v->mv_mode == MV_PMODE_INTENSITY_COMP) {
        /*
         * As in ff_vc1_parse_frame_header_adv(): (lumscale, lumshift) is for
         * the top reference field, (lumscale2, lumshift2) for the bottom one.
         * The second field of a frame refers to the first field of the frame
         * and to the field of its own parity of the forward reference.
         */
        int mask = v->field_mode ? v->intcompfield : 3;
        int lumscale2 = v->field_mode ? v->lumscale2 : v->lumscale;
        int lumshift2 = v->field_mode ? v->lumshift2 : v->lumshift;

        if (!v->second_field) {
            if (mask & 1)
                vc1_ic_add(&st->fwd_ic[0], v->lumscale, v->lumshift);
            if (mask & 2)
                vc1_ic_add(&st->fwd_ic[1], lumscale2, lumshift2);
        } else if (v->cur_field_type) {
            if (mask & 1)
                vc1_ic_add(&st->self_ic[0], v->lumscale, v->lumshift);
            if (mask & 2)
                vc1_ic_add(&st->fwd_ic[1], lumscale2, lumshift2);
        } else {
            if (mask & 2)
                vc1_ic_add(&st->self_ic[1], lumscale2, lumshift2);
            if (mask & 1)
                vc1_ic_add(&st->fwd_ic[0], v->lumscale, v->lumshift);
        }
    }
}

/*
 * The capture buffer holds the picture as decoded. Range expansion (Main
 * profile) and range mapping (Advanced profile) are output processes which
 * nothing applies to a DRM PRIME frame yet: leave such streams to software.
 */
static int vc1_needs_output_process(const VC1Context *v)
{
    if (v->profile == PROFILE_ADVANCED)
        return v->range_mapy_flag || v->range_mapuv_flag;

    return v->rangered;
}

/*
 * Pre-release WMV9 bitstreams with the X8 intra frames or with the earlier
 * transform (reserved bits of STRUCT_C, which SMPTE 421M fixes) are a
 * different bitstream, which the controls do not describe.
 */
static int vc1_is_prerelease(const VC1Context *v)
{
    return v->profile != PROFILE_ADVANCED && (v->res_x8 || !v->res_fasttx);
}

/*
 * A skipped picture has no macroblock layer and is not queued: the decoded
 * picture is its reference picture. Make the current frame share the buffers
 * of the reference frame, so that it is output again and so that the pictures
 * which refer to the skipped picture are given the capture buffer which holds
 * its content.
 */
static int v4l2_request_vc1_repeat_reference(AVCodecContext *avctx)
{
    VC1Context *v = avctx->priv_data;
    MpegEncContext *s = &v->s;
    V4L2RequestControlsVC1 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    const VC1ReferenceState *ref = vc1_reference_state(&s->last_pic);
    AVFrame *cur = s->cur_pic.ptr->f, *last;
    int ret;

    if (!s->last_pic.ptr)
        return AVERROR_INVALIDDATA;
    last = s->last_pic.ptr->f;

    for (int i = 0; i < FF_ARRAY_ELEMS(cur->buf); i++) {
        ret = av_buffer_replace(&cur->buf[i], last->buf[i]);
        if (ret < 0)
            return ret;
    }
    for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        cur->data[i]     = last->data[i];
        cur->linesize[i] = last->linesize[i];
    }
    for (int i = 0; i < MPV_MAX_PLANES; i++) {
        s->cur_pic.data[i]     = cur->data[i];
        s->cur_pic.linesize[i] = cur->linesize[i];
    }

    /*
     * Nothing is queued for this picture. This only makes the output of the
     * frame wait for the decoding of the buffer it now shares (it also takes
     * an output buffer from the ring, which is simply not used).
     */
    ret = ff_v4l2_request_start_frame(avctx, &controls->pic, cur);
    if (ret)
        return ret;

    /*
     * As a reference the skipped picture is its reference: same samples, same
     * structure and the same intensity compensation pending on its fields. It
     * has no motion, which the picture type tells, and has compensated nothing.
     */
    controls->state = *ref;
    controls->state.ptype[0] = controls->state.ptype[1] =
        V4L2_VC1_PICTURE_TYPE_SKIPPED;
    memset(controls->state.fwd_ic, 0, sizeof(controls->state.fwd_ic));
    controls->skipped = 1;

    return 0;
}

static int v4l2_request_vc1_start_frame(AVCodecContext *avctx,
                                        av_unused const AVBufferRef *buf_ref,
                                        av_unused const uint8_t *buffer,
                                        av_unused uint32_t size)
{
    const VC1Context *v = avctx->priv_data;
    const MpegEncContext *s = &v->s;
    V4L2RequestControlsVC1 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    int ret;

    if (vc1_needs_output_process(v) || vc1_is_prerelease(v))
        return AVERROR(ENOSYS);

    if (v->p_frame_skipped)
        return v4l2_request_vc1_repeat_reference(avctx);
    controls->skipped = 0;

    /*
     * Called once per coded picture: per frame, or per field of a field pair.
     * The second field is a second request which decodes to the capture
     * buffer of the first one: only a new output buffer is needed.
     */
    if (v->second_field)
        ret = ff_v4l2_request_reset_picture(avctx, &controls->pic);
    else
        ret = ff_v4l2_request_start_frame(avctx, &controls->pic,
                                          s->cur_pic.ptr->f);
    if (ret)
        return ret;

    vc1_fill_sequence(v, &controls->sequence);
    vc1_fill_entrypoint(v, &controls->entrypoint);
    vc1_fill_picture(v, &controls->picture);
    if (controls->picture.bitplane_flags)
        vc1_fill_bitplanes(v, controls->picture.bitplane_flags,
                           &controls->bitplanes);
    controls->nb_slices = 0;

    ret = vc1_fill_references(v, &controls->state, &controls->picture);
    if (ret)
        return ret;
    vc1_update_state(v, &controls->state);

    return 0;
}

static int v4l2_request_vc1_decode_slice(AVCodecContext *avctx,
                                         const uint8_t *buffer, uint32_t size)
{
    static const uint8_t frame_start_code[4] = { 0, 0, 1, 0x0d };
    const VC1Context *v = avctx->priv_data;
    const MpegEncContext *s = &v->s;
    V4L2RequestControlsVC1 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    int advanced = v->profile == PROFILE_ADVANCED;
    int has_marker = advanced && size >= 4 && IS_MARKER(AV_RB32(buffer));
    int first = !controls->nb_slices;
    /*
     * vc1dec.c has parsed the picture header (first slice) or the slice
     * header, from the unescaped payload which follows the start code.
     */
    int bit_offset = get_bits_count(&v->gb);
    int emulation = 0;
    uint32_t offset;
    int ret;

    if (controls->skipped)
        return 0;
    offset = controls->pic.output->bytesused;

    /*
     * vc1dec.c hands over everything up to the next slice, the next field or
     * the end of the packet. Only the BDU of the picture or of the slice goes
     * in the output buffer: stop at the next start code, which leaves out the
     * user data BDUs (and whatever else) that may follow it.
     */
    if (advanced && size > 4) {
        const uint8_t *next = find_next_marker(buffer + 4 * has_marker,
                                               buffer + size);
        size = next - buffer;
    }

    if (advanced) {
        emulation = vc1_count_emulation_bytes(buffer + 4 * has_marker,
                                              size - 4 * has_marker,
                                              bit_offset);

        /*
         * Advanced profile frames always carry their start codes in the
         * output buffer. A container may omit the frame start code of a
         * single-BDU packet (and of the first field of a field pair).
         */
        if (!has_marker) {
            if (!first)
                return AVERROR_INVALIDDATA;
            ret = ff_v4l2_request_append_output(avctx, &controls->pic,
                                                frame_start_code,
                                                sizeof(frame_start_code));
            if (ret)
                return ret;
        }
    }

    ret = ff_v4l2_request_append_output(avctx, &controls->pic, buffer, size);
    if (ret)
        return ret;

    if (first) {
        controls->picture.data_bit_offset        = bit_offset;
        controls->picture.header_emulation_bytes = emulation;
    }

    if (controls->nb_slices < VC1_MAX_SLICES) {
        controls->slices[controls->nb_slices++] =
            (struct v4l2_ctrl_vc1_slice_params) {
                .offset                 = offset,
                .size                   = controls->pic.output->bytesused - offset,
                .data_bit_offset        = bit_offset,
                .header_emulation_bytes = emulation,
                /* vc1dec.c sets mb_y to SLICE_ADDR before calling us. */
                .slice_addr             = first ? 0 : s->mb_y,
                .flags                  = !first && v->pic_header_flag ?
                                          V4L2_VC1_SLICE_PARAMS_FLAG_PIC_HEADER : 0,
            };
    }

    return 0;
}

static int v4l2_request_vc1_end_frame(AVCodecContext *avctx)
{
    const VC1Context *v = avctx->priv_data;
    const MpegEncContext *s = &v->s;
    V4L2RequestContextVC1 *ctx = avctx->internal->hwaccel_priv_data;
    V4L2RequestControlsVC1 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    struct v4l2_ext_control control[5] = {
        {
            .id = V4L2_CID_STATELESS_VC1_SEQUENCE,
            .ptr = &controls->sequence,
            .size = sizeof(controls->sequence),
        },
        {
            .id = V4L2_CID_STATELESS_VC1_ENTRYPOINT_HEADER,
            .ptr = &controls->entrypoint,
            .size = sizeof(controls->entrypoint),
        },
        {
            .id = V4L2_CID_STATELESS_VC1_PICTURE_LAYER,
            .ptr = &controls->picture,
            .size = sizeof(controls->picture),
        },
    };
    int count = 3;

    /* Skipped picture: the reference is repeated, nothing is decoded. */
    if (controls->skipped)
        return 0;

    /* Only sent when the picture has decoded bitplanes: it is 14 KiB. */
    if (controls->picture.bitplane_flags) {
        control[count++] = (struct v4l2_ext_control) {
            .id = V4L2_CID_STATELESS_VC1_BITPLANES,
            .ptr = &controls->bitplanes,
            .size = sizeof(controls->bitplanes),
        };
    }

    if (ctx->max_slice_params) {
        if (controls->nb_slices > ctx->max_slice_params)
            return AVERROR(ENOSYS);

        control[count++] = (struct v4l2_ext_control) {
            .id = V4L2_CID_STATELESS_VC1_SLICE_PARAMS,
            .ptr = controls->slices,
            .size = sizeof(controls->slices[0]) * controls->nb_slices,
        };
    }

    /*
     * The first field of a field pair must keep the capture buffer for the
     * second one: this is the multi-slice path of the common code, which sets
     * V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF on all but the last output buffer,
     * or dequeues and queues the capture buffer again between the two fields
     * when the driver cannot hold capture buffers.
     */
    if (v->field_mode)
        return ff_v4l2_request_decode_slice(avctx, &controls->pic, control,
                                            count, !v->second_field,
                                            v->second_field);

    return ff_v4l2_request_decode_frame(avctx, &controls->pic, control, count);
}

static int v4l2_request_vc1_post_frames_ctx(AVCodecContext *avctx)
{
    V4L2RequestContextVC1 *ctx = avctx->internal->hwaccel_priv_data;
    struct v4l2_query_ext_ctrl slice_params = {
        .id = V4L2_CID_STATELESS_VC1_SLICE_PARAMS,
    };

    if (!ff_v4l2_request_query_control(avctx, &slice_params))
        ctx->max_slice_params = FFMIN(FFMAX(slice_params.dims[0], 1),
                                      VC1_MAX_SLICES);
    else
        ctx->max_slice_params = 0;

    // TODO: check V4L2_CID_MPEG_VIDEO_VC1_PROFILE and V4L2_CID_MPEG_VIDEO_VC1_LEVEL

    return 0;
}

static int v4l2_request_vc1_init(AVCodecContext *avctx)
{
    const VC1Context *v = avctx->priv_data;
    struct v4l2_ctrl_vc1_sequence sequence;
    struct v4l2_ctrl_vc1_entrypoint_header entrypoint;
    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_STATELESS_VC1_SEQUENCE,
            .ptr = &sequence,
            .size = sizeof(sequence),
        },
        {
            .id = V4L2_CID_STATELESS_VC1_ENTRYPOINT_HEADER,
            .ptr = &entrypoint,
            .size = sizeof(entrypoint),
        },
    };

    if (vc1_needs_output_process(v) || vc1_is_prerelease(v))
        return AVERROR(ENOSYS);

    /* Lets the driver refuse the profile or the coded size before decoding. */
    vc1_fill_sequence(v, &sequence);
    vc1_fill_entrypoint(v, &entrypoint);

    return ff_v4l2_request_init(avctx, control, FF_ARRAY_ELEMS(control),
                                v4l2_request_vc1_post_frames_ctx);
}

static int v4l2_request_vc1_frame_params(AVCodecContext *avctx,
                                         AVBufferRef *hw_frames_ctx)
{
    return ff_v4l2_request_frame_params(avctx, hw_frames_ctx,
                                        V4L2_PIX_FMT_VC1_SLICE, 8);
}

#if CONFIG_WMV3_V4L2REQUEST_HWACCEL
const FFHWAccel ff_wmv3_v4l2request_hwaccel = {
    .p.name             = "wmv3_v4l2request",
    .p.type             = AVMEDIA_TYPE_VIDEO,
    .p.id               = AV_CODEC_ID_WMV3,
    .p.pix_fmt          = AV_PIX_FMT_DRM_PRIME,
    .start_frame        = v4l2_request_vc1_start_frame,
    .decode_slice       = v4l2_request_vc1_decode_slice,
    .end_frame          = v4l2_request_vc1_end_frame,
    .flush              = ff_v4l2_request_flush,
    .frame_priv_data_size = sizeof(V4L2RequestControlsVC1),
    .init               = v4l2_request_vc1_init,
    .uninit             = ff_v4l2_request_uninit,
    .priv_data_size     = sizeof(V4L2RequestContextVC1),
    .frame_params       = v4l2_request_vc1_frame_params,
    .caps_internal      = HWACCEL_CAP_ASYNC_SAFE,
};
#endif

#if CONFIG_VC1_V4L2REQUEST_HWACCEL
const FFHWAccel ff_vc1_v4l2request_hwaccel = {
    .p.name             = "vc1_v4l2request",
    .p.type             = AVMEDIA_TYPE_VIDEO,
    .p.id               = AV_CODEC_ID_VC1,
    .p.pix_fmt          = AV_PIX_FMT_DRM_PRIME,
    .start_frame        = v4l2_request_vc1_start_frame,
    .decode_slice       = v4l2_request_vc1_decode_slice,
    .end_frame          = v4l2_request_vc1_end_frame,
    .flush              = ff_v4l2_request_flush,
    .frame_priv_data_size = sizeof(V4L2RequestControlsVC1),
    .init               = v4l2_request_vc1_init,
    .uninit             = ff_v4l2_request_uninit,
    .priv_data_size     = sizeof(V4L2RequestContextVC1),
    .frame_params       = v4l2_request_vc1_frame_params,
    .caps_internal      = HWACCEL_CAP_ASYNC_SAFE,
};
#endif
