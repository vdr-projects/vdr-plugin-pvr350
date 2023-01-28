/*
 * pvr350audio.h:
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __PVR350_AUDIO_H
#define __PVR350_AUDIO_H

extern "C" {
#include <a52dec/a52.h>
#include <a52dec/mm_accel.h>
#define LIBTWOLAME_STATIC
#include <twolame.h>
#include <mpg123.h>
}

// PES header offsets
#define PESPACKETSTARTCODE1	0
#define PESPACKETSTARTCODE2	1
#define PESPACKETSTARTCODE3	2
#define PESSTREAMID		3
#define PESPACKETLEN1		4
#define PESPACKETLEN2		5
#define PESEXTENSION1		6
#define PESEXTENSION2		7
#define PESHEADERLENGTH		8

// PES header length
#define PESFIXHEADERLEN		6
#define PESMPEG2EXTLEN		3
#define PESMPEG2HEADLEN		9	// PESFIXHEADERLEN + PESMPEG2EXTLEN

#define SUBSTREAM_AC3_STREAMID	0
#define SUBSTREAM_AC3_FRAMECNT	1
#define SUBSTREAM_AC3_FIRSTACC1	2
#define SUBSTREAM_AC3_FIRSTACC2	3

#define AC3_INBUF_SIZE		4096
#define AC3_HEADER_SIZE		7
#define AC3_IDENTIFIER_BYTES	4

#define PCM_BUFFER_SIZE		(73728 + 1)	// 72kb
#define PCM_IDENTIFIER_BYTES	7

#define MP2_PES_DATA_SIZE	16384		// 16kb
#define MP2_PES_BUFFER_SIZE	(MP2_PES_DATA_SIZE + PESMPEG2HEADLEN + 1)

#define AUDIO_CHANNELS		2
#define BIAS			384
#define MPEG_AUDIO_STREAM_ID	0xC0

#define AUDIO_SAMPLERATE	48000
#define MP2_BITRATE		256
#define MP2_HIGHQUALITY		2
#define MP2_MEDIUMQUALITY	5
#define MP2_LOWQUALITY		7

#define SUBSTREAM_MP2_LENGTH	4

// Offsets for MP3 Audio Substream Header
#define SUBSTREAM_MP2_SYNC	0
#define SUBSTREAM_MP2_VERSION	1
#define SUBSTREAM_MP2_BITRATE	2
#define SUBSTREAM_MP2_MODE	3

#define MPEG_AUDIO_SYNC		0xFF
#define MPEG_AUDIO_VERSION	0xFA	// MPEG-1 Layer III (CRC protected)
#define MPEG_AUDIO_BITRATE	0xC4	// 256kbps 48000 Hz
#define MPEG_AUDIO_MODE		0x00

typedef struct	AC3DecodeState {
	uint8_t		inbuf[AC3_INBUF_SIZE];		// input buffer
	uint8_t		*inbuf_ptr;
	int		frame_size;
	int		flags;
	int		channels;
	a52_state_t	*state;
	sample_t	*samples;
	int		sample_rate;
	int		bit_rate;
} AC3DecodeState_t;

#pragma pack(1)
typedef struct MPEG2PES {
unsigned sync1:
	8;
unsigned sync2:
	8;
unsigned sync3:
	8;
unsigned stream_id:
	8;
unsigned packet_length_H:
	8;
unsigned packet_length_L:
	8;
unsigned original:
	1;	/* 1 implies original, 0 implies copy */
unsigned copyright:
	1;	/* 1 implies copyrighted */
unsigned data_alignment_indicators:
	1;	/* 1 indicates that the PES packet header is immediately followed by the video start code or audio syncword */
unsigned priority:
	1;
unsigned scrambling_control:
	2;
unsigned marker_bits:
	2;	/* 10 = MPEG-2  */
unsigned extension_flag:
	1;
unsigned CRC_flag:
	1;
unsigned additional_copy_info_flag:
	1;
unsigned DSM_trick_mode_flag:
	1;
unsigned ES_rate_flag:
	1;
unsigned ESCR_flag:
	1;
unsigned PTS_DTS_indicator:
	2;	/* 11 = both present, 10 = only PTS */
unsigned header_length:
	8;
} MPEG2PES_t;
#pragma pack()

#pragma pack(1)
typedef struct AC3HDR {
unsigned sync1:
	8;
unsigned sync2:
	8;
unsigned crc_H:
	8;
unsigned crc_L:
	8;
unsigned frmsizcod:
	6;
unsigned fscod:
	2;
unsigned bsmod:
	3;
unsigned bsid:
	5;
unsigned LFE_Mix_VarField:
	5;
unsigned acmod:
	3;
} AC3HDR_t;
#pragma pack()


class cAC3toMP2
{
private:
	bool		m_Initialized;
	int		m_AC3Channels;
	int		disable_adjust;
	int		disable_dynrng;
	float		scale;
	twolame_options	*encodeOptions;
	AC3DecodeState_t        *m_AC3DecodeStatePtr;

protected:
	void	float2s16(float *_f, int16_t *s16, int nchannels);
	int	convert2wav(sample_t *_samples, int16_t *out_samples, int channels);
	bool	A52DecodeInit(AC3DecodeState_t *s);
	void	A52DecodeEnd(AC3DecodeState_t *s);
	bool	MP2InitLibrary();
	void	MP2CloseLibrary();

public:
	cAC3toMP2(AC3DecodeState_t *s, int scale_value);
	~cAC3toMP2();
	int	scale_value;
	int 	A52DecodeFrame(AC3DecodeState_t *s, int16_t *outbuf, int *outbuf_size,
				uint8_t *inbuf, int inbuf_bytes);
	int 	MP2EncodeFrame(int16_t *inbuf, int inbuf_bytes, uint8_t *outbuf,
				int outbuf_size);
};

class cMP2toMP2
{
private:
	bool		m_Initialized;
	twolame_options	*encodeOptions;
	mpg123_handle   *MPG123HandlePtr;
	uint8_t 	*decode_buf;
	uint8_t 	*output_buf;

protected:
	bool	MP2InitLibrary();
	void	MP2CloseLibrary();
	bool	MPG123InitLibrary();
	void	MPG123CloseLibrary();

public:
	cMP2toMP2();
	~cMP2toMP2();
	int	MPG123DecodeFrame(uint8_t *inbuf, int inbuf_bytes,
				int16_t *outbuf, int outbuf_size,
				int *bytes_decoded);
	bool	MP2InitParams(int samplerate, int bitrate, int channels);
	int	MP2EncodeFrame(int16_t *inbuf, int inbuf_bytes,
				uint8_t *outbuf, int outbuf_size);
};
#endif
