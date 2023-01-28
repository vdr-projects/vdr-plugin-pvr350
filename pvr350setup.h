#ifndef _PVR350_SETUP_H_
#define _PVR350_SETUP_H_

#include <sys/time.h>
#include <linux/types.h>


class cPvr350Setup {
public:
  int LogLevel;
  int UseWssBits;
  int WSS_169_for_pmExtern;
  int DeviceNumber;
  int AC3Gain;
  int BlackVideoForAudioOnly;
  int RecodeMP2;
public:
  cPvr350Setup(void);
};

extern cPvr350Setup Pvr350Setup;

#endif
