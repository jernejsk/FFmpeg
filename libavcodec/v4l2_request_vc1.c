/*
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

#include "hwaccel.h"
#include "v4l2_request.h"
#include "vc1.h"
#include "vc1-ctrls.h"
#include "vc1data.h"

typedef struct V4L2RequestControlsVC1 {
    struct v4l2_ctrl_vc1_slice_params slice_params;
    struct v4l2_ctrl_vc1_bitplanes bitplanes;
} V4L2RequestControlsVC1;

/** Reconstruct bitstream PTYPE (7.1.1.4, index into Table-35) */
static int vc1_get_PTYPE(const VC1Context *v)
{
    const MpegEncContext *s = &v->s;
    switch (s->pict_type) {
        case AV_PICTURE_TYPE_I: return 0;
        case AV_PICTURE_TYPE_P: return v->p_frame_skipped ? 4 : 1;
        case AV_PICTURE_TYPE_B: return v->bi_type         ? 3 : 2;
    }
    return 0;
}

/** Reconstruct bitstream FPTYPE (9.1.1.42, index into Table-105) */
static int vc1_get_FPTYPE(const VC1Context *v)
{
    const MpegEncContext *s = &v->s;
    switch (s->pict_type) {
        case AV_PICTURE_TYPE_I: return 0;
        case AV_PICTURE_TYPE_P: return 3;
        case AV_PICTURE_TYPE_B: return v->bi_type ? 7 : 4;
    }
    return 0;
}

/** Reconstruct bitstream MVMODE (7.1.1.32) */
static inline int vc1_get_MVMODE(const VC1Context *v)
{
    if ((v->fcm == PROGRESSIVE || v->fcm == ILACE_FIELD) &&
        ((v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) ||
        (v->s.pict_type == AV_PICTURE_TYPE_B && !v->bi_type)))
        return v->mv_mode;
    return 0;
}

/** Reconstruct bitstream MVMODE2 (7.1.1.33) */
static inline int vc1_get_MVMODE2(const VC1Context *v)
{
    if ((v->fcm == PROGRESSIVE || v->fcm == ILACE_FIELD) &&
        (v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) &&
        v->mv_mode == MV_PMODE_INTENSITY_COMP)
        return v->mv_mode2;
    return 0;
}

static inline int vc1_get_LUMSCALE(const VC1Context *v)
{
    if (v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) {
        if ((v->fcm == PROGRESSIVE && v->mv_mode == MV_PMODE_INTENSITY_COMP) ||
            (v->fcm == ILACE_FRAME && v->intcomp))
            return v->lumscale;
        else if (v->fcm == ILACE_FIELD && v->mv_mode == MV_PMODE_INTENSITY_COMP)
            switch (v->intcompfield) {
                case 1: return v->lumscale;
                case 2: return v->lumscale2;
                case 3: return v->lumscale;
            }
    }
    return 0;
}

static inline int vc1_get_LUMSHIFT(const VC1Context *v)
{
    if (v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) {
        if ((v->fcm == PROGRESSIVE && v->mv_mode == MV_PMODE_INTENSITY_COMP) ||
            (v->fcm == ILACE_FRAME && v->intcomp))
            return v->lumshift;
        else if (v->fcm == ILACE_FIELD && v->mv_mode == MV_PMODE_INTENSITY_COMP)
            switch (v->intcompfield) {
                case 1: return v->lumshift;
                case 2: return v->lumshift2;
                case 3: return v->lumshift;
            }
    }
    return 0;
}

av_unused static inline int vc1_get_LUMSCALE2(const VC1Context *v)
{
    if ((v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) &&
        v->fcm == ILACE_FIELD &&
        v->mv_mode == MV_PMODE_INTENSITY_COMP &&
        v->intcompfield == 3)
        return v->lumscale2;
    return 0;
}

av_unused static inline int vc1_get_LUMSHIFT2(const VC1Context *v)
{
    if ((v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) &&
        v->fcm == ILACE_FIELD &&
        v->mv_mode == MV_PMODE_INTENSITY_COMP &&
        v->intcompfield == 3)
        return v->lumshift2;
    return 0;
}

av_unused static inline int vc1_get_INTCOMPFIELD(const VC1Context *v)
{
    if ((v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) &&
        v->fcm == ILACE_FIELD &&
        v->mv_mode == MV_PMODE_INTENSITY_COMP)
        switch (v->intcompfield) {
            case 1: return 1;
            case 2: return 2;
            case 3: return 0;
        }
    return 0;
}

/** Reconstruct bitstream TTFRM (7.1.1.41, Table-53) */
static inline int vc1_get_TTFRM(const VC1Context *v)
{
    switch (v->ttfrm) {
        case TT_8X8: return 0;
        case TT_8X4: return 1;
        case TT_4X8: return 2;
        case TT_4X4: return 3;
    }
    return 0;
}

/** Check whether the MVTYPEMB bitplane is present */
static inline int vc1_has_MVTYPEMB_bitplane(const VC1Context *v)
{
    if (v->mv_type_is_raw)
        return 0;
    return v->fcm == PROGRESSIVE &&
           (v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) &&
           (v->mv_mode == MV_PMODE_MIXED_MV ||
            (v->mv_mode == MV_PMODE_INTENSITY_COMP &&
             v->mv_mode2 == MV_PMODE_MIXED_MV));
}

/** Check whether the SKIPMB bitplane is present */
static inline int vc1_has_SKIPMB_bitplane(const VC1Context *v)
{
    if (v->skip_is_raw)
        return 0;
    return (v->fcm == PROGRESSIVE || v->fcm == ILACE_FRAME) &&
           ((v->s.pict_type == AV_PICTURE_TYPE_P && !v->p_frame_skipped) ||
            (v->s.pict_type == AV_PICTURE_TYPE_B && !v->bi_type));
}

/** Check whether the DIRECTMB bitplane is present */
static inline int vc1_has_DIRECTMB_bitplane(const VC1Context *v)
{
    if (v->dmb_is_raw)
        return 0;
    return (v->fcm == PROGRESSIVE || v->fcm == ILACE_FRAME) &&
           (v->s.pict_type == AV_PICTURE_TYPE_B && !v->bi_type);
}

/** Check whether the ACPRED bitplane is present */
static inline int vc1_has_ACPRED_bitplane(const VC1Context *v)
{
    if (v->acpred_is_raw)
        return 0;
    return v->profile == PROFILE_ADVANCED &&
           (v->s.pict_type == AV_PICTURE_TYPE_I ||
            (v->s.pict_type == AV_PICTURE_TYPE_B && v->bi_type));
}

/** Check whether the OVERFLAGS bitplane is present */
static inline int vc1_has_OVERFLAGS_bitplane(const VC1Context *v)
{
    if (v->overflg_is_raw)
        return 0;
    return v->profile == PROFILE_ADVANCED &&
           (v->s.pict_type == AV_PICTURE_TYPE_I ||
            (v->s.pict_type == AV_PICTURE_TYPE_B && v->bi_type)) &&
           (v->overlap && v->pq <= 8) &&
           v->condover == CONDOVER_SELECT;
}

/** Check whether the FIELDTX bitplane is present */
static inline int vc1_has_FIELDTX_bitplane(const VC1Context *v)
{
    if (v->fieldtx_is_raw)
        return 0;
    return v->fcm == ILACE_FRAME &&
           (v->s.pict_type == AV_PICTURE_TYPE_I ||
            (v->s.pict_type == AV_PICTURE_TYPE_B && v->bi_type));
}

/** Check whether the FORWARDMB bitplane is present */
static inline int vc1_has_FORWARDMB_bitplane(const VC1Context *v)
{
    if (v->fmb_is_raw)
        return 0;
    return v->fcm == ILACE_FIELD &&
           (v->s.pict_type == AV_PICTURE_TYPE_B && !v->bi_type);
}

static inline void vc1_pack_bitplanes(uint8_t *bitplane, const uint8_t *source, const MpegEncContext *s)
{
    int x, y, n, ff_bp_index;

    memset(bitplane, 0, 1024);

    n = 0;
    for (y = 0; y < s->mb_height; y++)
        for (x = 0; x < s->mb_width; x++, n++) {
            if (n == 1024 * 8) {
                av_log(s->avctx, AV_LOG_ERROR, "%s: Not enough space to store bitplane. Number of MB: %d\n", __func__, s->mb_height * s->mb_width);
                return;
            }
            ff_bp_index = y * s->mb_stride + x;
            bitplane[n / 8] |= source[ff_bp_index] << (n % 8);
        }
}

static int v4l2_request_vc1_start_frame(AVCodecContext *avctx,
                                        av_unused const uint8_t *buffer,
                                        av_unused uint32_t size)
{
    const VC1Context *v = avctx->priv_data;
    const MpegEncContext *s = &v->s;
    V4L2RequestControlsVC1 *controls = s->current_picture_ptr->hwaccel_picture_private;
    struct v4l2_vc1_entrypoint_header *entrypoint;
    struct v4l2_vc1_picture_layer *picture;
    struct v4l2_vc1_vopdquant *vopdquant;
    struct v4l2_vc1_sequence *sequence;
    struct v4l2_vc1_metadata *metadata;

    sequence = &controls->slice_params.sequence;
    entrypoint = &controls->slice_params.entrypoint_header;
    picture = &controls->slice_params.picture_layer;
    vopdquant = &controls->slice_params.vopdquant;
    metadata = &controls->slice_params.metadata;

    controls->slice_params = (struct v4l2_ctrl_vc1_slice_params) {
        .sequence = {
            .profile = v->profile,
            .level = v->level,
            .colordiff_format = v->chromaformat,
        },

        .entrypoint_header = {
            .dquant = v->dquant,
            .quantizer = v->quantizer_mode,
            .coded_width = s->avctx->coded_width,
            .coded_height = s->avctx->coded_height,
            .range_mapy = v->range_mapy,
            .range_mapuv = v->range_mapuv,
        },

        .picture_layer = {
            .ptype = (v->fcm == ILACE_FIELD ? vc1_get_FPTYPE(v) : vc1_get_PTYPE(v)),
            .pqindex = v->pqindex,
            .mvrange = v->mvrange,
            .respic = v->respic,
            .transacfrm = v->c_ac_table_index,
            .transacfrm2 = v->y_ac_table_index,
            .bfraction = v->bfraction_lut_index,
            .fcm = v->fcm,
            .mvmode = vc1_get_MVMODE(v),
            .mvmode2 = vc1_get_MVMODE2(v),
            .lumscale = vc1_get_LUMSCALE(v),
            .lumshift = vc1_get_LUMSHIFT(v),
            .lumscale2 = vc1_get_LUMSCALE2(v),
            .lumshift2 = vc1_get_LUMSHIFT2(v),
            .mvtab = s->mv_table_index,
            .cbptab = v->cbptab,
            .intcompfield = vc1_get_INTCOMPFIELD(v),
            .dmvrange = v->dmvrange,
            .mbmodetab = v->mbmodetab,
            .twomvbptab = v->twomvbptab,
            .fourmvbptab = v->fourmvbptab,
            .ttfrm = vc1_get_TTFRM(v),
            .refdist = v->refdist,
            .condover = v->condover,
            .imvtab = v->imvtab,
            .icbptab = v->icbptab,
        },

        .vopdquant = {
            .altpquant = v->altpq,
            .dqprofile = v->dqprofile,
            .dqsbedge = v->dqprofile == DQPROFILE_SINGLE_EDGE  ? v->dqsbedge : 0,
            .dqdbedge = v->dqprofile == DQPROFILE_DOUBLE_EDGES ? v->dqsbedge : 0,
        },

        .metadata = {
            .maxbframes = s->avctx->max_b_frames,
        },
    };

    if (v->broadcast)
        sequence->flags |= V4L2_VC1_SEQUENCE_FLAG_PULLDOWN;
    if (v->interlace)
        sequence->flags |= V4L2_VC1_SEQUENCE_FLAG_INTERLACE;
    if (v->tfcntrflag)
        sequence->flags |= V4L2_VC1_SEQUENCE_FLAG_TFCNTRFLAG;
    if (v->finterpflag)
        sequence->flags |= V4L2_VC1_SEQUENCE_FLAG_FINTERPFLAG;
    if (v->psf)
        sequence->flags |= V4L2_VC1_SEQUENCE_FLAG_PSF;

    if (v->broken_link)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_BROKEN_LINK;
    if (v->closed_entry)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_CLOSED_ENTRY;
    if (v->panscanflag)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_PANSCAN;
    if (v->refdist_flag)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_REFDIST;
    if (s->loop_filter)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_LOOPFILTER;
    if (v->fastuvmc)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_FASTUVMC;
    if (v->extended_mv)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_EXTENDED_MV;
    if (v->vstransform)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_VSTRANSFORM;
    if (v->overlap)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_OVERLAP;
    if (v->extended_dmv)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_EXTENDED_DMV;
    if (v->range_mapy_flag)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPY;
    if (v->range_mapuv_flag)
        entrypoint->flags |= V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPUV;

    if (v->rangeredfrm)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_RANGEREDFRM;
    if (v->halfpq)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_HALFQP;
    if (v->pquantizer)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_PQUANTIZER;
    if (v->s.dc_table_index)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_TRANSDCTAB;
    if (v->tff)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_TFF;
    if (v->rnd)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_RNDCTRL;
    if (v->ttmbf)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_TTMBF;
    if (v->fourmvswitch)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_4MVSWITCH;
    if (v->intcomp)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_INTCOMP;
    if (v->numref)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_NUMREF;
    if (v->reffield)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_REFFIELD;
    if (v->second_field)
        picture->flags |= V4L2_VC1_PICTURE_LAYER_FLAG_SECOND_FIELD;

    if (v->dquantfrm)
        vopdquant->flags |= V4L2_VC1_VOPDQUANT_FLAG_DQUANTFRM;
    if (v->dqbilevel)
        vopdquant->flags |= V4L2_VC1_VOPDQUANT_FLAG_DQBILEVEL;

    if (v->multires)
        metadata->flags |= V4L2_VC1_METADATA_FLAG_MULTIRES;
    if (v->resync_marker)
        metadata->flags |= V4L2_VC1_METADATA_FLAG_SYNCMARKER;
    if (v->rangered)
        metadata->flags |= V4L2_VC1_METADATA_FLAG_RANGERED;

    if (v->mv_type_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_MVTYPEMB;
    if (v->dmb_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_DIRECTMB;
    if (v->skip_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_SKIPMB;
    if (v->fieldtx_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_FIELDTX;
    if (v->fmb_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_FORWARDMB;
    if (v->acpred_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_ACPRED;
    if (v->overflg_is_raw)
        controls->slice_params.raw_coding_flags |= V4L2_VC1_RAW_CODING_FLAG_OVERFLAGS;

    switch (s->pict_type) {
    case AV_PICTURE_TYPE_B:
        controls->slice_params.backward_ref_ts = ff_v4l2_request_get_capture_timestamp(s->next_picture.f);
        // fall-through
    case AV_PICTURE_TYPE_P:
        controls->slice_params.forward_ref_ts = ff_v4l2_request_get_capture_timestamp(s->last_picture.f);
    }

    controls->bitplanes.bitplane_flags = 0;

    if (vc1_has_MVTYPEMB_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_MVTYPEMB;
        vc1_pack_bitplanes(controls->bitplanes.mvtypemb, v->mv_type_mb_plane, s);
    }
    if (vc1_has_DIRECTMB_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_DIRECTMB;
        vc1_pack_bitplanes(controls->bitplanes.directmb, v->direct_mb_plane, s);
    }
    if (vc1_has_SKIPMB_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_SKIPMB;
        vc1_pack_bitplanes(controls->bitplanes.skipmb, s->mbskip_table, s);
    }
    if (vc1_has_FIELDTX_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_FIELDTX;
        vc1_pack_bitplanes(controls->bitplanes.fieldtx, v->fieldtx_plane, s);
    }
    if (vc1_has_FORWARDMB_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_FORWARDMB;
        vc1_pack_bitplanes(controls->bitplanes.forwardmb, v->forward_mb_plane, s);
    }
    if (vc1_has_ACPRED_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_ACPRED;
        vc1_pack_bitplanes(controls->bitplanes.acpred, v->acpred_plane, s);
    }
    if (vc1_has_OVERFLAGS_bitplane(v)) {
        controls->bitplanes.bitplane_flags |= V4L2_VC1_BITPLANE_FLAG_OVERFLAGS;
        vc1_pack_bitplanes(controls->bitplanes.overflags, v->over_flags_plane, s);
    }

    return ff_v4l2_request_reset_frame(avctx, s->current_picture_ptr->f);
}

static int v4l2_request_vc1_decode_slice(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size)
{
    const MpegEncContext *s = avctx->priv_data;

    return ff_v4l2_request_append_output_buffer(avctx, s->current_picture_ptr->f, buffer, size);
}

static int v4l2_request_vc1_end_frame(AVCodecContext *avctx)
{
    const MpegEncContext *s = avctx->priv_data;
    V4L2RequestControlsVC1 *controls = s->current_picture_ptr->hwaccel_picture_private;
    V4L2RequestDescriptor *req = (V4L2RequestDescriptor*)s->current_picture_ptr->f->data[0];

    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_VC1_SLICE_PARAMS,
            .ptr = &controls->slice_params,
            .size = sizeof(controls->slice_params),
        },
        {
            .id = V4L2_CID_MPEG_VIDEO_VC1_BITPLANES,
            .ptr = &controls->bitplanes,
            .size = sizeof(controls->bitplanes),
        },
    };

    controls->slice_params.bit_size = req->output.used * 8;

    return ff_v4l2_request_decode_frame(avctx, s->current_picture_ptr->f, control, FF_ARRAY_ELEMS(control));
}

static int v4l2_request_vc1_init(AVCodecContext *avctx)
{
    return ff_v4l2_request_init(avctx, V4L2_PIX_FMT_VC1_SLICE, 1024 * 1024, NULL, 0);
}

#if CONFIG_WMV3_V4L2REQUEST_HWACCEL
const AVHWAccel ff_wmv3_v4l2request_hwaccel = {
    .name           = "wmv3_v4l2request",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_WMV3,
    .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    .start_frame    = v4l2_request_vc1_start_frame,
    .decode_slice   = v4l2_request_vc1_decode_slice,
    .end_frame      = v4l2_request_vc1_end_frame,
    .frame_priv_data_size = sizeof(V4L2RequestControlsVC1),
    .init           = v4l2_request_vc1_init,
    .uninit         = ff_v4l2_request_uninit,
    .priv_data_size = sizeof(V4L2RequestContext),
    .frame_params   = ff_v4l2_request_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE,
};
#endif

const AVHWAccel ff_vc1_v4l2request_hwaccel = {
    .name           = "vc1_v4l2request",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_VC1,
    .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    .start_frame    = v4l2_request_vc1_start_frame,
    .decode_slice   = v4l2_request_vc1_decode_slice,
    .end_frame      = v4l2_request_vc1_end_frame,
    .frame_priv_data_size = sizeof(V4L2RequestControlsVC1),
    .init           = v4l2_request_vc1_init,
    .uninit         = ff_v4l2_request_uninit,
    .priv_data_size = sizeof(V4L2RequestContext),
    .frame_params   = ff_v4l2_request_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE,
};
