/*
 * pvr350device.c:
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
#include <unistd.h>
#include <features.h>		/* Uses _GNU_SOURCE to define getsubopt in stdlib.h */
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <sys/utsname.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/user.h>
#include <sys/poll.h>
#include <linux/fb.h>

#include "pvr350device.h"
#include "pvr350blackvideo.h"

#include <vdr/dvbspu.h>
#include <vdr/status.h>
#include <vdr/device.h>
#include <vdr/tools.h>

#include <linux/videodev2.h>
#include <linux/ivtv.h>
#include <linux/ivtvfb.h>
#include <linux/version.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>

#include "pvr350tools.h"
#include "pvr350audio.h"
#include "pvr350setup.h"

/*uncomment the following for additional debugging */
//#define DEBUG

typedef unsigned long long u64;

static cPvr350Device  *Instance = 0;
static fb_var_screeninfo ivtvfb_var;
static fb_var_screeninfo ivtvfb_var_old;

/*
 * local function IOCTL
 * retries the ioctl given six times before giving up,
 * improves stability if device/driver is actually busy
 * */
static int IOCTL(int fd, int cmd, void *data)
{
	int retry;

	for (retry = 5; retry >= 0; ) {
		if (ioctl(fd, cmd, data) != 0) {
			if (retry) {
				usleep(20000); /* 20msec */
				retry--;
				continue;
			}
			return -1;
		} else {
			return 0;	/* all went okay :) */
		}
	}
	return 0;	/* should never reach this */
}


cPvr350Device::cPvr350Device(void)
    : fd_out(-1)
{
	Instance = this;
	struct v4l2_capability vcap;
	int dec_i;
        memset(&vcap, 0, sizeof(vcap));
	autodetect:
	char videodevName[64];
	if (Pvr350Setup.DeviceNumber == -1) {//no entry in setup.conf
		log(pvrINFO, "pvr350: Start autodetection of decoder device");
		for (dec_i = 16; dec_i < 20; dec_i++) {
			sprintf(videodevName, "/dev/video%d", dec_i);
			fd_out = open(videodevName, O_WRONLY | O_NONBLOCK);
			if (fd_out < 0) {
				log(pvrDEBUG1, "pvr350: Found no device on /dev/video%d", dec_i);
				continue;
				}
			ioctl(fd_out, VIDIOC_QUERYCAP, &vcap);
			if (vcap.capabilities & V4L2_CAP_VIDEO_OUTPUT_OVERLAY) {
				log(pvrINFO, "pvr350: Found decoder device on /dev/video%d", dec_i);
				Pvr350Setup.DeviceNumber = dec_i;
				break;
				}
			else {
				log(pvrDEBUG1, "pvr350: Found device without decoder on /dev/video%d", dec_i);
				continue;
				}
			close(fd_out);
			fd_out = -1;
			}
		}
	else {
		sprintf(videodevName, "/dev/video%d", Pvr350Setup.DeviceNumber);
		fd_out = open(videodevName, O_WRONLY | O_NONBLOCK);
		if (fd_out >= 0) {
			ioctl(fd_out, VIDIOC_QUERYCAP, &vcap);
			if (vcap.capabilities & V4L2_CAP_VIDEO_OUTPUT_OVERLAY) {
				log(pvrINFO, "pvr350: Found decoder device on /dev/video%d", Pvr350Setup.DeviceNumber);
				}
			else {
				log(pvrDEBUG1, "pvr350: Found device without decoder on /dev/video%d", Pvr350Setup.DeviceNumber);
				close(fd_out);
				fd_out = -1;
				}
			}
		if (fd_out < 0) {
			log(pvrERROR, "pvr350: Can't open decoder device on selected /dev/video%d, starting autodetection", \
				Pvr350Setup.DeviceNumber);
			Pvr350Setup.DeviceNumber = -1;
			goto autodetect;
			}
		}
	if (fd_out < 0) {
		log(pvrERROR, "pvr350: Can't open video decoder device");
		return;
		}
	OpenFramebuffer();

	m_AC3toMP2Init = false;
	m_AC3DecodeStatePtr = (AC3DecodeState_t *)&m_AC3DecodeState;
	audiomode = 0; //assume stereo setting
                       //Because ivtv sets AUDIO_MONO_LEFT for bilingual streams, we rely on vdr to set SetAudioChannel() to stereo 
	framecount = 0;
	newStream = true;
	streamtype = undef;
	m_MP2RecodeInit = false;
	DecEncMP2Audio = false;
}

void cPvr350Device::OpenFramebuffer()
{
	int ret;
	struct v4l2_framebuffer fbuf;
	int fbi;

	log(pvrDEBUG1, "cPvr350Device::OpenFramebuffer()");

	ioctl(fd_out, VIDIOC_G_FBUF, &fbuf);
	for (fbi = 0; fbi < 10; fbi++) {
		struct fb_fix_screeninfo si;
		char buf[10];
		sprintf(buf, "/dev/fb%d", fbi);
		fbfd = open(buf, O_RDWR);
		if (fbfd < 0)
			continue;
		ioctl(fbfd, FBIOGET_FSCREENINFO, &si);
 		if (si.smem_start == (unsigned long)fbuf.base)
			break;
		close(fbfd);
		fbfd = -1;
	}
	if (fbfd == -1) {
		log(pvrERROR, "pvr350: Cannot find framebuffer");
		_exit(1);
	}

	Format16_9 = true;

	struct v4l2_format fmt;

	if ((ret = ioctl(fd_out, VIDIOC_G_FBUF, &fbuf)) < 0) {
		log(pvrERROR, "pvr350: VIDIOC_G_FBUF error=%d:%s", errno, strerror(errno));
	}
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;

	if ((ret = ioctl(fd_out, VIDIOC_G_FMT, &fmt)) < 0) {
		log(pvrERROR, "pvr350: VIDIOC_G_FMT error=%d:%s", errno, strerror(errno));
	}

	log(pvrDEBUG1, "cPvr350Device::OpenFramebuffer() %d %d %d %x",
		fbuf.flags & V4L2_FBUF_FLAG_CHROMAKEY,
			fbuf.flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA,
			fbuf.flags & V4L2_FBUF_FLAG_LOCAL_ALPHA,
			fmt.fmt.win.global_alpha);


	fbuf.flags = V4L2_FBUF_FLAG_LOCAL_ALPHA;
	if ((ret = ioctl(fd_out, VIDIOC_S_FBUF, &fbuf)) < 0) {
		log(pvrERROR, "pvr350: VIDIOC_S_FBUF error=%d:%s", errno, strerror(errno));
	}

	fmt.fmt.win.global_alpha = 0;
	if ((ret = ioctl(fd_out, VIDIOC_S_FMT, &fmt)) < 0) {
		log(pvrERROR, "pvr350: VIDIOC_S_FMT error=%d:%s", errno, strerror(errno));
	}

	struct fb_fix_screeninfo ivtvfb_fix;
	memset(&ivtvfb_fix, 0, sizeof(ivtvfb_fix));

	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &ivtvfb_fix) < 0) {
		log(pvrERROR, "pvr350: FBIOGET_FSCREENINFO error");
	} else {
		// Take snapshot of current mode
		ioctl(fbfd, FBIOGET_VSCREENINFO, &ivtvfb_var_old);
		// Now set the display to 720x576x32
		memset(&ivtvfb_var, 0, sizeof(ivtvfb_var));
		ivtvfb_var.xres = ivtvfb_var.xres_virtual = 720;
		ivtvfb_var.yres = ivtvfb_var.yres_virtual = 576;
		ivtvfb_var.bits_per_pixel = 32;
		ivtvfb_var.activate = FB_ACTIVATE_NOW;
		if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &ivtvfb_var) < 0) {
			// Mode change failed. Maybe NTSC, so try again
			ivtvfb_var.yres = ivtvfb_var.yres_virtual = 480;
			if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &ivtvfb_var) < 0) {
				log(pvrERROR, "pvr350: FBIOPUT_VSCREENINFO error");
			}
		}
		osdbufsize = ivtvfb_fix.smem_len;
		stride = ivtvfb_var.xres * (ivtvfb_var.bits_per_pixel / 8);
	}

	osdbuffer = new unsigned char[osdbufsize + PAGE_SIZE];
	osdbuf_aligned = (unsigned char *)((intptr_t)osdbuffer + (PAGE_SIZE - 1));
	osdbuf_aligned = (unsigned char *)((intptr_t)osdbuf_aligned & PAGE_MASK);
	memset(osdbuf_aligned, 0x00, osdbufsize);
	lseek (fbfd, 0, SEEK_SET);
	if (write (fbfd, osdbuf_aligned, ivtvfb_var.xres * ivtvfb_var.yres * (ivtvfb_var.bits_per_pixel / 8)) < 0) {
		log(pvrERROR, "pvr350: OSD write failed error=%d:%s", errno, strerror(errno));
	}

	spuDecoder = NULL;
	ResetVideoSize();
}

cPvr350Device::~cPvr350Device()
{
	log(pvrDEBUG1, "pvr350: ~cPvr350Device()");
	delete spuDecoder;
	if (m_AC3toMP2Init) {
		delete m_AC3toMP2;
	}
	if (m_MP2RecodeInit) {
		delete m_MP2toMP2;
	}
	if (osdbuffer != NULL) {
		log(pvrDEBUG1, "pvr350: ~cPvr350Device() - delete osdbuffer");
		delete[] osdbuffer;
	}
	// restore old screen mode

	if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &ivtvfb_var_old) < 0) {
		log(pvrERROR, "pvr350: ~cPvr350Device() FBIOPUT_VSCREENINFO error=%d:%s", errno, strerror(errno));
	}
	// We're not explicitly closing any device files here, since this sometimes
	// caused segfaults. Besides, the program is about to terminate anyway...
	// (taken from dvbdevice.c)
}

void cPvr350Device::MakePrimaryDevice(bool On)
{
	log(pvrINFO, "cPvr350Device::MakePrimaryDevice(%s)", On ? "true" : "false");
	if (On) {
	  if (HasDecoder() && fbfd) {
		  new cPvr350OsdProvider(fbfd, osdbuffer);
	  }
	}
	cDevice::MakePrimaryDevice(On);
}

bool cPvr350Device::HasDecoder(void) const
{
	return fd_out >= 0; // We can decode MPEG2
}

bool cPvr350Device::CanReplay(void) const 
{
	return true;  // We can replay
}

bool cPvr350Device::SetPlayMode(ePlayMode PlayMode)
{
	log(pvrDEBUG1, "cPvr350Device::SetPlayMode() (%s)", \
		PlayMode == pmNone?"pmNone":
		PlayMode == pmAudioVideo?"pmAudioVideo":
		PlayMode == pmAudioOnly?"pmAudioOnly":
		PlayMode == pmAudioOnlyBlack?"pmAudioOnlyBlack":
		PlayMode == pmVideoOnly?"pmVideoOnly":"pmExtern_THIS_SHOULD_BE_AVOIDED");
	if (PlayMode != pmExtern_THIS_SHOULD_BE_AVOIDED && fd_out < 0) {
		current_wss_data=0;
		// reopen fd_out device
        	char videodevName[64];
		sprintf(videodevName, "/dev/video%d", Pvr350Setup.DeviceNumber);
		int retries = 20;
		retry:
		fd_out = open(videodevName, O_WRONLY | O_NONBLOCK);
		if (fd_out < 0) {
			log(pvrERROR, "pvr350: Can't reopen decoder /dev/video%d: %d:%s %s",
			  Pvr350Setup.DeviceNumber, strerror(errno), (--retries > 0) ? " - retrying" : "");
			if (retries > 0) {
			  usleep(100);
			  goto retry;
			  /* this will hopefully help in case of EBUSY if
			     mplayer or X closes the YUV device not fast enough */
			}
			return false;
		}
		SetVideoFormat(Setup.VideoFormat);
	}
	switch (PlayMode) {
		case pmNone:		// audio/video from decoder
			streamtype=undef;
			framecount = 0;
			newStream = true;
			DecoderStop(1);
			break;
		case  pmExtern_THIS_SHOULD_BE_AVOIDED:
			if (Pvr350Setup.WSS_169_for_pmExtern == 1) {
				Set_wss_mode(7);
			}
			close(fd_out);
			fd_out = -1;
			break;
		case pmAudioOnlyBlack:	// audio only from player, no video (black screen)
		case pmAudioOnly: 	// audio only from player, video from decoder
		case pmAudioVideo:	// audio/video from player
		default:
			DecoderPlay(0);
	}
	m_PlayMode = PlayMode;
	return true;
}

/*
	speed == 0 || speed == 1000: normal speed
	speed == 1: single step forward
	speed == -1: single step backward
	1 < speed < 1000: slow forward
	speed > 1000: fast forward
	speed == -1000: reverse play at normal speed
	-1000 < speed < -1: slow reverse
	speed < -1000: fast reverse.

	other description:

	0 or 1000 specifies normal speed,
	1 specifies forward single stepping,
	-1 specifies backward single stepping,
	>1: playback at speed/1000 of the normal speed,
	<-1: reverse playback at (-speed/1000) of the normal speed. 
*/

#if APIVERSNUM >= 20103
void cPvr350Device::TrickSpeed(int Speed, bool forward)
#else
void cPvr350Device::TrickSpeed(int Speed)
#endif
{
	log(pvrDEBUG1, "cPvr350Device::TrickSpeed() - Set speed %d", Speed);

	switch (Speed) {
		case 1 : // FastForward [3>>] or FastBackward [<<3]
			DecoderPlay(1000);
			break; 
		case 3 : // FastForward [2>>] or FastBackward [<<2]
			DecoderPlay(500);
			break;
		case 6 : // FastForward [1>>] or FastBackward [<<1]
			DecoderPlay(250);
			break;
		case 8 : // SlowForward [1|>]
			DecoderPlay(40);
			break; 
		case 4 : // SlowForward [2|>]
			DecoderPlay(80);
			break; 
		case 2 : // SlowForward [3|>]
			DecoderPlay(160);
			break;
		case 63 : // SlowReverse [<|1]
			DecoderPlay(40);
			break; 
		case 48 : // SlowReverse [<|2]
			DecoderPlay(80);
			break; 
		case 24 : // SlowReverse [<|3]
			DecoderPlay(160);
			break;
	}
}

void cPvr350Device::SetVideoFormat(bool VideoFormat16_9)
{
	log(pvrINFO, "cPvr350Device::SetVideoFormat(VideoFormat16_9=%s)",
		VideoFormat16_9 ? "true" : "false");
	Format16_9 = VideoFormat16_9;
}

void cPvr350Device::GetVideoSize(int &Width, int &Height, double &VideoAspect)
{
	if (fd_out >= 0) {
		Width  = horizontal_size;
		Height = vertical_size;
		switch (aspectratio) {
			default:
			case 2:   VideoAspect =  4.0 / 3.0; break;
			case 3:   VideoAspect = 16.0 / 9.0; break;
			case 4:   VideoAspect =       2.21; break;
		}
		return;
	}
	cDevice::GetVideoSize(Width, Height, VideoAspect);
	log(pvrDEBUG1, "cPvr350Device::GetVideoSize: Width=%d, Height=%d, VideoAspect=%f",
		Width, Height, VideoAspect);
}

void cPvr350Device::GetOsdSize(int &Width, int &Height, double &PixelAspect)
{
	Width = 720;
	Height = 576;
	if (fd_out >= 0) {
		switch (Setup.VideoFormat ? aspectratio : 2) {
			default:
			case 2:   PixelAspect =  4.0 / 3.0; break;
			case 3:
			case 4:   PixelAspect = 16.0 / 9.0; break;
		}
		PixelAspect /= double(Width) / Height;
		return;
	}
  	PixelAspect = 1.0; //use default vdr value for pmExtern
	log(pvrDEBUG1, "cPvr350Device::GetOsdSize: Width=%d, Height=%d, PixelAspect=%f",
		Width, Height, PixelAspect);
}

void cPvr350Device::DecoderStop(int blank)
{
	struct video_command cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = VIDEO_CMD_STOP;
	if (blank) {
		cmd.flags = VIDEO_CMD_STOP_TO_BLACK | VIDEO_CMD_STOP_IMMEDIATELY;
	} else { //show last frame instead of a black screen
		cmd.flags = VIDEO_CMD_STOP_IMMEDIATELY;
	}
	if (IOCTL(fd_out, VIDEO_COMMAND, &cmd) < 0) {
		log(pvrERROR, "pvr350: VIDEO_CMD_STOP %s error=%d:%s",
			blank ? "(blank)" : "", errno, strerror(errno));
	}
}

void cPvr350Device::DecoderPlay(int speed)
{
	struct video_command cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = VIDEO_CMD_PLAY;
	if (speed) {
		cmd.play.speed = speed;
	}
	if (IOCTL(fd_out, VIDEO_COMMAND, &cmd) < 0) {
		log(pvrERROR, "pvr350: VIDEO_CMD_START (speed=%d) error=%d:%s",
			speed, errno, strerror(errno));
	}
}

void cPvr350Device::DecoderPaused(int paused)
{
	struct video_command cmd;
	memset(&cmd, 0, sizeof(cmd));
	if (paused) {
		cmd.cmd = VIDEO_CMD_FREEZE;
	} else {
		cmd.cmd = VIDEO_CMD_CONTINUE;
	}
	if (ioctl(fd_out, VIDEO_COMMAND, &cmd) < 0) {
		log(pvrERROR, "pvr350: %s error=%d:%s",
			paused ? "VIDEO_CMD_FREEZE" : "VIDEO_CMD_CONTINUE",
			errno, strerror(errno));
	}
}

void cPvr350Device::Clear(void)
{
	log(pvrDEBUG1, "cPvr350Device::Clear()");
	DecoderStop(0); //stop, clear buffers and show last frame
	DecoderPlay(0); //resume play/decoding
	cDevice::Clear();
	///< A derived class must call the base class function to make sure
	///< all registered cAudio objects are notified.
}

void cPvr350Device::Play(void)
{
	log(pvrDEBUG1, "cPvr350Device::(Resume) Playback");
	DecoderPlay(1000); //normal speed
	//we cannot use VIDEO_CMD_CONTINUE: Leaving slow speed (trickmode) must reset to normal speed
}

void cPvr350Device::Freeze(void)
{
	log(pvrDEBUG1, "cPvr350Device::Freeze()");
	DecoderPaused(1);
}

void cPvr350Device::Mute(void)
{
	log(pvrDEBUG1, "cPvr350Device::Mute()");
	cDevice::Mute();
	///< A derived class must call the base class function to make sure
	///< all registered cAudio objects are notified.
}

void cPvr350Device::SetAudioChannelDevice(int AudioChannel)
{
	log(pvrDEBUG1, "cPvr350Device::SetAudioChannelDevice (%s)",
		AudioChannel == 0?"stereo":
		AudioChannel == 1?"mono left":"mono right");
	// 0=stereo, 1=left, 2=right, -1=no information available.

	if (ioctl(fd_out, AUDIO_CHANNEL_SELECT, AudioChannel) < 0) {
		log(pvrERROR, "pvr350: SetAudioChannelDevice (audio_stereo_mode) error=%d:%s", errno, strerror(errno));
	}
	if (ioctl(fd_out, AUDIO_BILINGUAL_CHANNEL_SELECT, AudioChannel) < 0) {
		log(pvrERROR, "pvr350: SetAudioChannelDevice (audio_bilingual_mode) error=%d:%s", errno, strerror(errno));
	}
	audiomode = AudioChannel;
}

int cPvr350Device::GetAudioChannelDevice(void)
{
	//ivtv driver does not support AUDIO_GET_STATUS ioctl, so we return the mode which was selected at last
	log(pvrDEBUG1, "cPvr350Device::GetAudioChannelDevice(): currently selected AudioChannel is %s",
		audiomode == 0?"stereo":
		audiomode == 1?"mono left":"mono right");
	return audiomode;
}

int cPvr350Device::PlayAudio(const uchar *Data, int Length, uchar Id)
{
	int PayloadOffset;
	int PES_Packet_Length;
	int len;
	static uchar Last_Id = 0x00;

	if (streamtype == undef) {
		streamtype = audio_only;
		Set_wss_mode(8); // 4:3 aspect ratio for radio
		log(pvrDEBUG1, "cPvr350Device::detected Audio-only stream");
	}

#ifdef DEBUG
        if (Pvr350Setup.LogLevel > 2) {
		PesDump(__FUNCTION__, Data, Length);
	}
#endif

	/* look if at the beginning there is the pes packet start indicator */
	if (Data[PESPACKETSTARTCODE1] != 0x00 ||
	    Data[PESPACKETSTARTCODE2] != 0x00 ||
	    Data[PESPACKETSTARTCODE3] != 0x01) {
		log(pvrDEBUG2, "cPvr350Device::PlayAudio(): no start indicator found...");
		return Length; // Return success to VDR
	}

	if (Id == 0x00) {
		/* needed for vdr >= 1.7.1 and vdr 1.6.0-2 with TSplay patch */
		Id = Data[PESSTREAMID];
	}

	/*
	 * PES Packet Length:	Specifies the number of bytes remaining
	 * 			in the packet after this field.
	 */
	len = Data[PESPACKETLEN1];
	PES_Packet_Length = (len << 8) + Data[PESPACKETLEN2];
	PES_Packet_Length += PESFIXHEADERLEN;
	if (Length != PES_Packet_Length) {
		log(pvrERROR, "cPvr350Device::PlayAudio(): PES Packet seems to be corrupted");
		log(pvrDEBUG1, "cPvr350Device::PlayAudio():Length=%d, len=%d, Id=0x%02x",
			Length, PES_Packet_Length, Id);
	}
#ifdef DEBUG
	MPEG2PES_t *mpeg2pes = (MPEG2PES_t *)Data;
	log(pvrDEBUG2, "pvr350: PES header(Audio)" \
		" prefix=0x%02x%02x%02x" \
		" stream_id=0x%02x" \
		" packet_length=%d (Length=%d)" \
		" marker_bits=%d",
			mpeg2pes->sync1, mpeg2pes->sync2, mpeg2pes->sync3,
			mpeg2pes->stream_id,
			((mpeg2pes->packet_length_H << 8) + mpeg2pes->packet_length_L),
			Length,
			mpeg2pes->marker_bits);
	log(pvrDEBUG2, "pvr350:        scrambling_control=%d" \
		" priority=%d" \
		" data_alignment_indicators=%d" \
		" copyright=%d" \
		" original=%d",
			mpeg2pes->scrambling_control,
			mpeg2pes->priority,
			mpeg2pes->data_alignment_indicators,
			mpeg2pes->copyright,
			mpeg2pes->original);
	log(pvrDEBUG2, "pvr350:        PTS_DTS_indicator=%d" \
		" ESCR_flag=%d" \
		" ES_rate_flag=%d" \
		" DSM_trick_mode_flag=%d" \
		" additional_copy_info_flag=%d",
			mpeg2pes->PTS_DTS_indicator,
			mpeg2pes->ESCR_flag,
			mpeg2pes->ES_rate_flag,
			mpeg2pes->DSM_trick_mode_flag,
			mpeg2pes->additional_copy_info_flag);
	log(pvrDEBUG2, "pvr350:        CRC_flag=%d" \
		" extension_flag=%d" \
		" header_length=%d",
			mpeg2pes->CRC_flag,
			mpeg2pes->extension_flag,
			mpeg2pes->header_length);
#endif

	/* #AS# ToDo:
	   Next line is for MPEG2 audio (PESFIXHEADERLEN + PESMPEG2EXTLEN) only.
	   More work is needed for MPEG1 audio.
	*/
	PayloadOffset = Data[PESHEADERLENGTH] + PESMPEG2HEADLEN;

#ifdef DEBUG
	if (Length <= PayloadOffset) {
		log(pvrDEBUG1, "cPvr350Device::PlayAudio():Length=%d <= PayloadOffset=%d",
			Length, PayloadOffset);
		return Length;
	}
#endif

	if (Id != Last_Id)  {
		Last_Id = Id;
		log(pvrDEBUG1, "cPvr350Device::PlayAudio(): Length=%d, Id=0x%02x",
			Length, Id);
	}

	switch (Id) {
	case 0x80 ... 0x87 :	// AC3 with vdr 1.4/1.6
	case 0xBD:		// since vdr 1.7.1 we only get the 'Privat Stream 1' Id for AC3.
		if (!m_AC3toMP2Init) {
			// Create a new decode/encode audio thread
			m_AC3toMP2 = new cAC3toMP2(m_AC3DecodeStatePtr, Pvr350Setup.AC3Gain);
			m_AC3toMP2Init = true;
		}
		ProcessAC3Audio((uint8_t *)Data, PayloadOffset, Length);
		len = Length;	// Return success to VDR
		break;
	case 0xA0 ... 0xA7 :		// LPCM
		if (!m_AC3toMP2Init) {
			// Create a new decode/encode audio thread
			m_AC3toMP2 = new cAC3toMP2(m_AC3DecodeStatePtr, Pvr350Setup.AC3Gain);
			m_AC3toMP2Init = true;
		}
		// ToDo: PCM to MP2 conversion
		len = Length;	// Return success to VDR
		break;
	case 0xC0 ... 0xDF :		// MP2
		if (m_AC3toMP2Init) {
			// Delete decode/encode audio thread
			delete m_AC3toMP2;
			m_AC3toMP2Init = false;
		}
		if (newStream) {
			newStream = false;
			//check if recoding for MP2 is selected
			if (Pvr350Setup.RecodeMP2) {
				log(pvrDEBUG1, "cPvr350Device::PlayAudio(): will recode MP2");
				DecEncMP2Audio = true;
				}
			else {
				log(pvrDEBUG1, "cPvr350Device::PlayAudio(): some streams may play not properly without recoding");
				DecEncMP2Audio = false;
			}
		}
		if (DecEncMP2Audio) {
			if (!m_MP2RecodeInit) {
				// Create a new MP2 decode/encode audio thread
				m_MP2toMP2 = new cMP2toMP2();
				m_MP2RecodeInit = true;
			}
			if (!ConvertMP2Audio((uint8_t *)Data, PayloadOffset, Length)) {
				// Converting the MP2 audio failed, process the 'original' data.
				ProcessMP2Audio((uint8_t *)Data, PayloadOffset, Length);
			}
		} else {
			if (m_MP2RecodeInit) {
				// Delete MP2 decode/encode audio thread
				delete m_MP2toMP2;
				m_MP2RecodeInit = false;
			}
			ProcessMP2Audio((uint8_t *)Data, PayloadOffset, Length);
		}
		len = Length;	// Return success to VDR
		break;
	default:
		static uchar LastUnknownId = 0;
		if (Id != LastUnknownId) {
			LastUnknownId = Id;
			log(pvrERROR, "pvr350: PlayAudio unknown audio format, Id=0x%02x", Id);
		}
		len = Length;	// Return success to VDR
		break;
	}
	return len;
}

int cPvr350Device::PlayVideo(const uchar *Data, int Length)
{
	int len;
#ifdef DEBUG
	MPEG2PES_t *mpeg2pes = (MPEG2PES_t *)Data;
	log(pvrDEBUG2, "pvr350: PES header(Video)" \
		" prefix=0x%02x%02x%02x" \
		" stream_id=0x%02x" \
		" packet_length=%d (Length=%d)" \
		" marker_bits=%d",
			mpeg2pes->sync1, mpeg2pes->sync2, mpeg2pes->sync3,
			mpeg2pes->stream_id,
			((mpeg2pes->packet_length_H << 8) + mpeg2pes->packet_length_L),
			Length,
			mpeg2pes->marker_bits);
	log(pvrDEBUG2, "pvr350:        scrambling_control=%d" \
		" priority=%d" \
		" data_alignment_indicators=%d" \
		" copyright=%d" \
		" original=%d",
			mpeg2pes->scrambling_control,
			mpeg2pes->priority,
			mpeg2pes->data_alignment_indicators,
			mpeg2pes->copyright,
			mpeg2pes->original);
	log(pvrDEBUG2, "pvr350:        PTS_DTS_indicator=%d" \
		" ESCR_flag=%d" \
		" ES_rate_flag=%d" \
		" DSM_trick_mode_flag=%d" \
		" additional_copy_info_flag=%d",
			mpeg2pes->PTS_DTS_indicator,
			mpeg2pes->ESCR_flag,
			mpeg2pes->ES_rate_flag,
			mpeg2pes->DSM_trick_mode_flag,
			mpeg2pes->additional_copy_info_flag);
	log(pvrDEBUG2, "pvr350:        CRC_flag=%d" \
		" extension_flag=%d" \
		" header_length=%d",
			mpeg2pes->CRC_flag,
			mpeg2pes->extension_flag,
			mpeg2pes->header_length);
#endif

	/* if the first packet was an audio packet, the stream was falsely detected as audio-only and
	   a black video frame was sent. We need to Clear the decoder buffer now to avoid
	   artefacts when the next real video frame appears  */
	if (streamtype == audio_only && Pvr350Setup.BlackVideoForAudioOnly == 1) {
		log(pvrDEBUG1, "cPvr350Device::PlayVideo: video packet after blackframe (audio-only) detected- reset the decoder now");
		DecoderStop(1);
		DecoderPlay(0);
	}
	streamtype = video;

	/* look if at the beginning there is the pes packet start indicator */
	if (Data[PESPACKETSTARTCODE1] != 0x00 ||
	    Data[PESPACKETSTARTCODE2] != 0x00 ||
	    Data[PESPACKETSTARTCODE3] != 0x01) {
		log(pvrDEBUG2, "cPvr350Device::PlayVideo(): no start indicator found...");
		return Length; // Return success to VDR
	}

	SetVidInfo(Data, Length);
	len = WriteAllOrNothing(fd_out, Data, Length, 1000, 10);

	if (len <= 0 && errno != EAGAIN) { //don't log if "Resource temporarily unavailable"
		log(pvrERROR, "pvr350: PlayVideo written=%d error=%d:%s",
			len, errno, strerror(errno));
	}
	return len;
}

void cPvr350Device::SetVolumeDevice(int Volume)
{
}

void cPvr350Device::StillPicture(const uchar *Data, int Length)
{
#define MIN_IFRAME 400000
	log(pvrDEBUG1, "cPvr350Device::StillPicture(Length=%d)", Length);
	if (!Data || Length < TS_SIZE) {
		return;
	}

	if (Data[PESPACKETSTARTCODE1] == 0x47) {
		// TS data
		cDevice::StillPicture(Data, Length);
		return;
	}

	/* Check if Data-Buffer contains a MPEG-1 or MPEG-2 video stream */
	if (Data[PESPACKETSTARTCODE1] == 0x00 &&
	    Data[PESPACKETSTARTCODE2] == 0x00 &&
	    Data[PESPACKETSTARTCODE3] == 0x01 &&
	    (Data[PESSTREAMID] & 0xF0) == 0xE0) {
		// PES data
		uchar *buf = MALLOC(uchar, Length);
		if (!buf) {
			return;
		}
		int i = 0;
		int blen = 0;
		while (i < Length - PESHEADERLENGTH) {
			if (Data[i + PESPACKETSTARTCODE1] == 0x00 &&
			    Data[i + PESPACKETSTARTCODE2] == 0x00 &&
			    Data[i + PESPACKETSTARTCODE3] == 0x01) {
				// Calculate PES packet length
				int len = Data[i + PESPACKETLEN1] * 256 + Data[i + PESPACKETLEN2];
				if ((Data[i + PESSTREAMID] & 0xF0) == 0xE0) {
					// MPEG-1 or MPEG-2 video packet found, skip PES header
					int offs = i + 6;
					if ((Data[i + PESEXTENSION1] & 0xC0) == 0x80) {
						// MPEG-2 PES header found, skip header extension
						if (Data[i + PESHEADERLENGTH] >= Length) {
							break;
						}
						offs += 3;
						offs += Data[i + PESHEADERLENGTH];
						len -= 3;
						len -= Data[i + PESHEADERLENGTH];
						if (len < 0 || offs + len > Length) {
							break;
						}
					} else {
						// MPEG-1 PES header found, skip header extension
						while (offs < Length && len > 0 && Data[offs] == 0xFF) {
							offs++;
							len--;
						}
						// Check for optional STD buffer size
						if (offs <= Length - 2 && len >= 2 &&
						    (Data[offs] & 0xC0) == 0x40) {
							offs += 2;
							len -= 2;
						}
						// Check for optional PTS/DTS
						if (offs <= Length - 5 && len >= 5 &&
						    (Data[offs] & 0xF0) == 0x20) {
							offs += 5;
							len -= 5;
						} else if (offs <= Length - 10 && len >= 10 &&
						           (Data[offs] & 0xF0) == 0x30) {
							offs += 10;
							len -= 10;
						} else if (offs < Length && len > 0) {
							offs++;
							len--;
						}
					}
					if (blen + len > Length) {
						// invalid PES length field
						break;
					}
					memcpy(&buf[blen], &Data[offs], len);
					i = offs + len;
					blen += len;
				} else if (Data[i + PESSTREAMID] >= 0xBD &&
					   Data[i + PESSTREAMID] <= 0xDF) {
					// other PES packets
					i += len + 6;
				} else {
					i++;
				}
			} else {
				i++;
			}
		} // End while()

		for (int i = MIN_IFRAME / Length + 1; i > 0; i--) {
			WriteAllOrNothing(fd_out, buf, blen);
			usleep(1); // allows the buffer to be displayed in case the progress display is active
		}
		free(buf);
	} else {
		// non-PES data
		for (int i = MIN_IFRAME / Length + 1; i > 0; i--) {
			WriteAllOrNothing(fd_out, Data, Length, 1000, 10);
		usleep(1); // allows the buffer to be displayed in case the progress display is active
		}
	}
}

bool cPvr350Device::Poll(cPoller &Poller, int TimeoutMs)
{
	log(pvrDEBUG2, "cPvr350Device::Poll(TimeoutMs=%d)", TimeoutMs);
	Poller.Add(fd_out,true);
	return Poller.Poll(TimeoutMs);
}

cSpuDecoder *cPvr350Device::GetSpuDecoder(void)
{
	log(pvrDEBUG1, "cPvr350Device::GetSpuDecoder()");
	if (!spuDecoder && IsPrimaryDevice()) {
		spuDecoder = new cDvbSpuDecoder();
	}
	return spuDecoder;
}

void cPvr350Device::SetVidInfo(const uchar *mbuf, int count)
{
	const uchar *headr;
	int found = 0;
	int c = 0;

	struct v4l2_crop crop;
	memset (&crop, 0, sizeof (crop));
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	while (found < 4 && c+4 < count){
		const uchar *b;

		b = mbuf + c;
		if (b[0] == 0x00 && b[1] == 0x00 &&
		    b[2] == 0x01 && b[3] == 0xb3) {
			found = 4;
		} else {
			c++;
		}
	}

	if (! found) return;
	c += 4;
	if (c+12 >= count) return;
	headr = mbuf+c; 

	horizontal_size = ((headr[1] &0xF0) >> 4) | (headr[0] << 4);
	vertical_size   = ((headr[1] &0x0F) << 8) | (headr[2]);
	aspectratio = (int)((headr[3]&0xF0) >> 4) ;

	log(pvrDEBUG2, "cPvr350Device::SetVidInfo() aspectratio=%s",
		aspectratio == 1?"1:1":
		aspectratio == 2?"4:3":
		aspectratio == 3?"16:9":
		aspectratio == 4?"2:21:1":"unknown");

	if ((horizontal_size > 720) || (vertical_size > 576))
		return;

	switch (aspectratio) {
	case 1:  /* 1:1 aspect ratio */
		break;
	case 2:  /* 4:3  aspect ratio */
		Set_wss_mode(8);
		if (Format16_9 == false) {	/* 4:3 TV */
			crop.c.width = width;
			crop.c.height= height;
			crop.c.left  = lx;
			crop.c.top   = ly;
		} else {			/* 16:9 TV */
			crop.c.width = width;
			crop.c.height= height;
			crop.c.left  = lx;
			crop.c.top   = ly;
		}
		if (crop.c.width  != (int)current_horiz ||
		    crop.c.height != (int)current_vertical ) {
			current_horiz    = crop.c.width;
			current_vertical = crop.c.height;
			log(pvrDEBUG1, "pvr350: Trying size set (4:3): %dx%d %dx%d",
				crop.c.width, crop.c.height,
				crop.c.left, crop.c.top);
			if (IOCTL(fd_out, VIDIOC_S_CROP,&crop) < 0) {
				log(pvrERROR, "pvr350: SetVidInfo (4:3) error=%d:%s",
					errno, strerror(errno));
			}
		}
		break;
	case 3:  /* 16:9 aspect ratio */
		if (Format16_9) {  /* 16:9 TV */
			crop.c.width = width;
			crop.c.height= height;
			crop.c.left  = lx;
			crop.c.top   = ly;
			Set_wss_mode(7);
		} else {  /* 4:3 TV */
			Set_wss_mode(8);
			if (horizontal_size <= vertical_size) {
				crop.c.width  = width;
				crop.c.height = (width/ 16) * 9;
				crop.c.left   = lx + (width - crop.c.width) / 2;
				crop.c.top    = ly + (height- crop.c.height) / 2;
			} else {
				crop.c.width = width < horizontal_size ? width : horizontal_size;
				//crop.c.width.height = ( crop.c.width.width ) / 16 * 9;
				crop.c.height = ( vertical_size ) / 4 * 3;
				crop.c.left = lx + (width - crop.c.width) / 2;
				crop.c.top = ly + (height - crop.c.height) / 2;
			}
		}
		if (crop.c.width != (int)current_horiz ||
		    crop.c.height != (int)current_vertical || sizechanged ) {
			sizechanged = 0;
			current_horiz = crop.c.width;
			current_vertical = crop.c.height;
			log(pvrDEBUG1, "pvr350: Trying size set (16:9): %dx%d %dx%d",
 				crop.c.width,crop.c.height,
				crop.c.left, crop.c.top);
			if (IOCTL(fd_out, VIDIOC_S_CROP,&crop) < 0) {
				log(pvrERROR, "pvr350: SetVidInfo (16:9) error=%d:%s",
					errno, strerror(errno));
			}
		}
		break;
	case 4: /* 2:21:1 aspect ratio */
		break;
	case 5 ... 15:
		break;
	default:
		break;
	}
}

int64_t cPvr350Device::GetSTC(void)
{
	int64_t pts = 0;

	if (ioctl(fd_out, VIDEO_GET_PTS, &pts) < 0) {
		log(pvrERROR, "pvr350: GetSTC error=%d:%s", errno, strerror(errno));
		return -1;
	}
	log(pvrDEBUG2, "cPvr350Device::GetSTC(): PTS=%lld", pts);
	return pts;
}

void cPvr350Device::SetVideoSize(int x, int y, int w, int d)
{
	log(pvrDEBUG1, "cPvr350Device::SetVideoSize(x=%d,y=%d,w=%d,d=%d)", x, y, w, d);
	width = w;
	height = d;
	lx = x;
	ly = y;
	sizechanged = 1;
}

void cPvr350Device::Set_wss_mode(int wss_data)
{
	if (Pvr350Setup.UseWssBits == 0 || wss_data == current_wss_data || wss_data < 7 || wss_data > 8) {
		return;
	}
	log(pvrDEBUG2, "cPvr350Device::Set_wss_mode(%d), was (%d) before", wss_data, current_wss_data);
	char vbidevName[64];
	sprintf(vbidevName, "/dev/vbi%d", Pvr350Setup.DeviceNumber);
	fh = open(vbidevName, O_WRONLY);
	if (fh < 0) {
		log(pvrERROR, "pvr350: Can't open vbi device for writing VBI data to TV-out");
		return;
	}
	struct v4l2_format fmt;
	struct v4l2_sliced_vbi_data data;
	fmt.fmt.sliced.service_set = V4L2_SLICED_WSS_625;
        fmt.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
	if (ioctl(fh, VIDIOC_S_FMT, &fmt) != 0) {
		log(pvrERROR, "pvr350: Set VBI mode failed");
	}
	if (ioctl(fh, VIDIOC_G_FMT, &fmt) != 0) {
		log(pvrERROR, "pvr350: VIDIOC_G_FMT failed");
	}
	data.id = V4L2_SLICED_WSS_625;
	data.line = 23;
	data.field = 0;
	data.data[0] = wss_data;
	data.data[1] = 0;

	if (wss_data == 7) {
		log(pvrDEBUG1, "pvr350: Set wss mode 16:9 (anarmorph)");
	}
	if (wss_data == 8) {
		log(pvrDEBUG1, "pvr350: Set wss mode 4:3");
	}
	if (write(fh, &data, sizeof data) < 0) {
		log(pvrERROR, "pvr350: write VBI data (wss) failed");
	}
	close(fh);
	fh = -1;
	current_wss_data = wss_data;
}

void SetVideoSize(int x, int y, int w, int d)
{
	Instance->SetVideoSize(x, y, w, d);
}

void ResetVideoSize()
{
	/* FIXME: Think NTSC */
	Instance->SetVideoSize(0, 0, 720, 576);
}

void cPvr350Device::ProcessAC3Audio(uint8_t *PESPacket, int PayloadOffset, int Length)
{
	int len;
	int bytes_left;
	int pcm_bytes;
	int mp2_len;
	int mp2_bytes_encoded;
	uint8_t *PESPacketData;
	int PESPacketData_len;
	uint8_t *MP2PESDataPtr;
	int PES_Packet_Length;
	uint8_t *DataPtr;
	int16_t *PCMBufferPtr;
	int mp2buffer_size;

	if (!m_AC3toMP2Init) {
		return;		// AC3 audio thread not running
	}

	log(pvrDEBUG2, "cPvr350Device::ProcessAC3Audio()");

	PESPacketData = (uint8_t *)&PESPacket[PayloadOffset];
	PESPacketData_len = Length - PayloadOffset;
	bytes_left = PESPacketData_len;
	DataPtr = PESPacketData;
	PCMBufferPtr = (int16_t *)m_PCMBuffer;
	mp2_bytes_encoded = 0;
	mp2buffer_size = MP2_PES_DATA_SIZE;

	// copy 'original' PES header to MP2 PES buffer
	memcpy(m_MP2PESBuffer, PESPacket, PayloadOffset);
	// Initialize output pointer for encode call
	MP2PESDataPtr = (uint8_t *)&m_MP2PESBuffer[PayloadOffset];

	while (bytes_left > 0) {
		len = m_AC3toMP2->A52DecodeFrame(m_AC3DecodeStatePtr,
						PCMBufferPtr, &pcm_bytes,
						DataPtr, bytes_left);
		if (len < 0) {
			log(pvrERROR, "pvr350: Error return from A52DecodeFrame()");
			return;
		}
		DataPtr += len;		// Adjust input pointer for next decode call
		bytes_left -= len;
		if (pcm_bytes) {
			mp2_len = m_AC3toMP2->MP2EncodeFrame(PCMBufferPtr,
								pcm_bytes,
								MP2PESDataPtr,
								mp2buffer_size);
			if (mp2_len < 0) {
				log(pvrERROR, "pvr350: ProcessAC3Audio() - MP2PESBuffer too small");
				break;
			}
			mp2_bytes_encoded += mp2_len;
			MP2PESDataPtr += mp2_len;	// Adjust outbuf pointer for next encode call
			mp2buffer_size -= mp2_len;	// Remaining bytes in outbuf buffer
		}
	}

#if 0
	/*
	 * Flush any remaining audio. (don't send any new audio data)
	 * There should only ever be a max of 1 frame on a flush. There may be zero frames
	 * if the audio data was an exact multiple of 1152 (TWOLAME_SAMPLES_PER_FRAME)
	*/
	mp2_len = m_MP2toMP2->MP2EncodeFrame(NULL,
						0,
						MP2PESDataPtr,
						mp2buffer_size);
	if (mp2_len > 0) {
		log(pvrDEBUG1, "ProcessAC3Audio() - flush remaining data [%d]", mp2_len);
		mp2_bytes_encoded += mp2_len;
	}
#endif

	if (mp2_bytes_encoded > 0) {
		// Change PES Stream ID
		m_MP2PESBuffer[PESSTREAMID] = MPEG_AUDIO_STREAM_ID;

		// Adjust PES Packet Length
		PES_Packet_Length = PayloadOffset - PESFIXHEADERLEN + mp2_bytes_encoded;
		m_MP2PESBuffer[PESPACKETLEN1] = (uint8_t)(PES_Packet_Length >> 8);
		m_MP2PESBuffer[PESPACKETLEN2] = (uint8_t)(PES_Packet_Length & 0xff);

		// Calculate length of PES Packet Frame
		len = PES_Packet_Length + PESFIXHEADERLEN;

		len = WriteAllOrNothing(fd_out, m_MP2PESBuffer, len, 1000, 10);
		if (len <= 0 && errno != EAGAIN) { //don't log if "Resource temporarily unavailable"
			log(pvrERROR, "pvr350: ProcessAC3Audio written=%d error=%d:%s",
				len, errno, strerror(errno));
		}
	}
	return;
}

bool cPvr350Device::ConvertMP2Audio(uint8_t *Data, int PayloadOffset, int Length)
{
	int len;
	int bytes_left;
	int pcm_bytes;
	int mp2_len;
	int mp2_bytes_encoded;
	uint8_t *PESPacketData;
	int PESPacketData_len;
	uint8_t *MP2PESDataPtr;
	int PES_Packet_Length;
	uint8_t *DataPtr;
	int16_t *PCMBufferPtr;
	int mp2buffer_size;

	if (!m_MP2RecodeInit) {
		return false;	// MP2 audio thread not running
	}

	log(pvrDEBUG2, "cPvr350Device::ConvertMP2Audio()");

	PESPacketData = (uint8_t *)&Data[PayloadOffset];
	PESPacketData_len = Length - PayloadOffset;

	DataPtr = PESPacketData;
	PCMBufferPtr = (int16_t *)m_PCMBuffer;
	mp2_bytes_encoded = 0;
	mp2buffer_size = MP2_PES_DATA_SIZE;

	// copy 'original' PES header to MP2 PES buffer
	memcpy(m_MP2PESBuffer, Data, PayloadOffset);
	// Initialize pointer for encode call
	MP2PESDataPtr = (uint8_t *)&m_MP2PESBuffer[PayloadOffset];

	bytes_left = m_MP2toMP2->MPG123DecodeFrame(DataPtr, PESPacketData_len,
						PCMBufferPtr, PCM_BUFFER_SIZE,
						&pcm_bytes);
	if (bytes_left < 0) {
		log(pvrERROR, "pvr350: ConvertMP2Audio() - error decode audio");
		return false;
	}
	if (pcm_bytes <= 0) {
		return true;
	}
	do {
		mp2_len = m_MP2toMP2->MP2EncodeFrame(PCMBufferPtr,
							pcm_bytes,
							MP2PESDataPtr,
							mp2buffer_size);
		if (mp2_len < 0) {
			log(pvrERROR, "pvr350: ConvertMP2Audio() - MP2PESBuffer too small");
			return false;
		}
		mp2buffer_size -= mp2_len;
		mp2_bytes_encoded += mp2_len;
		// Adjust pointer for next encode call
		MP2PESDataPtr += mp2_len;

		// Get all decoded audio that is available now before feeding more input.
		bytes_left = m_MP2toMP2->MPG123DecodeFrame(NULL, 0,
							PCMBufferPtr,
							PCM_BUFFER_SIZE,
							&pcm_bytes);
		if (bytes_left < 0) {
			log(pvrERROR, "pvr350: ConvertMP2Audio() - error get all decoded audio");
			return false;
		}
	} while (bytes_left);

	/*
	 * Flush any remaining audio. (don't send any new audio data)
	 * There should only ever be a max of 1 frame on a flush. There may be zero frames
	 * if the audio data was an exact multiple of 1152 (TWOLAME_SAMPLES_PER_FRAME)
	*/
	mp2_len = m_MP2toMP2->MP2EncodeFrame(NULL,
						0,
						MP2PESDataPtr,
						mp2buffer_size);
	if (mp2_len > 0) {
		log(pvrDEBUG1, "ConvertMP2Audio() - flush remaining data [%d]", mp2_len);
		mp2_bytes_encoded += mp2_len;
	}

	if (mp2_bytes_encoded > 0) {
		// Adjust PES Packet Length
		PES_Packet_Length = PayloadOffset - PESFIXHEADERLEN + mp2_bytes_encoded;
		m_MP2PESBuffer[PESPACKETLEN1] = (uint8_t)(PES_Packet_Length >> 8);
		m_MP2PESBuffer[PESPACKETLEN2] = (uint8_t)(PES_Packet_Length & 0xff);

		// Calculate length of PES Packet Frame
		len = PES_Packet_Length + PESFIXHEADERLEN;
		ProcessMP2Audio((uint8_t *)m_MP2PESBuffer, PayloadOffset, len);
	}

	return true;
}

void cPvr350Device::ProcessMP2Audio(uint8_t *Data, int PayloadOffset, int Length)
{
	int len;

	log(pvrDEBUG2, "cPvr350Device::ProcessMP2Audio()");

	if (streamtype == audio_only || m_PlayMode == pmAudioOnlyBlack || m_PlayMode == pmAudioOnly) {
		if (Pvr350Setup.BlackVideoForAudioOnly == 1 || m_PlayMode == pmAudioOnlyBlack) {
			//PesDump(__FUNCTION__, Data, Length);
			len = WriteAllOrNothing(fd_out, Data, Length, 1000, 10);
			if (len <= 0 && errno !=11) { //don't log if "Resource temporarily unavailable"
				log(pvrERROR, "pvr350: ProcessMP2Audio(MP2 Audio only) written=%d error=%d:%s",
				    len, errno, strerror(errno));
			} else {
				framecount += 24; //assume frame length of 24ms for MP2 audio
			}
			if (framecount >= 40) {
				int header_size = Data[PESHEADERLENGTH] + PESMPEG2HEADLEN;
				uchar *dst = blackvideo + PESMPEG2HEADLEN + 255 - header_size;
				memcpy(dst, Data, header_size);
				dst[3] = 0xe0;
				dst[4] = 0x00;
				dst[5] = 0x00;
				int frame_length = sizeof(blackvideo) - PESMPEG2HEADLEN - 255 + header_size;
				//PesDump(__FUNCTION__, dst, frame_length);
				len = WriteAllOrNothing(fd_out, dst, frame_length, 1000, 10);
				if (len <= 0 && errno != EAGAIN) { //don't log if "Resource temporarily unavailable"
					log(pvrERROR, "pvr350: ProcessMP2Audio(Blackframe) written=%d error=%d:%s",
						len, errno, strerror(errno));
				} else {
					framecount -= 40; //video frame length = 40 ms
				}
			}
		} else {
			len = WriteAllOrNothing(fd_out, Data, Length, 1000, 10);
			if (len <= 0 && errno != EAGAIN) { //don't log if "Resource temporarily unavailable"
				log(pvrERROR, "pvr350: ProcessMP2Audio(MP2 Audio only) written=%d error=%d:%s",
					len, errno, strerror(errno));
			}
		}
	} else { //Audio packet for a stream that has also video
		len = WriteAllOrNothing(fd_out, Data, Length, 1000, 10);
		if (len <= 0 && errno != EAGAIN) { //don't log if "Resource temporarily unavailable"
			log(pvrERROR, "pvr350: ProcessMP2Audio(MP2) written=%d error=%d:%s",
				len, errno, strerror(errno));
		}
	}
}

