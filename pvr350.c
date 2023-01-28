/*
 * pvr350.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: pvr350.c,v 1.9 2007/01/02 20:00:00 dom Exp $
 */

#include <vdr/plugin.h>
#include "pvr350device.h"
#include "pvr350menu.h"
#include "pvr350.h"

static const char *VERSION        = "1.7.5";
static const char *DESCRIPTION    = trNOOP("PVR350 as output device");


cPluginPvr350::cPluginPvr350(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  pvr350device = NULL;
}

cPluginPvr350::~cPluginPvr350()
{
  // Clean up after yourself!
  if (pvr350device) {
    // FIXME: 'delete pvr350device;' gives segfaults
    pvr350device = NULL;
  }
}

const char * cPluginPvr350::Version(void) {
  return VERSION;
}

const char * cPluginPvr350::Description(void) {
  return tr(DESCRIPTION);
}

const char *cPluginPvr350::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginPvr350::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginPvr350::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  pvr350device = new cPvr350Device();
  return true;
}

bool cPluginPvr350::Start(void)
{
  // Start any background activities the plugin shall perform.
  return true;
}

void cPluginPvr350::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cMenuSetupPage * cPluginPvr350::SetupMenu(void)
{
  return new cPvr350MenuSetup();
}

bool cPluginPvr350::SetupParse(const char *Name, const char *Value)
{
  if      (!strcasecmp(Name, "LogLevel"))                Pvr350Setup.LogLevel               = atoi(Value);
  else if (!strcasecmp(Name, "UseWssBits"))              Pvr350Setup.UseWssBits             = atoi(Value);
  else if (!strcasecmp(Name, "WSS_16:9_for_pmExtern"))   Pvr350Setup.WSS_169_for_pmExtern   = atoi(Value);
  else if (!strcasecmp(Name, "AC3Gain"))                 Pvr350Setup.AC3Gain                = atoi(Value);
  else if (!strcasecmp(Name, "DeviceNumber"))            Pvr350Setup.DeviceNumber           = atoi(Value);
  else if (!strcasecmp(Name, "BlackVideoForAudioOnly"))  Pvr350Setup.BlackVideoForAudioOnly = atoi(Value);
  else if (!strcasecmp(Name, "RecodeMP2"))               Pvr350Setup.RecodeMP2              = atoi(Value);
  else
    return false;
  return true;
}

const char **cPluginPvr350::SVDRPHelpPages(void)
{
  // Return help text for SVDRP commands this plugin implements
  static const char *HelpPages[] = {
    "WSS_16:9\n"
    "    Send a 16:9 (anamorphic) WSS signal to the TV\n"
    "    Note: makes only sense if UseWssBits is set to 'no' or for PlayMode pmExtern (e.g. mplayer)\n"
    "    Otherwise autodetection will immidiately overwrite the signal",
    "WSS_4:3\n"
    "    Send a 4:3  WSS signal to the TV\n"
    "    Note: makes only sense if UseWssBits is set to 'no' or for PlayMode pmExtern (e.g. mplayer)\n"
    "    Otherwise autodetection will immidiately overwrite the signal",
    "WSS_ZOOM\n"
    "    Send a 'zoom 4:3 to 16:9 Letterbox' WSS signal to the TV\n"
    "    Note: makes only sense if UseWssBits is set to 'no' or for PlayMode pmExtern (e.g. mplayer)\n"
    "    Otherwise autodetection will immidiately overwrite the signal",
    "RESET\n"
    "    Reset pvr350-plugin\n",
    NULL
    };
  return HelpPages;
}

cString cPluginPvr350::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  if(!strcasecmp(Command,"WSS_16:9"))          { pvr350device->Set_wss_mode(7);    return "16:9 WSS signal sent";   }
  else if(!strcasecmp(Command,"WSS_4:3"))      { pvr350device->Set_wss_mode(8);    return "4:3 WSS signal sent";    }
  else if(!strcasecmp(Command,"WSS_ZOOM"))     { pvr350device->Set_wss_mode(11);   return "Zoom WSS signal sent";   }
  else if(!strcasecmp(Command,"RESET"))        { new cPvr350Device();              return "resetted pvr350- plugin";}
  return NULL;
}

VDRPLUGINCREATOR(cPluginPvr350); // Don't touch this!
