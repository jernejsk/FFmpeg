/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Stateless MPEG-4 Part 2 (ISO/IEC 14496-2), H.263 (ITU-T H.263) and
 * Sorenson Spark decoder controls.
 *
 * Private copy of the definitions proposed for include/uapi/linux/videodev2.h
 * and include/uapi/linux/v4l2-controls.h, for use until they are part of the
 * kernel headers. It must be kept in sync with the kernel patches. Everything
 * here is skipped when the kernel headers already provide the definitions.
 */

#ifndef AVCODEC_V4L2_STATELESS_MPEG4_H
#define AVCODEC_V4L2_STATELESS_MPEG4_H

#include <linux/videodev2.h>

#ifndef V4L2_PIX_FMT_MPEG4_SLICE
#define V4L2_PIX_FMT_MPEG4_SLICE v4l2_fourcc('M', 'G', '4', 'S') /* MPEG-4 Part 2 parsed VOP data */
#define V4L2_PIX_FMT_H263_SLICE v4l2_fourcc('S', '2', '6', '3') /* H.263 parsed picture data */
#define V4L2_PIX_FMT_SPK_SLICE v4l2_fourcc('S', 'P', 'K', 'S') /* Sorenson Spark parsed picture data */
#endif

#ifndef V4L2_CID_STATELESS_MPEG4_VOL

#define V4L2_CTRL_TYPE_MPEG4_VOL		0x02a0
#define V4L2_CTRL_TYPE_MPEG4_VOP		0x02a1
#define V4L2_CTRL_TYPE_MPEG4_QUANTISATION	0x02a2
#define V4L2_CTRL_TYPE_MPEG4_SLICE_PARAMS	0x02a3

#define V4L2_CTRL_TYPE_H263_PICTURE		0x02b0
#define V4L2_CTRL_TYPE_H263_SLICE_PARAMS	0x02b1

#define V4L2_MPEG4_VOL_FLAG_INTERLACED			0x01
#define V4L2_MPEG4_VOL_FLAG_QUANT_TYPE			0x02
#define V4L2_MPEG4_VOL_FLAG_QUARTER_SAMPLE		0x04
#define V4L2_MPEG4_VOL_FLAG_RESYNC_MARKER_DISABLE	0x08
#define V4L2_MPEG4_VOL_FLAG_DATA_PARTITIONED		0x10
#define V4L2_MPEG4_VOL_FLAG_REVERSIBLE_VLC		0x20

#define V4L2_MPEG4_SPRITE_ENABLE_NONE			0
#define V4L2_MPEG4_SPRITE_ENABLE_GMC			2

#define V4L2_MPEG4_MAX_GMC_WARPING_POINTS		3

#define V4L2_CID_STATELESS_MPEG4_VOL	(V4L2_CID_CODEC_STATELESS_BASE + 700)
/**
 * struct v4l2_ctrl_mpeg4_vol - MPEG-4 video object layer
 *
 * All the members, except the flags, carry the syntax elements of the same
 * name of the video_object_layer() syntax of ISO/IEC 14496-2. Only the
 * rectangular, 8-bit, 4:2:0 video object layers of the Simple and Advanced
 * Simple profiles are covered.
 *
 * @flags: see V4L2_MPEG4_VOL_FLAG_{}.
 * @video_object_layer_width: width of the VOP in luma samples.
 * @video_object_layer_height: height of the VOP in luma samples.
 * @vop_time_increment_resolution: number of ticks in one second.
 * @sprite_enable: V4L2_MPEG4_SPRITE_ENABLE_NONE or
 *	V4L2_MPEG4_SPRITE_ENABLE_GMC; static sprites are not supported.
 * @no_of_sprite_warping_points: number of GMC warping points, 0 to 3.
 * @sprite_warping_accuracy: GMC warping accuracy code, 0 to 3.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_ctrl_mpeg4_vol {
	__u32	flags;
	__u16	video_object_layer_width;
	__u16	video_object_layer_height;
	__u16	vop_time_increment_resolution;
	__u8	sprite_enable;
	__u8	no_of_sprite_warping_points;
	__u8	sprite_warping_accuracy;
	__u8	reserved[3];
};

#define V4L2_MPEG4_VOP_CODING_TYPE_I			0
#define V4L2_MPEG4_VOP_CODING_TYPE_P			1
#define V4L2_MPEG4_VOP_CODING_TYPE_B			2
#define V4L2_MPEG4_VOP_CODING_TYPE_S			3

#define V4L2_MPEG4_VOP_FLAG_ROUNDING_TYPE		0x01
#define V4L2_MPEG4_VOP_FLAG_TOP_FIELD_FIRST		0x02
#define V4L2_MPEG4_VOP_FLAG_ALTERNATE_VERTICAL_SCAN	0x04

#define V4L2_CID_STATELESS_MPEG4_VOP	(V4L2_CID_CODEC_STATELESS_BASE + 701)
/**
 * struct v4l2_ctrl_mpeg4_vop - MPEG-4 video object plane
 *
 * @backward_ref_ts: timestamp of the V4L2 capture buffer to use as backward
 *	reference, used with B-VOPs.
 * @forward_ref_ts: timestamp of the V4L2 capture buffer to use as forward
 *	reference, used with P-VOPs, S-VOPs and B-VOPs.
 * @data_bit_offset: offset in bits from the first bit of the VOP start code
 *	to the first macroblock.
 * @flags: see V4L2_MPEG4_VOP_FLAG_{}.
 * @trb: TRB of B-VOPs, in ticks of vop_time_increment_resolution.
 * @trd: TRD of B-VOPs, in ticks of vop_time_increment_resolution.
 * @trb_field: TRB of field B-VOP macroblocks, in field periods.
 * @trd_field: TRD of field B-VOP macroblocks, in field periods.
 * @sprite_trajectory_du: horizontal GMC warping point trajectories of
 *	S-VOPs, as decoded from sprite_trajectory().
 * @sprite_trajectory_dv: vertical GMC warping point trajectories of S-VOPs.
 * @vop_coding_type: see V4L2_MPEG4_VOP_CODING_TYPE_{}.
 * @vop_quant: vop_quant syntax element, 1 to 31.
 * @intra_dc_vlc_thr: intra_dc_vlc_thr syntax element, 0 to 7.
 * @vop_fcode_forward: vop_fcode_forward syntax element, 1 to 7.
 * @vop_fcode_backward: vop_fcode_backward syntax element, 1 to 7.
 * @backward_ref_vop_coding_type: coding type of the backward reference of
 *	B-VOPs, V4L2_MPEG4_VOP_CODING_TYPE_I, P or S.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_ctrl_mpeg4_vop {
	__u64	backward_ref_ts;
	__u64	forward_ref_ts;
	__u32	data_bit_offset;
	__u32	flags;
	__u32	trb;
	__u32	trd;
	__u16	trb_field;
	__u16	trd_field;
	__s16	sprite_trajectory_du[V4L2_MPEG4_MAX_GMC_WARPING_POINTS];
	__s16	sprite_trajectory_dv[V4L2_MPEG4_MAX_GMC_WARPING_POINTS];
	__u8	vop_coding_type;
	__u8	vop_quant;
	__u8	intra_dc_vlc_thr;
	__u8	vop_fcode_forward;
	__u8	vop_fcode_backward;
	__u8	backward_ref_vop_coding_type;
	__u8	reserved[2];
};

#define V4L2_CID_STATELESS_MPEG4_QUANTISATION	(V4L2_CID_CODEC_STATELESS_BASE + 702)
/**
 * struct v4l2_ctrl_mpeg4_quantisation - MPEG-4 quantisation matrices
 *
 * The matrices are in zigzag scan order and always complete. They default
 * to the default matrices of ISO/IEC 14496-2. Used with
 * V4L2_MPEG4_VOL_FLAG_QUANT_TYPE.
 *
 * @intra_quantiser_matrix: intra quantisation matrix.
 * @non_intra_quantiser_matrix: non-intra quantisation matrix.
 */
struct v4l2_ctrl_mpeg4_quantisation {
	__u8	intra_quantiser_matrix[64];
	__u8	non_intra_quantiser_matrix[64];
};

#define V4L2_CID_STATELESS_MPEG4_SLICE_PARAMS	(V4L2_CID_CODEC_STATELESS_BASE + 703)
/**
 * struct v4l2_ctrl_mpeg4_slice_params - MPEG-4 video packet
 *
 * One element per segment of the VOP that starts with a header: the first
 * one starts with the VOP header, the following ones with a video packet
 * header (resync marker).
 *
 * @offset: offset in bytes from the start of the OUTPUT buffer to the
 *	first byte of the VOP start code or of the resync marker.
 * @size: size in bytes of the segment, header included.
 * @data_bit_offset: offset in bits from the first bit of the segment to its
 *	first macroblock.
 * @macroblock_number: number of the first macroblock of the segment, in
 *	raster scan order.
 * @quant_scale: quantiser scale of the first macroblock of the segment:
 *	vop_quant for the first segment, quant_scale for video packets.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_ctrl_mpeg4_slice_params {
	__u32	offset;
	__u32	size;
	__u32	data_bit_offset;
	__u16	macroblock_number;
	__u8	quant_scale;
	__u8	reserved;
};

#define V4L2_MPEG4_QUIRK_EDGE_EXACT_SIZE		0x01
#define V4L2_MPEG4_QUIRK_QPEL_CHROMA			0x02
#define V4L2_MPEG4_QUIRK_QPEL_CHROMA2			0x04
#define V4L2_MPEG4_QUIRK_FIELD_HPEL_CHROMA		0x08
#define V4L2_MPEG4_QUIRK_DC_NO_CLIP			0x10
#define V4L2_MPEG4_QUIRK_XVID_ILACE			0x20
#define V4L2_MPEG4_QUIRK_GMC_UNSCALED_REF		0x40

#define V4L2_CID_STATELESS_MPEG4_QUIRKS		(V4L2_CID_CODEC_STATELESS_BASE + 704)

/* Stateless H.263 and Sorenson Spark controls */

#define V4L2_H263_PICTURE_CODING_TYPE_I			0
#define V4L2_H263_PICTURE_CODING_TYPE_P			1

#define V4L2_H263_PICTURE_FLAG_ROUNDING_TYPE		0x0001
#define V4L2_H263_PICTURE_FLAG_PLUSPTYPE		0x0002
#define V4L2_H263_PICTURE_FLAG_UMV			0x0004
#define V4L2_H263_PICTURE_FLAG_SAC			0x0008
#define V4L2_H263_PICTURE_FLAG_AP			0x0010
#define V4L2_H263_PICTURE_FLAG_AIC			0x0020
#define V4L2_H263_PICTURE_FLAG_DF			0x0040
#define V4L2_H263_PICTURE_FLAG_SS			0x0080
#define V4L2_H263_PICTURE_FLAG_SS_RECTANGULAR		0x0100
#define V4L2_H263_PICTURE_FLAG_SS_ARBITRARY		0x0200
#define V4L2_H263_PICTURE_FLAG_ISD			0x0400
#define V4L2_H263_PICTURE_FLAG_AIV			0x0800
#define V4L2_H263_PICTURE_FLAG_MQ			0x1000
#define V4L2_H263_PICTURE_FLAG_RRU			0x2000

#define V4L2_CID_STATELESS_H263_PICTURE	(V4L2_CID_CODEC_STATELESS_BASE + 800)
/**
 * struct v4l2_ctrl_h263_picture - H.263 or Sorenson Spark picture
 *
 * @forward_ref_ts: timestamp of the V4L2 capture buffer to use as reference,
 *	used with P pictures.
 * @data_bit_offset: offset in bits from the first bit of the picture start
 *	code to the first macroblock.
 * @flags: see V4L2_H263_PICTURE_FLAG_{}.
 * @width: width of the picture in luma samples.
 * @height: height of the picture in luma samples.
 * @picture_coding_type: see V4L2_H263_PICTURE_CODING_TYPE_{}.
 * @pquant: PQUANT syntax element, 1 to 31.
 * @spk_version: version of the Sorenson Spark bitstream, 0 or 1. Zero for
 *	H.263.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_ctrl_h263_picture {
	__u64	forward_ref_ts;
	__u32	data_bit_offset;
	__u32	flags;
	__u16	width;
	__u16	height;
	__u8	picture_coding_type;
	__u8	pquant;
	__u8	spk_version;
	__u8	reserved;
};

#define V4L2_CID_STATELESS_H263_SLICE_PARAMS	(V4L2_CID_CODEC_STATELESS_BASE + 801)
/**
 * struct v4l2_ctrl_h263_slice_params - H.263 GOB or slice
 *
 * One element per segment of the picture that starts with a header: the
 * first one starts with the picture header, the following ones with a GOB
 * or slice header. GOBs without a header continue the previous segment.
 *
 * @offset: offset in bytes from the start of the OUTPUT buffer to the byte
 *	which holds the first bit of the picture, GOB or slice start code.
 * @size: size in bytes of the segment, header included.
 * @data_bit_offset: offset in bits from the first bit of the byte at
 *	@offset to the first macroblock of the segment.
 * @macroblock_number: number of the first macroblock of the segment, in
 *	raster scan order.
 * @quant_scale: quantiser of the first macroblock of the segment: PQUANT,
 *	GQUANT or SQUANT.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_ctrl_h263_slice_params {
	__u32	offset;
	__u32	size;
	__u32	data_bit_offset;
	__u16	macroblock_number;
	__u8	quant_scale;
	__u8	reserved;
};

#endif /* V4L2_CID_STATELESS_MPEG4_VOL */

#endif /* AVCODEC_V4L2_STATELESS_MPEG4_H */
