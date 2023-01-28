#include "pvr350setup.h"


cPvr350Setup::cPvr350Setup(void) {
  LogLevel                       = 2;        // errors and (some) messages
  UseWssBits                     = 1;        // use wss bits
  WSS_169_for_pmExtern           = 0;        // send no 16:9 wss bit for Playmode pmExtern (e.g. mplayer)
  AC3Gain                        = 1;        // increase the gain of the ac3 audio prior to encoding
  DeviceNumber                   = -1;       // auto detection
  BlackVideoForAudioOnly         = 1;        // send blank (black) video for audio-only streams.
  RecodeMP2                      = 1;        // recode MP2 audio
}

cPvr350Setup Pvr350Setup;
