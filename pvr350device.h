/*
 * pvr350device.h: 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __PVR350_DEVICE_H
#define __PVR350_DEVICE_H
#include <vdr/device.h>
#include <vdr/thread.h>
#include "pvr350osd.h"
#include "pvr350audio.h"

typedef enum {
  undef,
  video,
  audio_only
} eStreamtype;

class cPvr350Device: public cDevice {
private:
	int		fd_out;
	int		fbfd;
	int		fh;
	int		storedglobalalpha;
	ulong		initglobalalpha;
	int		fb_bpp;
	int		fb_pixel_size;
	int		fb_line_len;
	int		fb_size;
	int		osdbufsize;
	uint8_t		*osdbuffer;
	uint8_t		*osdbuf_aligned;
	int		stride;
	int		output_mode;
	bool		Format16_9;

	ePlayMode	m_PlayMode;
	cSpuDecoder	*spuDecoder;

	/* Current sizes of the mpeg video */
        int		aspectratio;
	int		horizontal_size;
	int		vertical_size;

	/* Current sizes of the display */
	int		current_horiz;
	int		current_vertical;
	int		current_afd;
	int		sizechanged;
	int		current_wss_data;
	int		lx, ly;
	int		width, height;

	int		audiomode;
	int 		framecount;
	bool		newStream;
        eStreamtype     streamtype;

	uint32_t	KernelVersion;
	uint8_t		m_PCMBuffer[PCM_BUFFER_SIZE];
	uint8_t		m_MP2PESBuffer[MP2_PES_BUFFER_SIZE];
	uint8_t		*m_PESHeader;
	uint8_t		*m_PESPacketData;
	cAC3toMP2	*m_AC3toMP2;
	bool		m_AC3toMP2Init;
	AC3DecodeState_t	m_AC3DecodeState;
	AC3DecodeState_t	*m_AC3DecodeStatePtr;
	cMP2toMP2	*m_MP2toMP2;
	bool		m_MP2RecodeInit;
	bool		DecEncMP2Audio;

	void SetVidInfo(const uchar *mbuf, int count);
	void OpenFramebuffer();
	void ProcessAC3Audio(uint8_t *PESPacket, int PESHeader_len, int Length);
	bool CheckMPEGAudio4JointStereo(const uchar *PESData, int Length);
	bool ConvertMP2Audio(uint8_t *PESPacket, int PESHeader_len, int Length);
	void ProcessMP2Audio(uint8_t *PESPacket, int PESHeader_len, int Length);

public:
	cPvr350Device(void);
	~cPvr350Device();
	void MakePrimaryDevice(bool On);
	virtual bool HasDecoder(void) const;

	virtual bool CanReplay(void) const;
	virtual bool SetPlayMode(ePlayMode PlayMode);
	virtual void TrickSpeed(int Speed);
	virtual void Clear(void);
	virtual void Play(void);
	virtual void Freeze(void);
	virtual void Mute(void);
        virtual int GetAudioChannelDevice(void);
	virtual void SetAudioChannelDevice(int AudioChannel);
	virtual void StillPicture(const uchar *Data, int Length);
	virtual int PlayVideo(const uchar *Data, int Length);
	virtual int PlayAudio(const uchar *Data, int Length, uchar Id);
	virtual void SetVideoFormat(bool VideoFormat16_9);
	virtual void SetVolumeDevice(int Volume);
#if APIVERSNUM >= 10708
	virtual void GetVideoSize(int &Width, int &Height, double &VideoAspect);
	virtual void GetOsdSize(int &Width, int &Height, double &PixelAspect);
#endif
	virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
	virtual cSpuDecoder *GetSpuDecoder(void);
	virtual int64_t GetSTC(void);
	void SetVideoSize(int x, int y, int w, int d);
	void Set_wss_mode(int wss_data);
	void DecoderStop(int blank);
	void DecoderPlay(int speed);
	void DecoderPaused(int paused);
};
#endif
