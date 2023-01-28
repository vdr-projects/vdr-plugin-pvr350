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

#include "pvr350tools.h"
#include "pvr350device.h"
#include "pvr350setup.h"

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

