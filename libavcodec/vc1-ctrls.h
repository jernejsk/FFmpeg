/* SPDX-License-Identifier: GPL-2.0 */
/*
 * These are the VC1 state controls for use with stateless VC1 codec drivers.
 *
 * It turns out that these structs are not stable yet and will undergo
 * more changes. So keep them private until they are stable and ready to
 * become part of the official public API.
 */

#ifndef _VC1_CTRLS_H_
#define _VC1_CTRLS_H_

/* Our pixel format isn't stable at the moment */
#define V4L2_PIX_FMT_VC1_SLICE v4l2_fourcc('S', 'V', 'C', '1') /* VC1 parsed slices */

#define V4L2_CID_STATELESS_VC1_SLICE_PARAMS	(V4L2_CID_CODEC_STATELESS_BASE + 500)
#define V4L2_CID_STATELESS_VC1_BITPLANES	(V4L2_CID_CODEC_STATELESS_BASE + 501)

/* enum v4l2_ctrl_type type values */
#define V4L2_CTRL_TYPE_VC1_SLICE_PARAMS			0x0109
#define	V4L2_CTRL_TYPE_VC1_BITPLANES			0x010a

#define V4L2_VC1_SEQUENCE_FLAG_PULLDOWN			0x01
#define V4L2_VC1_SEQUENCE_FLAG_INTERLACE		0x02
#define V4L2_VC1_SEQUENCE_FLAG_TFCNTRFLAG		0x04
#define V4L2_VC1_SEQUENCE_FLAG_FINTERPFLAG		0x08
#define V4L2_VC1_SEQUENCE_FLAG_PSF			0x10

struct v4l2_vc1_sequence {
	__u8	profile;
	__u8	level;
	__u8	colordiff_format;
	__u32	flags;
};

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

struct v4l2_vc1_entrypoint_header {
	__u8	dquant;
	__u8	quantizer;
	__u16	coded_width;
	__u16	coded_height;
	__u8	range_mapy;
	__u8	range_mapuv;
	__u32	flags;
};

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

struct v4l2_vc1_picture_layer {
	__u8	ptype;
	__u8	pqindex;
	__u8	mvrange;
	__u8	respic;
	__u8	transacfrm;
	__u8	transacfrm2;
	__u8	bfraction;
	__u8	fcm;
	__u8	mvmode;
	__u8	mvmode2;
	__u8	lumscale;
	__u8	lumshift;
	__u8	lumscale2;
	__u8	lumshift2;
	__u8	mvtab;
	__u8	cbptab;
	__u8	intcompfield;
	__u8	dmvrange;
	__u8	mbmodetab;
	__u8	twomvbptab;
	__u8	fourmvbptab;
	__u8	ttfrm;
	__u8	refdist;
	__u8	condover;
	__u8	imvtab;
	__u8	icbptab;
	__u32	flags;
};

#define V4L2_VC1_VOPDQUANT_FLAG_DQUANTFRM		0x1
#define V4L2_VC1_VOPDQUANT_FLAG_DQBILEVEL		0x2

struct v4l2_vc1_vopdquant {
	__u8	altpquant;
	__u8	dqprofile;
	__u8	dqsbedge;
	__u8	dqdbedge;
	__u8	flags;
};

#define V4L2_VC1_METADATA_FLAG_MULTIRES		0x1
#define V4L2_VC1_METADATA_FLAG_SYNCMARKER	0x2
#define V4L2_VC1_METADATA_FLAG_RANGERED		0x4

struct v4l2_vc1_metadata {
	__u8	maxbframes;
	__u8	flags;
};

#define V4L2_VC1_RAW_CODING_FLAG_MVTYPEMB	0x01
#define V4L2_VC1_RAW_CODING_FLAG_DIRECTMB	0x02
#define V4L2_VC1_RAW_CODING_FLAG_SKIPMB		0x04
#define V4L2_VC1_RAW_CODING_FLAG_FIELDTX	0x08
#define V4L2_VC1_RAW_CODING_FLAG_FORWARDMB	0x10
#define V4L2_VC1_RAW_CODING_FLAG_ACPRED		0x20
#define V4L2_VC1_RAW_CODING_FLAG_OVERFLAGS	0x40

struct v4l2_ctrl_vc1_slice_params {
	__u32	bit_size;
	__u32	data_bit_offset;
	__u64	backward_ref_ts;
	__u64	forward_ref_ts;

	struct v4l2_vc1_sequence sequence;
	struct v4l2_vc1_entrypoint_header entrypoint_header;
	struct v4l2_vc1_picture_layer picture_layer;
	struct v4l2_vc1_vopdquant vopdquant;
	struct v4l2_vc1_metadata metadata;

	__u8	raw_coding_flags;
};

#define V4L2_VC1_BITPLANE_FLAG_MVTYPEMB		0x01
#define V4L2_VC1_BITPLANE_FLAG_DIRECTMB		0x02
#define V4L2_VC1_BITPLANE_FLAG_SKIPMB		0x04
#define V4L2_VC1_BITPLANE_FLAG_FIELDTX		0x08
#define V4L2_VC1_BITPLANE_FLAG_FORWARDMB	0x10
#define V4L2_VC1_BITPLANE_FLAG_ACPRED		0x20
#define V4L2_VC1_BITPLANE_FLAG_OVERFLAGS	0x40

struct v4l2_ctrl_vc1_bitplanes {
	__u8	bitplane_flags;

	__u8	mvtypemb[1024];
	__u8	directmb[1024];
	__u8	skipmb[1024];
	__u8	fieldtx[1024];
	__u8	forwardmb[1024];
	__u8	acpred[1024];
	__u8	overflags[1024];
};

#endif
