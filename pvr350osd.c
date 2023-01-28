/*
 * pvr350osd.c: 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "pvr350osd.h"
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <linux/ivtvfb.h>
#include <linux/version.h>
#include <vdr/tools.h>
#include "pvr350tools.h"
#include "pvr350device.h"

#if VDRVERSNUM < 10509
cPvr350Osd::cPvr350Osd(int Left, int Top, int fbfd, unsigned char *osdbuf)
    :cOsd(Left, Top)
#else
cPvr350Osd::cPvr350Osd(int Left, int Top, uint Level, int fbfd, unsigned char *osdbuf)
    :cOsd(Left, Top, Level)
#endif
{
    fd = fbfd;
    osd = osdbuf; 
    shown = false;
}

void cPvr350Osd::Copy_OSD_buffer_to_card(void)
{
  if (GetIvtvVersion(0,0) >= KERNEL_VERSION(1, 4, 0)) {
      lseek (fd, 0, SEEK_SET);
      if (write (fd, osd, 720 * 576 * 4) < 0) {
          log(pvrERROR, "pvr350: OSD write failed error=%d:%s", errno, strerror(errno));
      }
  } else {
      struct ivtvfb_dma_frame prep;
      memset(&prep, 0, sizeof(prep));
      prep.source = osd;
      prep.dest_offset = 0;
      prep.count = 720 * 576 * 4;
      if (ioctl(fd, IVTVFB_IOC_DMA_FRAME, &prep) < 0) {
          log(pvrERROR, "pvr350: OSD DMA failed error=%d:%s", errno, strerror(errno));
      }
  }
}

cPvr350Osd::~cPvr350Osd()
{
#if VDRVERSNUM < 10509
  if (shown) {
    cBitmap *Bitmap;
    for ( int i = 0; ( Bitmap = GetBitmap(i)) != NULL; i++ ) {
        Hide(Bitmap);
        }
    Copy_OSD_buffer_to_card();
    }
#else
    SetActive(false);
#endif
#ifdef SET_VIDEO_WINDOW
    if ( vidWin.bpp != 0 ) {
	    ResetVideoSize();
    }
#endif
}

#if VDRVERSNUM >= 10509
void cPvr350Osd::SetActive(bool On)
{
  if (On != Active()) {
      cBitmap *Bitmap;
      // Clears the OSD screen image when it becomes active
      // removes it from screen when it becomes inactive
      cOsd::SetActive(On);
      if (On) {
          for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
              Hide(Bitmap);
              }
          Copy_OSD_buffer_to_card();

          if (GetBitmap(0)) // only flush here if there are already bitmaps
             Flush();

      }
      else if (shown) {
          for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
              Hide(Bitmap);
              }
          Copy_OSD_buffer_to_card();
          shown = false;
      }
  }
}
#endif

eOsdError cPvr350Osd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
    eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
    if (Result == oeOk) {
         for (int i = 0; i < NumAreas; i++) {
           if (Areas[i].Width() < 1 || Areas[i].Height() < 1 || Areas[i].Width() > 720 || Areas[i].Height() > 576)
              return oeWrongAreaSize;
           if (Areas[i].bpp > 24)
              return oeBppNotSupported;
           }
       }
  return Result;
}

eOsdError cPvr350Osd::SetAreas(const tArea *Areas, int NumAreas)
{
  if (shown) {
    cBitmap *Bitmap;
    for ( int i = 0; ( Bitmap = GetBitmap(i)) != NULL; i++ ) {
        Hide(Bitmap);
        }
    Copy_OSD_buffer_to_card();
    shown = false;
    }
  return cOsd::SetAreas(Areas, NumAreas);
}

void cPvr350Osd::Flush(void)
{
#if VDRVERSNUM >= 10509
    if (!Active())
     return;
#endif
    cBitmap *Bitmap;
    for ( int i = 0; ( Bitmap = GetBitmap(i)) != NULL; i++ ) {
        int  x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        if ( Bitmap->Dirty(x1,y2,x2,y2) ) {
            for ( int yp = y1; yp <= y2; yp++ ) {
                for ( int xp = x1; xp <= x2; xp++  ) {
                    tColor c = Bitmap->Color(*Bitmap->Data(xp,yp));

                    osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) ] = ( c >> 0 ) & 0xFF;
                    osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) + 1] = ( c >> 8 ) & 0xFF;
                    osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) + 2] = ( c >> 16 ) & 0xFF;
                    osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) + 3] = ( c >> 24 ) & 0xFF;
                }
            }
        }
    }
    Copy_OSD_buffer_to_card();
#ifdef SET_VIDEO_WINDOW
    if (vidWin.bpp != 0 ) {
	    SetVideoSize(vidWin.x1,vidWin.y1,vidWin.x2 - vidWin.x1,vidWin.y2 - vidWin.y1);
    } else {
	    ResetVideoSize();
    }
#endif
    shown = true;
}

void cPvr350Osd::Hide(cBitmap *Bitmap)
{
    for ( int yp = 0; yp < Bitmap->Height(); yp++ ) {
        for ( int xp = 0; xp < Bitmap->Width(); xp++ ) {
            tColor c = 0;

            osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) ] = ( c >> 24 ) & 0xFF;
            osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) + 1] = ( c >> 16 ) & 0xFF;    
            osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) + 2] = ( c >> 8 ) & 0xFF;
            osd[((yp + Top() + Bitmap->Y0()) * 720 *4) + ((xp +  Bitmap->X0() + Left()) * 4) + 3] = ( c >> 0 ) & 0xFF;
        }
    }
}


// --- cPvr350OsdProvider -------------------------------------------------------

cPvr350OsdProvider::cPvr350OsdProvider(int fd, unsigned char *buf)
{
    osdBuf = buf;
    osdfd  = fd;
}

#if VDRVERSNUM < 10509
cOsd *cPvr350OsdProvider::CreateOsd(int Left, int Top)
{
  return new cPvr350Osd(Left, Top, osdfd, osdBuf);
}
#else
cOsd *cPvr350OsdProvider::CreateOsd(int Left, int Top, uint Level)
{
  return new cPvr350Osd(Left, Top, Level, osdfd, osdBuf);
}
#endif
