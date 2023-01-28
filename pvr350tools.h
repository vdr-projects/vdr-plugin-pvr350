/*
 * pvr350tools.h: 
 *
 * generic functions
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __PVR350_TOOLS_H
#define __PVR350_TOOLS_H

enum eLogLevel { pvrUNUSED, pvrERROR, pvrINFO, pvrDEBUG1, pvrDEBUG2 };

extern void    SetVideoSize(int x, int y, int w, int d);
extern void    ResetVideoSize();
int GetIvtvVersion(int init, int fd);
void log(int level, const char * fmt, ...);
#endif

