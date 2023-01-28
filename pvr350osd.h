/*
 * pvr350osd.h:
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __PVR350_OSD_H
#define __PVR350_OSD_H

#include <vdr/osdbase.h>
#include <linux/fb.h>

class cPvr350Osd : public cOsd {
private:
    int  fd;
    unsigned char *osd;
    bool shown;
#if VDRVERSNUM >= 10509
protected:
virtual void SetActive(bool On);
#endif
public:
#if VDRVERSNUM < 10509
    cPvr350Osd(int Left, int Top, int fbfd, unsigned char *osdbuf);
#else
    cPvr350Osd(int Left, int Top, uint Level, int fbfd, unsigned char *osdbuf);
#endif
    ~cPvr350Osd();
    eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    eOsdError SetAreas(const tArea *Areas, int NumAreas);
    void Flush(void);
    void Copy_OSD_buffer_to_card(void);

private:
    void Hide(cBitmap *Bitmap);
};

class cPvr350OsdProvider : public cOsdProvider {
private:
    unsigned char *osdBuf;
    int            osdfd; 
public:
    cPvr350OsdProvider(int fd, unsigned char *buf);
#if VDRVERSNUM < 10509
    virtual cOsd *CreateOsd(int Left, int Top);
#else
    virtual cOsd *CreateOsd(int Left, int Top, uint Level);
#endif
};


#endif
