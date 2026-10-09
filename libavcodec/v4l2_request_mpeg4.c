/*
 * V4L2 Request API MPEG-4 Part 2, H.263 and Sorenson Spark hwaccel
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
#include "h263dec.h"
#include "internal.h"
#include "mpeg4video.h"
#include "mpeg4videodec.h"
#include "mpeg4videodefs.h"
#include "mpegutils.h"
#include "startcode.h"
#include "v4l2_request.h"
#include "v4l2-stateless-mpeg4.h"

/* The MPEG-4 and H.263 slice parameters share their layout. */
_Static_assert(sizeof(struct v4l2_ctrl_mpeg4_slice_params) ==
               sizeof(struct v4l2_ctrl_h263_slice_params), "slice params");

/* Video packets or GOBs with a header in one picture */
#define MPEG4_MAX_SLICES 2048

typedef struct V4L2RequestContextMPEG4 {
    V4L2RequestContext base;
    /* Size of the SLICE_PARAMS array of the driver, 0 if it has no such control. */
    unsigned int max_slice_params;
    /* MPEG-4 quirks the driver emulates */
    uint32_t supported_quirks;
} V4L2RequestContextMPEG4;

typedef struct V4L2RequestControlsMPEG4 {
    V4L2RequestPictureContext pic;
    union {
        struct {
            struct v4l2_ctrl_mpeg4_vol vol;
            struct v4l2_ctrl_mpeg4_vop vop;
            struct v4l2_ctrl_mpeg4_quantisation quantisation;
            uint32_t quirks;
        };
        struct v4l2_ctrl_h263_picture picture;
    };
    /* Offset of the start code of the picture in the bitstream buffer */
    unsigned int start;
    /* Size of the picture in the bitstream buffer, from its start code */
    unsigned int size;
    struct v4l2_ctrl_mpeg4_slice_params slices[MPEG4_MAX_SLICES];
    /* Bit position of the start code of each slice, from the picture start */
    unsigned int slice_pos[MPEG4_MAX_SLICES];
    unsigned int nb_slices;
} V4L2RequestControlsMPEG4;

static int mpeg4_intra_dc_vlc_thr(const Mpeg4DecContext *ctx)
{
    switch (ctx->intra_dc_threshold) {
    case 99: return 0;
    case 13: return 1;
    case 15: return 2;
    case 17: return 3;
    case 19: return 4;
    case 21: return 5;
    case 23: return 6;
    case 0:  return 7;
    }
    return 0;
}

static void mpeg4_fill_vol(AVCodecContext *avctx,
                           struct v4l2_ctrl_mpeg4_vol *vol)
{
    const Mpeg4DecContext *ctx = avctx->priv_data;
    const MPVContext *s = &ctx->h.c;

    *vol = (struct v4l2_ctrl_mpeg4_vol) {
        .video_object_layer_width      = s->width,
        .video_object_layer_height     = s->height,
        .vop_time_increment_resolution = avctx->framerate.num,
    };

    if (!s->progressive_sequence)
        vol->flags |= V4L2_MPEG4_VOL_FLAG_INTERLACED;
    if (ctx->mpeg_quant)
        vol->flags |= V4L2_MPEG4_VOL_FLAG_QUANT_TYPE;
    if (s->quarter_sample)
        vol->flags |= V4L2_MPEG4_VOL_FLAG_QUARTER_SAMPLE;
    if (!ctx->resync_marker)
        vol->flags |= V4L2_MPEG4_VOL_FLAG_RESYNC_MARKER_DISABLE;
    if (ctx->h.data_partitioning)
        vol->flags |= V4L2_MPEG4_VOL_FLAG_DATA_PARTITIONED;
    if (ctx->rvlc)
        vol->flags |= V4L2_MPEG4_VOL_FLAG_REVERSIBLE_VLC;

    if (ctx->vol_sprite_usage == GMC_SPRITE) {
        vol->sprite_enable               = V4L2_MPEG4_SPRITE_ENABLE_GMC;
        vol->no_of_sprite_warping_points = ctx->num_sprite_warping_points;
        vol->sprite_warping_accuracy     = ctx->sprite_warping_accuracy;
    }
}

/*
 * The deviations from ISO/IEC 14496-2 of the encoder of the stream, as
 * detected by ff_mpeg4_workaround_bugs(), which the driver has to emulate.
 * Those which the bitstream cannot exhibit are left out. Returns a negative
 * value if a deviation cannot be described to the driver at all.
 */
static int mpeg4_get_quirks(AVCodecContext *avctx, uint32_t *quirks,
                            uint32_t *optional)
{
    const Mpeg4DecContext *ctx = avctx->priv_data;
    const MPVContext *s = &ctx->h.c;
    int bugs = s->workaround_bugs;

    *quirks = *optional = 0;

    if (bugs & (FF_BUG_AMV | FF_BUG_IEDGE))
        return AVERROR(ENOSYS);
    if ((bugs & FF_BUG_STD_QPEL) && s->quarter_sample)
        return AVERROR(ENOSYS);

    if (bugs & FF_BUG_EDGE)
        *quirks |= V4L2_MPEG4_QUIRK_EDGE_EXACT_SIZE;
    if ((bugs & FF_BUG_QPEL_CHROMA) && s->quarter_sample)
        *quirks |= V4L2_MPEG4_QUIRK_QPEL_CHROMA;
    if ((bugs & FF_BUG_QPEL_CHROMA2) && s->quarter_sample)
        *quirks |= V4L2_MPEG4_QUIRK_QPEL_CHROMA2;
    /*
     * The chroma vectors of the direct mode of quarter sample B-VOPs,
     * which packed DivX streams have even when they claim low delay, have
     * no quirk.
     */
    if ((bugs & FF_BUG_DIRECT_BLOCKSIZE) && s->quarter_sample &&
        (!s->low_delay || ctx->h.divx_packed || s->pict_type == AV_PICTURE_TYPE_B))
        return AVERROR(ENOSYS);
    if ((bugs & FF_BUG_HPEL_CHROMA) && !s->progressive_sequence)
        *quirks |= V4L2_MPEG4_QUIRK_FIELD_HPEL_CHROMA;
    if ((bugs & FF_BUG_XVID_ILACE) && !s->progressive_sequence)
        *quirks |= V4L2_MPEG4_QUIRK_XVID_ILACE;
    if (ctx->vol_sprite_usage == GMC_SPRITE &&
        ctx->divx_version == 500 && ctx->divx_build == 413)
        *quirks |= V4L2_MPEG4_QUIRK_GMC_UNSCALED_REF;

    /*
     * Only matters for intra DC values above 2047, which no sane encoder
     * produces: decode anyway if the driver cannot emulate it.
     */
    if (bugs & FF_BUG_DC_CLIP)
        *optional |= V4L2_MPEG4_QUIRK_DC_NO_CLIP;

    return 0;
}

static int mpeg4_check_quirks(AVCodecContext *avctx, uint32_t *quirks)
{
    V4L2RequestContextMPEG4 *ctx = avctx->internal->hwaccel_priv_data;
    uint32_t optional;
    int ret;

    ret = mpeg4_get_quirks(avctx, quirks, &optional);
    if (ret)
        return ret;

    if (*quirks & ~ctx->supported_quirks) {
        av_log(avctx, AV_LOG_VERBOSE,
               "Encoder quirks 0x%x not supported by the driver\n",
               *quirks & ~ctx->supported_quirks);
        return AVERROR(ENOSYS);
    }

    *quirks |= optional & ctx->supported_quirks;

    return 0;
}

static int mpeg4_unsupported(AVCodecContext *avctx)
{
    const Mpeg4DecContext *ctx = avctx->priv_data;
    const MPVContext *s = &ctx->h.c;

    /* Only the tools of the Simple and Advanced Simple profiles */
    return s->studio_profile || ctx->shape != RECT_SHAPE ||
           ctx->vol_sprite_usage == STATIC_SPRITE ||
           ctx->sprite_brightness_change || ctx->new_pred ||
           ctx->scalability || ctx->quant_precision != 5 ||
           ctx->h.data_partitioning || ctx->rvlc;
}

static int mpeg4_find_vop(const uint8_t *buf, unsigned int size,
                          unsigned int data_pos, unsigned int *start,
                          unsigned int *end)
{
    const uint8_t *p = buf, *vop = NULL;
    uint32_t state = -1;

    /* The last VOP start code before the macroblock data */
    while (p < buf + data_pos) {
        p = avpriv_find_start_code(p, buf + data_pos, &state);
        if (state == VOP_STARTCODE)
            vop = p - 4;
    }
    if (!vop)
        return AVERROR_INVALIDDATA;

    /* No start code can be emulated by the VOP, resync markers included. */
    state = -1;
    p = avpriv_find_start_code(buf + data_pos, buf + size, &state);
    *start = vop - buf;
    *end   = (state & 0xffffff00) == 0x100 ? p - 4 - buf : size;

    return 0;
}

static int v4l2_request_mpeg4_start_frame(AVCodecContext *avctx,
                                          av_unused const AVBufferRef *buf_ref,
                                          const uint8_t *buffer,
                                          uint32_t size)
{
    const Mpeg4DecContext *ctx = avctx->priv_data;
    const H263DecContext *h = &ctx->h;
    const MPVContext *s = &h->c;
    V4L2RequestControlsMPEG4 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    unsigned int data_pos = get_bits_count(&h->gb);
    unsigned int end;
    int ret;

    if (mpeg4_unsupported(avctx))
        return AVERROR(ENOSYS);

    ret = mpeg4_check_quirks(avctx, &controls->quirks);
    if (ret)
        return ret;

    ret = mpeg4_find_vop(buffer, size, data_pos >> 3, &controls->start, &end);
    if (ret)
        return ret;
    controls->size = end - controls->start;

    ret = ff_v4l2_request_start_frame(avctx, &controls->pic, s->cur_pic.ptr->f);
    if (ret)
        return ret;

    mpeg4_fill_vol(avctx, &controls->vol);

    controls->vop = (struct v4l2_ctrl_mpeg4_vop) {
        .data_bit_offset    = data_pos - 8 * controls->start,
        .vop_coding_type    = s->pict_type - AV_PICTURE_TYPE_I,
        .vop_quant          = s->qscale,
        .intra_dc_vlc_thr   = mpeg4_intra_dc_vlc_thr(ctx),
        .vop_fcode_forward  = s->pict_type != AV_PICTURE_TYPE_I ? ctx->f_code : 1,
        .vop_fcode_backward = s->pict_type == AV_PICTURE_TYPE_B ? ctx->b_code : 1,
    };

    if (s->no_rounding)
        controls->vop.flags |= V4L2_MPEG4_VOP_FLAG_ROUNDING_TYPE;
    if (s->top_field_first)
        controls->vop.flags |= V4L2_MPEG4_VOP_FLAG_TOP_FIELD_FIRST;
    if (s->alternate_scan)
        controls->vop.flags |= V4L2_MPEG4_VOP_FLAG_ALTERNATE_VERTICAL_SCAN;

    if (s->pict_type != AV_PICTURE_TYPE_I && s->last_pic.ptr)
        controls->vop.forward_ref_ts =
            ff_v4l2_request_get_capture_timestamp(s->last_pic.ptr->f);

    if (s->pict_type == AV_PICTURE_TYPE_B) {
        if (!s->next_pic.ptr)
            return AVERROR_INVALIDDATA;
        controls->vop.backward_ref_ts =
            ff_v4l2_request_get_capture_timestamp(s->next_pic.ptr->f);
        controls->vop.backward_ref_vop_coding_type =
            s->next_pic.ptr->f->pict_type - AV_PICTURE_TYPE_I;
        controls->vop.trb       = s->pb_time;
        controls->vop.trd       = s->pp_time;
        controls->vop.trb_field = s->pb_field_time;
        controls->vop.trd_field = s->pp_field_time;
    }

    if (s->pict_type == AV_PICTURE_TYPE_S) {
        for (int i = 0; i < ctx->num_sprite_warping_points && i < 3; i++) {
            controls->vop.sprite_trajectory_du[i] = (int16_t)ctx->sprite_traj[i][0];
            controls->vop.sprite_trajectory_dv[i] = (int16_t)ctx->sprite_traj[i][1];
        }
    }

    if (ctx->mpeg_quant) {
        for (int i = 0; i < 64; i++) {
            int n = s->idsp.idct_permutation[ff_zigzag_direct[i]];

            controls->quantisation.intra_quantiser_matrix[i]     = s->intra_matrix[n];
            controls->quantisation.non_intra_quantiser_matrix[i] = s->inter_matrix[n];
        }
    }

    controls->nb_slices = 0;

    return ff_v4l2_request_append_output(avctx, &controls->pic,
                                         buffer + controls->start,
                                         controls->size);
}

/*
 * H.263 tools the controls cannot describe to the driver without flags no
 * driver supports yet: decode them in software. FFmpeg itself refuses SAC,
 * rectangular slices, RPS, ISD and RRU.
 */
static int h263_unsupported(AVCodecContext *avctx)
{
    const H263DecContext *h = avctx->priv_data;

    return h->pb_frame || h->c.obmc || h->loop_filter || h->alt_inter_vlc;
}

/* Offset of the picture start code: 22 bits for H.263, 17 for Spark */
static int h263_find_picture(const uint8_t *buf, unsigned int size,
                             unsigned int data_pos, int flv)
{
    for (unsigned int i = 0; i + 2 < size && i < data_pos; i++)
        if (!buf[i] && !buf[i + 1] && (buf[i + 2] & (flv ? 0xf8 : 0xfc)) == 0x80)
            return i;

    return AVERROR_INVALIDDATA;
}

static void h263_fill_picture(AVCodecContext *avctx, int plusptype,
                              unsigned int data_bit_offset,
                              struct v4l2_ctrl_h263_picture *picture)
{
    const H263DecContext *h = avctx->priv_data;
    const MPVContext *s = &h->c;
    int flv = avctx->codec_id == AV_CODEC_ID_FLV1;

    *picture = (struct v4l2_ctrl_h263_picture) {
        .data_bit_offset     = data_bit_offset,
        .width               = s->width,
        .height              = s->height,
        .picture_coding_type = s->pict_type == AV_PICTURE_TYPE_P ?
                               V4L2_H263_PICTURE_CODING_TYPE_P :
                               V4L2_H263_PICTURE_CODING_TYPE_I,
        .pquant              = s->qscale,
        .spk_version         = flv ? h->flv : 0,
    };

    if (s->no_rounding)
        picture->flags |= V4L2_H263_PICTURE_FLAG_ROUNDING_TYPE;
    if (plusptype)
        picture->flags |= V4L2_H263_PICTURE_FLAG_PLUSPTYPE;
    if (plusptype ? h->umvplus : h->h263_long_vectors)
        picture->flags |= V4L2_H263_PICTURE_FLAG_UMV;
    if (s->h263_aic)
        picture->flags |= V4L2_H263_PICTURE_FLAG_AIC;
    if (h->modified_quant)
        picture->flags |= V4L2_H263_PICTURE_FLAG_MQ;
    if (h->h263_slice_structured)
        picture->flags |= V4L2_H263_PICTURE_FLAG_SS;

    if (s->pict_type == AV_PICTURE_TYPE_P && s->last_pic.ptr)
        picture->forward_ref_ts =
            ff_v4l2_request_get_capture_timestamp(s->last_pic.ptr->f);
}

static int v4l2_request_h263_start_frame(AVCodecContext *avctx,
                                         av_unused const AVBufferRef *buf_ref,
                                         const uint8_t *buffer,
                                         uint32_t size)
{
    const H263DecContext *h = avctx->priv_data;
    const MPVContext *s = &h->c;
    V4L2RequestControlsMPEG4 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    unsigned int data_pos = get_bits_count(&h->gb);
    int flv = avctx->codec_id == AV_CODEC_ID_FLV1;
    int plusptype, start, ret;

    if (h263_unsupported(avctx) || s->pict_type == AV_PICTURE_TYPE_B)
        return AVERROR(ENOSYS);

    start = h263_find_picture(buffer, size, data_pos >> 3, flv);
    if (start < 0)
        return start;
    controls->start = start;
    controls->size  = size - start;

    /* source format 7 in PTYPE: PLUSPTYPE follows */
    plusptype = !flv && controls->size >= 8 &&
                ((AV_RB64(buffer + start) >> 26) & 7) == 7;

    ret = ff_v4l2_request_start_frame(avctx, &controls->pic, s->cur_pic.ptr->f);
    if (ret)
        return ret;

    h263_fill_picture(avctx, plusptype, data_pos - 8 * start,
                      &controls->picture);
    controls->nb_slices = 0;

    return ff_v4l2_request_append_output(avctx, &controls->pic,
                                         buffer + start, controls->size);
}

/*
 * Called by h263dec.c for the picture header and for every GOB or video
 * packet header it finds (HWACCEL_CAP_RESYNC_SLICES), with the header parsed.
 */
static int v4l2_request_mpeg4_decode_slice(AVCodecContext *avctx,
                                           av_unused const uint8_t *buffer,
                                           av_unused uint32_t size)
{
    const H263DecContext *h = avctx->priv_data;
    const MPVContext *s = &h->c;
    V4L2RequestControlsMPEG4 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    unsigned int first = !controls->nb_slices;
    unsigned int pos, data_pos;

    if (controls->nb_slices >= MPEG4_MAX_SLICES)
        return AVERROR(ENOSYS);

    data_pos = get_bits_count(&h->gb) - 8 * controls->start;
    pos      = first ? 0 : h->resync_pos - 8 * controls->start;
    if (data_pos > 8 * controls->size || pos > data_pos)
        return AVERROR_INVALIDDATA;

    controls->slice_pos[controls->nb_slices] = pos;
    controls->slices[controls->nb_slices++] =
        (struct v4l2_ctrl_mpeg4_slice_params) {
            .offset            = pos >> 3,
            .data_bit_offset   = data_pos - (pos & ~7),
            .macroblock_number = first ? 0 : s->mb_y * s->mb_width + s->mb_x,
            .quant_scale       = s->qscale,
        };

    return 0;
}

static void mpeg4_finish_slices(V4L2RequestControlsMPEG4 *controls)
{
    for (unsigned int i = 0; i < controls->nb_slices; i++) {
        /* The byte which holds the next start code ends the segment. */
        unsigned int end = i + 1 < controls->nb_slices ?
                           (controls->slice_pos[i + 1] + 7) >> 3 :
                           controls->size;

        controls->slices[i].size = end - controls->slices[i].offset;
    }
}

static int v4l2_request_mpeg4_end_frame(AVCodecContext *avctx)
{
    const H263DecContext *h = avctx->priv_data;
    const MPVContext *s = &h->c;
    V4L2RequestContextMPEG4 *ctx = avctx->internal->hwaccel_priv_data;
    V4L2RequestControlsMPEG4 *controls = s->cur_pic.ptr->hwaccel_picture_private;
    struct v4l2_ext_control control[5];
    int count = 0;

    mpeg4_finish_slices(controls);

    if (avctx->codec_id == AV_CODEC_ID_MPEG4) {
        control[count++] = (struct v4l2_ext_control) {
            .id   = V4L2_CID_STATELESS_MPEG4_VOL,
            .ptr  = &controls->vol,
            .size = sizeof(controls->vol),
        };
        control[count++] = (struct v4l2_ext_control) {
            .id   = V4L2_CID_STATELESS_MPEG4_VOP,
            .ptr  = &controls->vop,
            .size = sizeof(controls->vop),
        };
        if (controls->vol.flags & V4L2_MPEG4_VOL_FLAG_QUANT_TYPE)
            control[count++] = (struct v4l2_ext_control) {
                .id   = V4L2_CID_STATELESS_MPEG4_QUANTISATION,
                .ptr  = &controls->quantisation,
                .size = sizeof(controls->quantisation),
            };
        if (ctx->supported_quirks)
            control[count++] = (struct v4l2_ext_control) {
                .id    = V4L2_CID_STATELESS_MPEG4_QUIRKS,
                .value = controls->quirks,
            };
    } else {
        control[count++] = (struct v4l2_ext_control) {
            .id   = V4L2_CID_STATELESS_H263_PICTURE,
            .ptr  = &controls->picture,
            .size = sizeof(controls->picture),
        };
    }

    if (ctx->max_slice_params) {
        if (controls->nb_slices > ctx->max_slice_params)
            return AVERROR(ENOSYS);

        control[count++] = (struct v4l2_ext_control) {
            .id   = avctx->codec_id == AV_CODEC_ID_MPEG4 ?
                    V4L2_CID_STATELESS_MPEG4_SLICE_PARAMS :
                    V4L2_CID_STATELESS_H263_SLICE_PARAMS,
            .ptr  = controls->slices,
            .size = sizeof(controls->slices[0]) * controls->nb_slices,
        };
    }

    return ff_v4l2_request_decode_frame(avctx, &controls->pic, control, count);
}

static int v4l2_request_mpeg4_post_frames_ctx(AVCodecContext *avctx)
{
    V4L2RequestContextMPEG4 *ctx = avctx->internal->hwaccel_priv_data;
    int mpeg4 = avctx->codec_id == AV_CODEC_ID_MPEG4;
    struct v4l2_query_ext_ctrl slice_params = {
        .id = mpeg4 ? V4L2_CID_STATELESS_MPEG4_SLICE_PARAMS :
                      V4L2_CID_STATELESS_H263_SLICE_PARAMS,
    };
    struct v4l2_query_ext_ctrl quirks = {
        .id = V4L2_CID_STATELESS_MPEG4_QUIRKS,
    };
    uint32_t needed;

    if (!ff_v4l2_request_query_control(avctx, &slice_params))
        ctx->max_slice_params = FFMIN(FFMAX(slice_params.dims[0], 1),
                                      MPEG4_MAX_SLICES);
    else
        ctx->max_slice_params = 0;

    if (!mpeg4)
        return 0;

    if (!ff_v4l2_request_query_control(avctx, &quirks))
        ctx->supported_quirks = quirks.maximum;
    else
        ctx->supported_quirks = 0;

    /* Fall back to software when the driver cannot follow the encoder. */
    return mpeg4_check_quirks(avctx, &needed);
}

static int v4l2_request_mpeg4_init(AVCodecContext *avctx)
{
    struct v4l2_ctrl_mpeg4_vol vol;
    struct v4l2_ext_control control[] = {
        {
            .id   = V4L2_CID_STATELESS_MPEG4_VOL,
            .ptr  = &vol,
            .size = sizeof(vol),
        },
    };

    if (mpeg4_unsupported(avctx))
        return AVERROR(ENOSYS);

    /* Lets the driver refuse the tools or the size before decoding. */
    mpeg4_fill_vol(avctx, &vol);

    return ff_v4l2_request_init(avctx, control, FF_ARRAY_ELEMS(control),
                                v4l2_request_mpeg4_post_frames_ctx);
}

static int v4l2_request_h263_init(AVCodecContext *avctx)
{
    const H263DecContext *h = avctx->priv_data;
    struct v4l2_ctrl_h263_picture picture;
    struct v4l2_ext_control control[] = {
        {
            .id   = V4L2_CID_STATELESS_H263_PICTURE,
            .ptr  = &picture,
            .size = sizeof(picture),
        },
    };

    if (h263_unsupported(avctx))
        return AVERROR(ENOSYS);

    /*
     * Lets the driver refuse the coding tools or the size before decoding:
     * the extended picture type tools stay enabled until a header changes
     * them, which the first picture header of a stream does.
     */
    h263_fill_picture(avctx, h->umvplus || h->c.h263_aic ||
                      h->modified_quant || h->h263_slice_structured, 0,
                      &picture);
    picture.forward_ref_ts = 0;
    /* The Spark decoder may not have parsed a picture header yet. */
    picture.pquant = FFMAX(picture.pquant, 1);

    return ff_v4l2_request_init(avctx, control, FF_ARRAY_ELEMS(control),
                                v4l2_request_mpeg4_post_frames_ctx);
}

static int v4l2_request_mpeg4_frame_params(AVCodecContext *avctx,
                                           AVBufferRef *hw_frames_ctx)
{
    uint32_t pixelformat;

    switch (avctx->codec_id) {
    case AV_CODEC_ID_MPEG4:
        pixelformat = V4L2_PIX_FMT_MPEG4_SLICE;
        break;
    case AV_CODEC_ID_FLV1:
        pixelformat = V4L2_PIX_FMT_SPK_SLICE;
        break;
    default:
        pixelformat = V4L2_PIX_FMT_H263_SLICE;
        break;
    }

    return ff_v4l2_request_frame_params(avctx, hw_frames_ctx, pixelformat, 8);
}

#define V4L2_REQUEST_MPEG4_HWACCEL(codec, id_, start)                       \
const FFHWAccel ff_ ## codec ## _v4l2request_hwaccel = {                     \
    .p.name             = #codec "_v4l2request",                             \
    .p.type             = AVMEDIA_TYPE_VIDEO,                                \
    .p.id               = id_,                                               \
    .p.pix_fmt          = AV_PIX_FMT_DRM_PRIME,                              \
    .start_frame        = start ## _start_frame,                             \
    .decode_slice       = v4l2_request_mpeg4_decode_slice,                   \
    .end_frame          = v4l2_request_mpeg4_end_frame,                      \
    .flush              = ff_v4l2_request_flush,                             \
    .frame_priv_data_size = sizeof(V4L2RequestControlsMPEG4),                \
    .init               = start ## _init,                                    \
    .uninit             = ff_v4l2_request_uninit,                            \
    .priv_data_size     = sizeof(V4L2RequestContextMPEG4),                   \
    .frame_params       = v4l2_request_mpeg4_frame_params,                   \
    .caps_internal      = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_RESYNC_SLICES, \
}

#if CONFIG_MPEG4_V4L2REQUEST_HWACCEL
V4L2_REQUEST_MPEG4_HWACCEL(mpeg4, AV_CODEC_ID_MPEG4, v4l2_request_mpeg4);
#endif

#if CONFIG_H263_V4L2REQUEST_HWACCEL
V4L2_REQUEST_MPEG4_HWACCEL(h263, AV_CODEC_ID_H263, v4l2_request_h263);
#endif

#if CONFIG_FLV_V4L2REQUEST_HWACCEL
V4L2_REQUEST_MPEG4_HWACCEL(flv, AV_CODEC_ID_FLV1, v4l2_request_h263);
#endif
