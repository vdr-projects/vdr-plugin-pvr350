/*
 * pvr350tools.c: 
 *
 * generic functions
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/version.h>

#include "pvr350tools.h"
#include "pvr350device.h"
#include "pvr350setup.h"

struct v4l2_capability vcap;

int GetIvtvVersion(int init, int fd)
{
	if (init) {
	/*only execute if init is not zero, see cPvr350Device::Open()*/
		memset(&vcap, 0, sizeof(vcap));
		if (fd <= 0) {
			log(pvrERROR, "pvr350: GetIvtvVersion: no valid file handle, file is not open");
			return -1; /*no valid file handle, file is not open*/
		}
		if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) != 0) {
			log(pvrERROR, "pvr350: GetIvtvVersion: query V4L2 capabilities failed.");
			return -1;
		} else {
			log(pvrINFO, "pvr350: IVTV version=0x%06x found", vcap.version);}
	} //end init  
	return vcap.version;
}

void log(int level, const char * fmt, ...)
{
	char tmpstr[BUFSIZ];
	char timestr[16];
	va_list ap;
	time_t now;
	struct tm local;

	if (Pvr350Setup.LogLevel >= level) {
		va_start(ap, fmt);
		time(&now);
		localtime_r(&now, &local);
		vsnprintf(tmpstr, sizeof(tmpstr), fmt, ap);
		strftime(timestr, sizeof(timestr), "%H:%M:%S", &local);
		printf("pvr350: %s %s\n", timestr, tmpstr);
		switch (level) {
		case pvrERROR:
			esyslog("%s",tmpstr);
			break;
		case pvrINFO:
			isyslog("%s",tmpstr);
			break;
		case pvrDEBUG1:
		case pvrDEBUG2:
		default:
			dsyslog("%s",tmpstr);
			break;
		}
		va_end(ap);
	}
}

