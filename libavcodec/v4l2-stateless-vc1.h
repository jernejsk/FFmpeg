/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Stateless VC-1 decoder controls (SMPTE 421M Simple, Main and Advanced profiles).
 *
 * Private copy of the definitions proposed for include/uapi/linux/videodev2.h
 * and include/uapi/linux/v4l2-controls.h, for use until they are part of the
 * kernel headers. It must be kept in sync with the kernel patches. Everything
 * here is skipped when the kernel headers already provide the definitions.
 */

#ifndef AVCODEC_V4L2_STATELESS_VC1_H
#define AVCODEC_V4L2_STATELESS_VC1_H

#include <linux/videodev2.h>

#ifndef V4L2_PIX_FMT_VC1_SLICE
#define V4L2_PIX_FMT_VC1_SLICE v4l2_fourcc('S', 'V', 'C', '1') /* VC-1 parsed slice data */
#endif

#ifndef V4L2_CID_STATELESS_VC1_SEQUENCE

#define V4L2_CTRL_TYPE_VC1_SEQUENCE		0x0290
#define V4L2_CTRL_TYPE_VC1_ENTRYPOINT_HEADER	0x0291
#define V4L2_CTRL_TYPE_VC1_PICTURE_LAYER	0x0292
#define V4L2_CTRL_TYPE_VC1_BITPLANES		0x0293
#define V4L2_CTRL_TYPE_VC1_SLICE_PARAMS		0x0294

/* Stateless VC-1 controls */

#define V4L2_VC1_PROFILE_SIMPLE				0
#define V4L2_VC1_PROFILE_MAIN				1
#define V4L2_VC1_PROFILE_COMPLEX			2
#define V4L2_VC1_PROFILE_ADVANCED			3

#define V4L2_VC1_SEQUENCE_FLAG_PULLDOWN			0x0001
#define V4L2_VC1_SEQUENCE_FLAG_INTERLACE		0x0002
#define V4L2_VC1_SEQUENCE_FLAG_TFCNTRFLAG		0x0004
#define V4L2_VC1_SEQUENCE_FLAG_FINTERPFLAG		0x0008
#define V4L2_VC1_SEQUENCE_FLAG_PSF			0x0010
#define V4L2_VC1_SEQUENCE_FLAG_MULTIRES			0x0020
#define V4L2_VC1_SEQUENCE_FLAG_SYNCMARKER		0x0040
#define V4L2_VC1_SEQUENCE_FLAG_RANGERED			0x0080
#define V4L2_VC1_SEQUENCE_FLAG_POSTPROCFLAG		0x0100
#define V4L2_VC1_SEQUENCE_FLAG_RES_RTM			0x0200

#define V4L2_CID_STATELESS_VC1_SEQUENCE	(V4L2_CID_CODEC_STATELESS_BASE + 600)
/**
 * struct v4l2_ctrl_vc1_sequence - VC-1 sequence layer parameters
 *
 * For the Advanced profile the members match the syntax elements of the
 * sequence layer, as specified by section 6.1 "Sequence-level Syntax and
 * Semantics" of SMPTE 421M. For the Simple and Main profiles there is no
 * sequence layer in the bitstream and the members are taken from the
 * STRUCT_C and STRUCT_A sequence layer data structures specified by Annex J
 * and Annex L of SMPTE 421M.
 *
 * @profile: PROFILE syntax element, see V4L2_VC1_PROFILE_{}.
 * @level: LEVEL syntax element. Advanced profile only, zero otherwise.
 * @colordiff_format: COLORDIFF_FORMAT syntax element. The only value defined
 * by SMPTE 421M is 1 (4:2:0), which shall also be used for the Simple and Main
 * profiles.
 * @maxbframes: MAXBFRAMES syntax element. Simple and Main profiles only, zero
 * otherwise.
 * @frmrtq_postproc: FRMRTQ_POSTPROC syntax element.
 * @bitrtq_postproc: BITRTQ_POSTPROC syntax element.
 * @max_coded_width: maximum coded width of the pictures of the sequence, in
 * luma samples. For the Advanced profile this is (MAX_CODED_WIDTH + 1) * 2,
 * for the Simple and Main profiles this is HORIZ_SIZE of STRUCT_A.
 * @max_coded_height: maximum coded height of the pictures of the sequence, in
 * luma samples. For the Advanced profile this is (MAX_CODED_HEIGHT + 1) * 2,
 * for the Simple and Main profiles this is VERT_SIZE of STRUCT_A.
 * @reserved: padding field. Should be zeroed by applications.
 * @flags: see V4L2_VC1_SEQUENCE_FLAG_{}. V4L2_VC1_SEQUENCE_FLAG_RES_RTM is the
 * last bit of STRUCT_C (Simple and Main profiles): it is 1 in the bitstreams
 * which conform to SMPTE 421M and 0 in pre-release bitstreams, whose
 * macroblock layer is coded differently.
 */
struct v4l2_ctrl_vc1_sequence {
	__u8	profile;
	__u8	level;
	__u8	colordiff_format;
	__u8	maxbframes;
	__u8	frmrtq_postproc;
	__u8	bitrtq_postproc;
	__u16	max_coded_width;
	__u16	max_coded_height;
	__u8	reserved[2];
	__u32	flags;
};

#define V4L2_VC1_QUANTIZER_IMPLICIT			0
#define V4L2_VC1_QUANTIZER_EXPLICIT			1
#define V4L2_VC1_QUANTIZER_NON_UNIFORM			2
#define V4L2_VC1_QUANTIZER_UNIFORM			3

#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_BROKEN_LINK	0x001
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_CLOSED_ENTRY	0x002
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_PANSCAN		0x004
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_REFDIST		0x008
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_LOOPFILTER	0x010
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_FASTUVMC	0x020
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_EXTENDED_MV	0x040
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_VSTRANSFORM	0x080
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_OVERLAP		0x100
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_EXTENDED_DMV	0x200
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPY	0x400
#define V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPUV	0x800

#define V4L2_CID_STATELESS_VC1_ENTRYPOINT_HEADER (V4L2_CID_CODEC_STATELESS_BASE + 601)
/**
 * struct v4l2_ctrl_vc1_entrypoint_header - VC-1 entry-point header parameters
 *
 * For the Advanced profile the members match the syntax elements of the
 * entry-point header, as specified by section 6.2 "Entry-point Header Syntax
 * and Semantics" of SMPTE 421M, and apply to all the pictures of the
 * entry-point segment. For the Simple and Main profiles there is no entry-point
 * layer; the members carry the equivalent syntax elements of STRUCT_C and the
 * coded picture size of STRUCT_A (Annex J and Annex L of SMPTE 421M), and the
 * Advanced profile only members and flags shall be zero.
 *
 * @dquant: DQUANT syntax element.
 * @quantizer: QUANTIZER syntax element, see V4L2_VC1_QUANTIZER_{}.
 * @coded_width: coded width of the pictures, in luma samples. For the Advanced
 * profile this is (CODED_WIDTH + 1) * 2 if CODED_SIZE_FLAG is set, the maximum
 * coded width of the sequence otherwise.
 * @coded_height: coded height of the pictures, in luma samples. For the
 * Advanced profile this is (CODED_HEIGHT + 1) * 2 if CODED_SIZE_FLAG is set,
 * the maximum coded height of the sequence otherwise.
 * @range_mapy: RANGE_MAPY syntax element. Advanced profile only, valid if
 * V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPY is set.
 * @range_mapuv: RANGE_MAPUV syntax element. Advanced profile only, valid if
 * V4L2_VC1_ENTRYPOINT_HEADER_FLAG_RANGE_MAPUV is set.
 * @flags: see V4L2_VC1_ENTRYPOINT_HEADER_FLAG_{}.
 */
struct v4l2_ctrl_vc1_entrypoint_header {
	__u8	dquant;
	__u8	quantizer;
	__u16	coded_width;
	__u16	coded_height;
	__u8	range_mapy;
	__u8	range_mapuv;
	__u32	flags;
};

#define V4L2_VC1_DQPROFILE_ALL_FOUR_EDGES		0
#define V4L2_VC1_DQPROFILE_DOUBLE_EDGES			1
#define V4L2_VC1_DQPROFILE_SINGLE_EDGE			2
#define V4L2_VC1_DQPROFILE_ALL_MBS			3

#define V4L2_VC1_VOPDQUANT_FLAG_DQUANTFRM		0x1
#define V4L2_VC1_VOPDQUANT_FLAG_DQBILEVEL		0x2

/**
 * struct v4l2_vc1_vopdquant - VC-1 VOPDQUANT parameters
 *
 * The members match the VOPDQUANT syntax elements of the picture layer, as
 * specified by section 7.1.1.31 of SMPTE 421M. All members are zero if
 * VOPDQUANT is not present in the picture header.
 *
 * @altpquant: alternative picture quantizer scale ALTPQUANT, derived from the
 * PQDIFF and ABSPQ syntax elements.
 * @dqprofile: DQPROFILE syntax element, see V4L2_VC1_DQPROFILE_{}.
 * @dqsbedge: DQSBEDGE syntax element, valid if @dqprofile is
 * V4L2_VC1_DQPROFILE_SINGLE_EDGE.
 * @dqdbedge: DQDBEDGE syntax element, valid if @dqprofile is
 * V4L2_VC1_DQPROFILE_DOUBLE_EDGES.
 * @flags: see V4L2_VC1_VOPDQUANT_FLAG_{}.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_vc1_vopdquant {
	__u8	altpquant;
	__u8	dqprofile;
	__u8	dqsbedge;
	__u8	dqdbedge;
	__u8	flags;
	__u8	reserved[3];
};

#define V4L2_VC1_PICTURE_TYPE_I				0
#define V4L2_VC1_PICTURE_TYPE_P				1
#define V4L2_VC1_PICTURE_TYPE_B				2
#define V4L2_VC1_PICTURE_TYPE_BI			3
/* Only used to describe a reference picture, see struct v4l2_vc1_reference */
#define V4L2_VC1_PICTURE_TYPE_SKIPPED			4

#define V4L2_VC1_FPTYPE_I_I				0
#define V4L2_VC1_FPTYPE_I_P				1
#define V4L2_VC1_FPTYPE_P_I				2
#define V4L2_VC1_FPTYPE_P_P				3
#define V4L2_VC1_FPTYPE_B_B				4
#define V4L2_VC1_FPTYPE_B_BI				5
#define V4L2_VC1_FPTYPE_BI_B				6
#define V4L2_VC1_FPTYPE_BI_BI				7

#define V4L2_VC1_FCM_PROGRESSIVE			0
#define V4L2_VC1_FCM_FRAME_INTERLACE			1
#define V4L2_VC1_FCM_FIELD_INTERLACE			2

#define V4L2_VC1_MVMODE_1MV_HPEL_BILIN			0
#define V4L2_VC1_MVMODE_1MV				1
#define V4L2_VC1_MVMODE_1MV_HPEL			2
#define V4L2_VC1_MVMODE_MIXED_MV			3
#define V4L2_VC1_MVMODE_INTENSITY_COMP			4

#define V4L2_VC1_INTCOMPFIELD_BOTH			0
#define V4L2_VC1_INTCOMPFIELD_TOP			1
#define V4L2_VC1_INTCOMPFIELD_BOTTOM			2

#define V4L2_VC1_CONDOVER_NONE				0
#define V4L2_VC1_CONDOVER_ALL				1
#define V4L2_VC1_CONDOVER_SELECT			2

#define V4L2_VC1_TTFRM_8X8				0
#define V4L2_VC1_TTFRM_8X4				1
#define V4L2_VC1_TTFRM_4X8				2
#define V4L2_VC1_TTFRM_4X4				3

/* Number of entries of Table 40 "BFRACTION VLC Table" that code a fraction */
#define V4L2_VC1_BFRACTION_NUM				21

#define V4L2_VC1_PICTURE_LAYER_FLAG_RANGEREDFRM		0x0001
#define V4L2_VC1_PICTURE_LAYER_FLAG_HALFQP		0x0002
#define V4L2_VC1_PICTURE_LAYER_FLAG_PQUANTIZER		0x0004
#define V4L2_VC1_PICTURE_LAYER_FLAG_TRANSDCTAB		0x0008
#define V4L2_VC1_PICTURE_LAYER_FLAG_TFF			0x0010
#define V4L2_VC1_PICTURE_LAYER_FLAG_RNDCTRL		0x0020
#define V4L2_VC1_PICTURE_LAYER_FLAG_TTMBF		0x0040
#define V4L2_VC1_PICTURE_LAYER_FLAG_4MVSWITCH		0x0080
#define V4L2_VC1_PICTURE_LAYER_FLAG_INTCOMP		0x0100
#define V4L2_VC1_PICTURE_LAYER_FLAG_NUMREF		0x0200
#define V4L2_VC1_PICTURE_LAYER_FLAG_REFFIELD		0x0400
#define V4L2_VC1_PICTURE_LAYER_FLAG_SECOND_FIELD	0x0800
#define V4L2_VC1_PICTURE_LAYER_FLAG_RFF			0x1000
#define V4L2_VC1_PICTURE_LAYER_FLAG_INTERPFRM		0x2000
#define V4L2_VC1_PICTURE_LAYER_FLAG_UVSAMP		0x4000

#define V4L2_VC1_RAW_CODING_FLAG_MVTYPEMB		0x01
#define V4L2_VC1_RAW_CODING_FLAG_DIRECTMB		0x02
#define V4L2_VC1_RAW_CODING_FLAG_SKIPMB			0x04
#define V4L2_VC1_RAW_CODING_FLAG_FIELDTX		0x08
#define V4L2_VC1_RAW_CODING_FLAG_FORWARDMB		0x10
#define V4L2_VC1_RAW_CODING_FLAG_ACPRED			0x20
#define V4L2_VC1_RAW_CODING_FLAG_OVERFLAGS		0x40

/* Number of intensity compensations a reference field can accumulate */
#define V4L2_VC1_REFERENCE_NUM_INTCOMP			2

/**
 * struct v4l2_vc1_intcomp - VC-1 intensity compensation parameters
 *
 * @lumscale: LUMSCALE syntax element.
 * @lumshift: LUMSHIFT syntax element.
 */
struct v4l2_vc1_intcomp {
	__u8	lumscale;
	__u8	lumshift;
};

#define V4L2_VC1_REFERENCE_FLAG_RANGEREDFRM		0x01
#define V4L2_VC1_REFERENCE_FLAG_TFF			0x02

/**
 * struct v4l2_vc1_reference - VC-1 reference picture description
 *
 * State of a reference picture which the decoding of the current picture
 * depends on. It is taken from the headers of the reference picture and of
 * the pictures decoded since, so that drivers do not have to remember anything
 * from one decode request to the next. In the arrays, index 0 is the top field
 * and index 1 the bottom field of the reference frame; for a reference which
 * is a progressive or frame-interlaced picture both elements are equal.
 *
 * @fcm: frame coding mode of the reference, see V4L2_VC1_FCM_{}.
 * @ptype: picture type of each field of the reference, see
 * V4L2_VC1_PICTURE_TYPE_{}. V4L2_VC1_PICTURE_TYPE_SKIPPED designates a
 * skipped picture: it was not decoded, the capture buffer is the one of the
 * picture which the skipped picture repeats and its motion vectors, as used by
 * the direct mode, are zero.
 * @refdist: REFDIST syntax element of the reference. Field-interlaced
 * references only.
 * @flags: see V4L2_VC1_REFERENCE_FLAG_{}.
 * @num_intcomp: number of valid elements of @intcomp for each field of the
 * reference.
 * @reserved: padding field. Should be zeroed by applications.
 * @intcomp: intensity compensations which the pictures decoded before the
 * current one have applied to each field of the reference, in the order in
 * which they are applied when the field is read. The intensity compensation
 * signalled by the current picture is not included: it is applied last.
 */
struct v4l2_vc1_reference {
	__u8	fcm;
	__u8	ptype[2];
	__u8	refdist;
	__u8	flags;
	__u8	num_intcomp[2];
	__u8	reserved;
	struct v4l2_vc1_intcomp intcomp[2][V4L2_VC1_REFERENCE_NUM_INTCOMP];
};

#define V4L2_VC1_BITPLANE_FLAG_MVTYPEMB			0x01
#define V4L2_VC1_BITPLANE_FLAG_DIRECTMB			0x02
#define V4L2_VC1_BITPLANE_FLAG_SKIPMB			0x04
#define V4L2_VC1_BITPLANE_FLAG_FIELDTX			0x08
#define V4L2_VC1_BITPLANE_FLAG_FORWARDMB		0x10
#define V4L2_VC1_BITPLANE_FLAG_ACPRED			0x20
#define V4L2_VC1_BITPLANE_FLAG_OVERFLAGS		0x40

#define V4L2_CID_STATELESS_VC1_PICTURE_LAYER (V4L2_CID_CODEC_STATELESS_BASE + 602)
/**
 * struct v4l2_ctrl_vc1_picture_layer - VC-1 picture layer parameters
 *
 * The members match the syntax elements of the picture layer of the coded
 * picture (a progressive frame, an interlaced frame or one field of a pair of
 * interlaced fields) held by the OUTPUT buffer, as specified by sections 7.1
 * and 9.1 of SMPTE 421M. The two fields of a field-interlaced frame are two
 * decode requests, each with its own complete control: for a field the
 * members are a combination of the frame-level syntax elements, which the
 * bitstream only carries in front of the first field, and of the field-level
 * syntax elements.
 * Syntax elements which are not present in the picture header shall be set
 * to zero, unless stated otherwise. Skipped pictures have no macroblock layer
 * and are not submitted for decoding: the application repeats their reference
 * picture.
 *
 * @backward_ref_ts: timestamp of the V4L2 capture buffer to use as backward
 * reference, used with B-coded pictures.
 * @forward_ref_ts: timestamp of the V4L2 capture buffer to use as forward
 * reference, used with P-coded and B-coded pictures. These timestamps refer
 * to the timestamp field in struct v4l2_buffer. Use v4l2_timeval_to_ns() to
 * convert the struct timeval to a __u64. The first field of the frame being
 * decoded is never referred to by timestamp: a reference to it is implied
 * when decoding the second field.
 * @data_bit_offset: offset in bits from the first bit of the picture layer
 * (for the Advanced profile the bit following the 32-bit frame or field start
 * code which begins the OUTPUT buffer, for the Simple and Main profiles the
 * first bit of the OUTPUT buffer) to the first bit of the first macroblock
 * layer. The offset is counted on the unescaped bitstream: the emulation
 * prevention bytes are not counted. The position of that bit in the OUTPUT
 * buffer of an Advanced profile picture is therefore
 * 32 + @data_bit_offset + 8 * @header_emulation_bytes bits.
 * @flags: see V4L2_VC1_PICTURE_LAYER_FLAG_{}.
 * @header_emulation_bytes: number of emulation prevention bytes present in the
 * OUTPUT buffer between the start code and the byte holding the first bit of
 * the first macroblock layer. Always zero for the Simple and Main profiles.
 * @ptype: picture type of the frame or of the field being decoded, see
 * V4L2_VC1_PICTURE_TYPE_{}. V4L2_VC1_PICTURE_TYPE_SKIPPED is not allowed.
 * @fptype: FPTYPE syntax element of the frame the field belongs to, see
 * V4L2_VC1_FPTYPE_{}. Field-interlaced frames only.
 * @fcm: frame coding mode, see V4L2_VC1_FCM_{}. Always progressive for the
 * Simple and Main profiles.
 * @pqindex: PQINDEX syntax element.
 * @pquant: picture quantizer scale PQUANT derived from @pqindex and from the
 * QUANTIZER syntax element as specified by section 7.1.1.6 of SMPTE 421M.
 * @mvrange: MVRANGE syntax element.
 * @dmvrange: DMVRANGE syntax element.
 * @respic: RESPIC syntax element. Simple and Main profiles only, coded in I
 * and P pictures; B and BI pictures take the value of the last I or P picture.
 * Bit 0 halves the coded width, bit 1 the coded height.
 * @transacfrm: TRANSACFRM syntax element.
 * @transacfrm2: TRANSACFRM2 syntax element.
 * @bfraction: index of the BFRACTION syntax element in Table 40 "BFRACTION
 * VLC Table" of SMPTE 421M, in the range 0 to V4L2_VC1_BFRACTION_NUM - 1.
 * The indices 0 to 20 stand for the fractions 1/2, 1/3, 2/3, 1/4, 3/4, 1/5,
 * 2/5, 3/5, 4/5, 1/6, 5/6, 1/7, 2/7, 3/7, 4/7, 5/7, 6/7, 1/8, 3/8, 5/8 and
 * 7/8.
 * @mvmode: MVMODE syntax element, see V4L2_VC1_MVMODE_{}.
 * @mvmode2: MVMODE2 syntax element, see V4L2_VC1_MVMODE_{}. Valid if @mvmode
 * is V4L2_VC1_MVMODE_INTENSITY_COMP.
 * @lumscale: LUMSCALE syntax element of a progressive or frame-interlaced
 * picture, LUMSCALE1 syntax element of a field picture.
 * @lumshift: LUMSHIFT syntax element of a progressive or frame-interlaced
 * picture, LUMSHIFT1 syntax element of a field picture.
 * @lumscale2: LUMSCALE2 syntax element. Field pictures only.
 * @lumshift2: LUMSHIFT2 syntax element. Field pictures only.
 * @intcompfield: INTCOMPFIELD syntax element, see V4L2_VC1_INTCOMPFIELD_{}.
 * Field pictures only, valid if @mvmode is V4L2_VC1_MVMODE_INTENSITY_COMP.
 * @mvtab: MVTAB syntax element.
 * @cbptab: CBPTAB syntax element.
 * @mbmodetab: MBMODETAB syntax element.
 * @imvtab: IMVTAB syntax element.
 * @icbptab: ICBPTAB syntax element.
 * @twomvbptab: 2MVBPTAB syntax element.
 * @fourmvbptab: 4MVBPTAB syntax element.
 * @ttfrm: TTFRM syntax element, see V4L2_VC1_TTFRM_{}.
 * @refdist: REFDIST syntax element. B-coded field pictures do not carry it:
 * the REFDIST which their decoding uses is the one of @backward_ref.
 * @condover: CONDOVER syntax element, see V4L2_VC1_CONDOVER_{}. It is not
 * coded when PQUANT is 9 or more: overlap smoothing then applies to all the
 * macroblocks of the I, BI and P pictures of a sequence with OVERLAP set.
 * @postproc: POSTPROC syntax element.
 * @rptfrm: RPTFRM syntax element.
 * @raw_coding_flags: bitplanes of the picture which are coded in raw mode,
 * that is, at the macroblock layer. See V4L2_VC1_RAW_CODING_FLAG_{}.
 * @bitplane_flags: bitplanes of the picture which are coded in the picture
 * header and passed, decoded, through struct v4l2_ctrl_vc1_bitplanes. See
 * V4L2_VC1_BITPLANE_FLAG_{}. A bitplane is never both raw coded and passed
 * through the control.
 * @reserved: padding field. Should be zeroed by applications.
 * @vopdquant: see struct v4l2_vc1_vopdquant.
 * @forward_ref: description of the forward reference, see struct
 * v4l2_vc1_reference. Used with P-coded and B-coded pictures.
 * @backward_ref: description of the backward reference, see struct
 * v4l2_vc1_reference. Used with B-coded pictures.
 */
struct v4l2_ctrl_vc1_picture_layer {
	__u64	backward_ref_ts;
	__u64	forward_ref_ts;
	__u32	data_bit_offset;
	__u32	flags;
	__u16	header_emulation_bytes;
	__u8	ptype;
	__u8	fptype;
	__u8	fcm;
	__u8	pqindex;
	__u8	pquant;
	__u8	mvrange;
	__u8	dmvrange;
	__u8	respic;
	__u8	transacfrm;
	__u8	transacfrm2;
	__u8	bfraction;
	__u8	mvmode;
	__u8	mvmode2;
	__u8	lumscale;
	__u8	lumshift;
	__u8	lumscale2;
	__u8	lumshift2;
	__u8	intcompfield;
	__u8	mvtab;
	__u8	cbptab;
	__u8	mbmodetab;
	__u8	imvtab;
	__u8	icbptab;
	__u8	twomvbptab;
	__u8	fourmvbptab;
	__u8	ttfrm;
	__u8	refdist;
	__u8	condover;
	__u8	postproc;
	__u8	rptfrm;
	__u8	raw_coding_flags;
	__u8	bitplane_flags;
	__u8	reserved[6];
	struct v4l2_vc1_vopdquant vopdquant;
	struct v4l2_vc1_reference forward_ref;
	struct v4l2_vc1_reference backward_ref;
};

/* Size in bytes of a bitplane: one bit per macroblock, up to 16384 macroblocks */
#define V4L2_VC1_BITPLANE_SIZE				2048

#define V4L2_CID_STATELESS_VC1_BITPLANES (V4L2_CID_CODEC_STATELESS_BASE + 603)
/**
 * struct v4l2_ctrl_vc1_bitplanes - VC-1 decoded bitplanes
 *
 * Bitplanes of the picture layer, decoded as specified by section 8.7
 * "Bitplane Coding" of SMPTE 421M. Each bitplane holds one bit per macroblock
 * of the picture (of the field, for field pictures) in raster scan order
 * without any padding between macroblock rows: the bit of macroblock n is
 * bit (n % 8) of byte (n / 8), bit 0 being the least significant bit. Unused
 * bits shall be zero.
 *
 * A bitplane is valid if the matching flag is set in
 * &v4l2_ctrl_vc1_picture_layer.bitplane_flags, which is the case if it is
 * present in the picture header and not coded in raw mode. The content of the
 * other bitplanes is ignored. This control only needs to be set for the
 * pictures which have at least one valid bitplane.
 *
 * @mvtypemb: MVTYPEMB bitplane.
 * @directmb: DIRECTMB bitplane.
 * @skipmb: SKIPMB bitplane.
 * @fieldtx: FIELDTX bitplane.
 * @forwardmb: FORWARDMB bitplane.
 * @acpred: ACPRED bitplane.
 * @overflags: OVERFLAGS bitplane.
 */
struct v4l2_ctrl_vc1_bitplanes {
	__u8	mvtypemb[V4L2_VC1_BITPLANE_SIZE];
	__u8	directmb[V4L2_VC1_BITPLANE_SIZE];
	__u8	skipmb[V4L2_VC1_BITPLANE_SIZE];
	__u8	fieldtx[V4L2_VC1_BITPLANE_SIZE];
	__u8	forwardmb[V4L2_VC1_BITPLANE_SIZE];
	__u8	acpred[V4L2_VC1_BITPLANE_SIZE];
	__u8	overflags[V4L2_VC1_BITPLANE_SIZE];
};

#define V4L2_VC1_SLICE_PARAMS_FLAG_PIC_HEADER		0x01

#define V4L2_CID_STATELESS_VC1_SLICE_PARAMS (V4L2_CID_CODEC_STATELESS_BASE + 604)
/**
 * struct v4l2_ctrl_vc1_slice_params - VC-1 slice parameters
 *
 * Location of a slice of the coded picture held by the OUTPUT buffer. This
 * control is a dynamically sized array with one element per slice of the
 * picture, in bitstream order. The first element describes the slice which
 * begins with the picture layer. The following elements describe the slices
 * which begin with a slice start code, as specified by section 7.1.2 "Slice
 * Layer" of SMPTE 421M. Drivers which expose this control need it to be set
 * with every request, also for pictures with a single slice.
 *
 * @offset: offset in bytes from the beginning of the OUTPUT buffer to the
 * first byte of the start code of the slice. Zero for the first slice.
 * @size: size in bytes of the slice, including its start code and its
 * emulation prevention bytes.
 * @data_bit_offset: offset in bits from the bit following the start code of
 * the slice to the first bit of the first macroblock layer of the slice, that
 * is, the size in bits of the slice header including the picture header it
 * carries if V4L2_VC1_SLICE_PARAMS_FLAG_PIC_HEADER is set. The offset is
 * counted on the unescaped bitstream: the emulation prevention bytes are not
 * counted. The position of that bit in the OUTPUT buffer is therefore
 * 8 * @offset + 32 + @data_bit_offset + 8 * @header_emulation_bytes bits.
 * For the first slice this is equal to
 * &v4l2_ctrl_vc1_picture_layer.data_bit_offset.
 * @header_emulation_bytes: number of emulation prevention bytes present in the
 * OUTPUT buffer between the start code of the slice and the byte holding the
 * first bit of its first macroblock layer.
 * @slice_addr: SLICE_ADDR syntax element. Zero for the first slice.
 * @flags: see V4L2_VC1_SLICE_PARAMS_FLAG_{}.
 * @reserved: padding field. Should be zeroed by applications.
 */
struct v4l2_ctrl_vc1_slice_params {
	__u32	offset;
	__u32	size;
	__u32	data_bit_offset;
	__u16	header_emulation_bytes;
	__u16	slice_addr;
	__u8	flags;
	__u8	reserved[3];
};

#endif /* V4L2_CID_STATELESS_VC1_SEQUENCE */

#ifndef V4L2_CID_MPEG_VIDEO_VC1_PROFILE
#define V4L2_CID_MPEG_VIDEO_VC1_PROFILE	(V4L2_CID_CODEC_BASE + 658)
enum v4l2_mpeg_video_vc1_profile {
	V4L2_MPEG_VIDEO_VC1_PROFILE_SIMPLE	= 0,
	V4L2_MPEG_VIDEO_VC1_PROFILE_MAIN	= 1,
	V4L2_MPEG_VIDEO_VC1_PROFILE_ADVANCED	= 2,
};
#define V4L2_CID_MPEG_VIDEO_VC1_LEVEL	(V4L2_CID_CODEC_BASE + 659)
enum v4l2_mpeg_video_vc1_level {
	V4L2_MPEG_VIDEO_VC1_LEVEL_LOW		= 0,
	V4L2_MPEG_VIDEO_VC1_LEVEL_MEDIUM	= 1,
	V4L2_MPEG_VIDEO_VC1_LEVEL_HIGH		= 2,
	V4L2_MPEG_VIDEO_VC1_LEVEL_0		= 3,
	V4L2_MPEG_VIDEO_VC1_LEVEL_1		= 4,
	V4L2_MPEG_VIDEO_VC1_LEVEL_2		= 5,
	V4L2_MPEG_VIDEO_VC1_LEVEL_3		= 6,
	V4L2_MPEG_VIDEO_VC1_LEVEL_4		= 7,
};
#endif /* V4L2_CID_MPEG_VIDEO_VC1_PROFILE */

#endif /* AVCODEC_V4L2_STATELESS_VC1_H */
