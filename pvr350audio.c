/*
 * pvr350audio.c: 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <vdr/tools.h>
#include "pvr350audio.h"
#include "pvr350tools.h"

// ********************************************
// ***************  AC3 -> MP2  ***************
// ********************************************

/**** The following functions are coming more or less from a52dec */
static inline int16_t Convert(int32_t i)
{
	if (i > 0x43c07fff)
		return 32767;
	else if (i < 0x43bf8000)
		return -32768;
	else
		return i - 0x43c00000;
}

void cAC3toMP2::float2s16(float *_f, int16_t *s16, int nchannels)
{
	int		i, j;
	int32_t	*f = (int32_t *)_f;	// XX assumes IEEE float format

	for (i = 0; i < 256; i++) {
		for (j = 0; j < nchannels; j++) {
			s16[nchannels * i + j] = Convert(f[i + j * 256]);
		}
	}
}

int cAC3toMP2::convert2wav(sample_t *_samples, int16_t *out_samples, int channels)
{
	float *samples = _samples;
	int out_sample_bytes = (256 * sizeof(int16_t) * channels);

	float2s16(samples, out_samples, channels);
	return out_sample_bytes;
}
/**** end */

cAC3toMP2::cAC3toMP2(AC3DecodeState_t *AC3DecodeStatePtr, int scale_value)
{
	m_Initialized = false;
	disable_adjust = 0;
	disable_dynrng = 0;
	scale = (float)scale_value;

	if (!A52DecodeInit(AC3DecodeStatePtr)) {
		log(pvrERROR, "pvr350: A52 init failed");
		return;
	}

	if (!MP2InitLibrary()) {
		log(pvrERROR, "pvr350: LAME init failed");
		A52DecodeEnd(AC3DecodeStatePtr);
		return;
	}

	m_AC3DecodeStatePtr = AC3DecodeStatePtr;
	m_Initialized = true;
}

cAC3toMP2::~cAC3toMP2()
{
	A52DecodeEnd(m_AC3DecodeStatePtr);
	MP2CloseLibrary();
	m_Initialized = false;
}

bool cAC3toMP2::A52DecodeInit(AC3DecodeState_t *s)
{
	s->state = a52_init(MM_ACCEL_DJBFFT);
	if (s->state == NULL) {
		return false;
	}
	s->samples = a52_samples(s->state);
	s->inbuf_ptr = s->inbuf;
	s->frame_size = 0;
	m_AC3Channels = AUDIO_CHANNELS;
	return true;
}

void cAC3toMP2::A52DecodeEnd(AC3DecodeState_t *s)
{
	a52_free(s->state);
	s->state = NULL;
}

int cAC3toMP2::A52DecodeFrame(AC3DecodeState_t *s,
			  int16_t *outbuf, int *outbuf_size,
			  uint8_t *inbuf, int inbuf_size)
{
	uint8_t *inbuf_ptr = inbuf;
	int16_t *outbuf_ptr = outbuf;
	int sample_rate, bit_rate;
	int flags, i, len, wav_size;
	float level;
	static const int ac3_channels[8] = {
		2, 1, 2, 3, 3, 4, 4, 5
	};

	if (!m_Initialized) {
		// Libraries not initialized !!!
		return -1;
	}

	*outbuf_size = 0;
	while (inbuf_size > 0) {
		len = s->inbuf_ptr - s->inbuf;	// number of bytes in input buffer
		if (s->frame_size == 0) {
			// no header seen : find one. We need a least 7 bytes to parse it
			len = AC3_HEADER_SIZE - len;
			if (len > inbuf_size)
				len = inbuf_size;
			memcpy(s->inbuf_ptr, inbuf_ptr, len);	//append data to input buffer
			inbuf_ptr += len;
			s->inbuf_ptr += len;
			inbuf_size -= len;
			if ((s->inbuf_ptr - s->inbuf) == AC3_HEADER_SIZE) {
				len = a52_syncinfo(s->inbuf, &s->flags, &sample_rate, &bit_rate);

				if (len == 0) {
					// no sync found : move by one byte (inefficient, but simple!)
#if 0
					for (bufptr = s->inbuf;
						bufptr < (s->inbuf + (AC3_HEADER_SIZE - 1));
						bufptr++) {
						bufptr[0] = bufptr[1];
					}
#else
					memcpy(s->inbuf, s->inbuf + 1, AC3_HEADER_SIZE - 1);
#endif
					s->inbuf_ptr--;
				} else {
					s->frame_size = len;	// AC3 frame length
					// update codec info
					s->sample_rate = sample_rate;
					s->channels = ac3_channels[s->flags & 7];
					if (s->flags & A52_LFE)
						s->channels++;
					if (m_AC3Channels == 0) {
						m_AC3Channels = s->channels;
					} else if (s->channels < m_AC3Channels) {
						log(pvrERROR, "pvr350: AC3 source channels are less than specified");
						m_AC3Channels = s->channels;
					}
					s->bit_rate = bit_rate;
				}
			}
		} else if (len < s->frame_size) {
			len = s->frame_size - len;
			if (len > inbuf_size)
				len = inbuf_size;
			memcpy(s->inbuf_ptr, inbuf_ptr, len);
			inbuf_ptr += len;
			s->inbuf_ptr += len;
			inbuf_size -= len;
		} else {
			flags = A52_DOLBY;
			if (!disable_adjust)
				flags |= A52_ADJUST_LEVEL;
			level = 1;
			if (a52_frame(s->state, s->inbuf, &flags, &level, BIAS)) {
				log(pvrERROR, "pvr350: Error decoding frame");
fail:
				s->inbuf_ptr = s->inbuf;
				s->frame_size = 0;
				continue;
			}
			if (disable_dynrng)
				a52_dynrng(s->state, NULL, NULL);

			wav_size = 0;
			for (i = 0; i < 6; i++) {
				int bytes;
				if (a52_block(s->state)) {
					log(pvrERROR, "pvr350: Error decoding block");
					goto fail;
				}
				bytes = convert2wav(s->samples, outbuf_ptr, m_AC3Channels);
				wav_size += bytes;
				outbuf_ptr += (bytes / 2);	// pointer to int16_t (short)
			}
			s->inbuf_ptr = s->inbuf;	// mark the input buffer as empty
			s->frame_size = 0;		// we need to look for a new header next time
			*outbuf_size = wav_size;
			break;
		}
	}
	return inbuf_ptr - inbuf;
}

void cAC3toMP2::MP2CloseLibrary()
{
	twolame_close(&encodeOptions);
	encodeOptions = NULL;
}

bool cAC3toMP2::MP2InitLibrary()
{
        encodeOptions = twolame_init();
	if (!encodeOptions) {
		log(pvrERROR, "pvr350: TwoLAME init failed!");
		return false;
	}

	// Set the samplerate of the PCM audio input. (Default: 44100)
	twolame_set_in_samplerate(encodeOptions, AUDIO_SAMPLERATE);
	// Set the samplerate of the MPEG audio output. (Default: 44100)
	twolame_set_out_samplerate(encodeOptions, AUDIO_SAMPLERATE);
	// Set the bitrate of the MPEG audio output stream. (Default: 192)
	twolame_set_bitrate(encodeOptions, MP2_BITRATE);
	// Set the number of channels in the input stream. (Default: 2)
	twolame_set_num_channels(encodeOptions, AUDIO_CHANNELS);
	// Set the scaling level for audio before encoding. Set to 0 to disable. (Default: 0)
	twolame_set_scale(encodeOptions, scale);

	if (twolame_init_params(encodeOptions) != 0) {
		MP2CloseLibrary();
		return false;
	} else {
		return true;
	}
}

int cAC3toMP2::MP2EncodeFrame(int16_t *inbuf, int inbuf_bytes, uint8_t *outbuf, int outbuf_size)
{
	int mp2_len = 0;
	int num_samples = (inbuf_bytes / sizeof(int16_t) / m_AC3Channels);

	if (!m_Initialized) {
		return -1;	// Libraries not initialized !!!
	}

	if (inbuf) {
		if (m_AC3Channels != AUDIO_CHANNELS) {
			mp2_len = twolame_encode_buffer(encodeOptions,
							inbuf,
							inbuf,
							num_samples,
							outbuf,
							outbuf_size);
		} else {
			mp2_len = twolame_encode_buffer_interleaved(encodeOptions,
							inbuf,
							num_samples,
							outbuf,
							outbuf_size);
		}
	} else {
		mp2_len = twolame_encode_flush(encodeOptions,
						outbuf,
					 	outbuf_size);
	}

	if (mp2_len < 0) {
		log(pvrERROR, "pvr350: TwoLAME output buffer too small");
	}

	return mp2_len;
}

// ********************************************
// ***************  MP2 -> MP2  ***************
// ********************************************

cMP2toMP2::cMP2toMP2()
{
	m_Initialized = false;

	// Init Decoder and Encoder
	if (!MPG123InitLibrary()) {
		log(pvrERROR, "pvr350: MPG123 init failed");
		return;
	}

	if (!MP2InitLibrary()) {
		log(pvrERROR, "pvr350: TwoLAME init failed");
		MPG123CloseLibrary();
		return;
	}
	
	m_Initialized = true;
}

cMP2toMP2::~cMP2toMP2()
{
	if (!m_Initialized) {
		return;
	}
	MPG123CloseLibrary();
	MP2CloseLibrary();
	m_Initialized = false;
}

void cMP2toMP2::MP2CloseLibrary()
{
	twolame_close(&encodeOptions);
	encodeOptions = NULL;
}

bool cMP2toMP2::MP2InitParams(int samplerate, int bitrate, int channels)
{
        twolame_close(&encodeOptions);
        encodeOptions = twolame_init();
        if (!encodeOptions) {
                log(pvrERROR, "pvr350: TwoLAME init failed!");
                return false;
        }
	// Set the samplerate of the PCM audio input. (Default: 44100)
	twolame_set_in_samplerate(encodeOptions, samplerate);
	// Set the samplerate of the MPEG audio output. (Default: 44100)
	twolame_set_out_samplerate(encodeOptions, samplerate);
	// Set the bitrate of the MPEG audio output stream. (Default: 192)
	twolame_set_bitrate(encodeOptions, bitrate);
	// Set the number of channels in the input stream. (Default: 2)
	twolame_set_num_channels(encodeOptions, channels);

	if (twolame_init_params(encodeOptions) != 0) {
		return false;
	} else {
		return true;
	}
}

bool cMP2toMP2::MP2InitLibrary()
{
	encodeOptions = twolame_init();
	if (!encodeOptions) {
		log(pvrERROR, "pvr350: TwoLAME init failed!");
		return false;
	}
	return true;
}

int cMP2toMP2::MP2EncodeFrame(int16_t *inbuf, int inbuf_bytes, uint8_t *outbuf, int outbuf_size)
{
	int mp2_len = 0;
	int num_samples = (inbuf_bytes / sizeof(int16_t) / AUDIO_CHANNELS);

	if (!m_Initialized) {
		return -1;	// Libraries not initialized !!!
	}

	if (inbuf) {
		mp2_len = twolame_encode_buffer_interleaved(encodeOptions,
							inbuf,
							num_samples,
							outbuf,
							outbuf_size);
	} else {
		mp2_len = twolame_encode_flush(encodeOptions,
						outbuf,
					 	outbuf_size);
	}

	if (mp2_len < 0) {
		log(pvrERROR, "pvr350: TwoLAME output buffer too small");
	}

	return mp2_len;
}

void cMP2toMP2::MPG123CloseLibrary()
{
	mpg123_delete(MPG123HandlePtr);
	mpg123_exit();
	MPG123HandlePtr = NULL;
}

bool cMP2toMP2::MPG123InitLibrary()
{
	int ret;
	int safe_buffer_size;

	if ((ret = mpg123_init()) != MPG123_OK) {
		log(pvrERROR, "pvr350: Initializing mpg123 failed: %d", ret);
		return false;
	}

	safe_buffer_size = mpg123_safe_buffer();
	if (safe_buffer_size > PCM_BUFFER_SIZE) {
		log(pvrERROR, "pvr350: PCM_BUFFER_SIZE=%d too small, should be %d",
			PCM_BUFFER_SIZE, safe_buffer_size);
		return false;
	}

	MPG123HandlePtr = mpg123_new(NULL, &ret);
	if (MPG123HandlePtr == NULL) {
		log(pvrERROR, "pvr350: Unable to create mpg123 handle: '%s'",
			mpg123_plain_strerror(ret));
		return false;
	}

	mpg123_param(MPG123HandlePtr, MPG123_FLAGS, MPG123_FORCE_STEREO, 0);
	ret = mpg123_open_feed(MPG123HandlePtr);
	if (ret == MPG123_ERR) {
		log(pvrERROR, "pvr350: mpg123_open_feed error '%s'",
			mpg123_strerror(MPG123HandlePtr));
		MPG123CloseLibrary();
		return false;
	} else {
		return true;
	}
}

int cMP2toMP2::MPG123DecodeFrame(uint8_t *inbuf, int inbuf_bytes, int16_t *outbuf, int outbuf_size, int *bytes_decoded)
{
	int ret;
	size_t bytes;
	int bytes_left;
	uint8_t *decbuf_ptr;

	if (!m_Initialized)  {
		return -1;	// Libraries not initialized !!!
	}

	decbuf_ptr = (uint8_t *)outbuf;

	/* Feed input chunk and get first chunk of decoded audio. */
	ret = mpg123_decode(MPG123HandlePtr, inbuf, inbuf_bytes,
				decbuf_ptr, outbuf_size, &bytes);
	*bytes_decoded = bytes;

	if (ret == MPG123_NEW_FORMAT) {
		long rate;
		int chan, enc;
		struct mpg123_frameinfo frameinfo;
		mpg123_getformat(MPG123HandlePtr, &rate, &chan, &enc);
		mpg123_info(MPG123HandlePtr, &frameinfo);
		log(pvrDEBUG1, "pvr350: MPG123 new format: %ld Hz, %d channels, %d bitrate, encoding value 0x%02x",
			rate, chan, frameinfo.bitrate, enc);
		if (!MP2InitParams(rate, frameinfo.bitrate, chan)) {
			log(pvrERROR, "pvr350: MP2InitParams failed");
			return -1;
		}
	}

	switch (ret) {
		case MPG123_DONE:
			bytes_left = 1;
			break;
		case MPG123_NEW_FORMAT:
			bytes_left = 1;
			break;
		case MPG123_OK:
			bytes_left = 1;
			break;
		case MPG123_NEED_MORE:
			bytes_left = 0;
			break;
		case MPG123_ERR:
		default:
			log(pvrERROR, "pvr350: mpg123_decode() error: '%s'",
				mpg123_strerror(MPG123HandlePtr));
			bytes_left = -1;
	}
	return bytes_left;
}
