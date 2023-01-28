
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program;                                              *
 *   if not, write to the Free Software Foundation, Inc.,                  *
 *   59 Temple Place, Suite 330, Boston, MA  02111-1307  USA               *
 *                                                                         *
 ***************************************************************************/

#include "pvr350device.h"
#include "pvr350menu.h"
#include <vdr/plugin.h>
#include "pvr350.h"
#include <strings.h>

cPvr350MenuSetup::cPvr350MenuSetup() {

  newPvr350Setup = Pvr350Setup;

  Add(new cMenuEditIntItem(tr("Setup.pvr350$Log level"), \
      &newPvr350Setup.LogLevel, 0, 4));

  Add(new cMenuEditBoolItem(tr("Setup.pvr350$Wide Screen Signaling (WSS)"), \
      &newPvr350Setup.UseWssBits));

  Add(new cMenuEditBoolItem(tr("Setup.pvr350$16:9 WSS for pmExtern"), \
      &newPvr350Setup.WSS_169_for_pmExtern));

  Add(new cMenuEditIntItem(tr("Setup.pvr350$Device number (/dev/videoXX)"), \
      &newPvr350Setup.DeviceNumber, 16, 20));

  Add(new cMenuEditIntItem(tr("Setup.pvr350$AC3 Gain"), \
      &newPvr350Setup.AC3Gain, 1, 5));

  Add(new cMenuEditBoolItem(tr("Setup.pvr350$Black video for audio-only streams"), \
      &newPvr350Setup.BlackVideoForAudioOnly));

  Add(new cMenuEditBoolItem(tr("Setup.pvr350$Recode MP2"), &newPvr350Setup.RecodeMP2));      
}

void cPvr350MenuSetup::Store() {

  SetupStore("LogLevel", \
      Pvr350Setup.LogLevel = newPvr350Setup.LogLevel);

  SetupStore("UseWssBits", \
      Pvr350Setup.UseWssBits = newPvr350Setup.UseWssBits);

  SetupStore("WSS_16:9_for_pmExtern", \
      Pvr350Setup.WSS_169_for_pmExtern = newPvr350Setup.WSS_169_for_pmExtern);

  SetupStore("DeviceNumber", \
      Pvr350Setup.DeviceNumber = newPvr350Setup.DeviceNumber);

  SetupStore("AC3Gain", \
      Pvr350Setup.AC3Gain = newPvr350Setup.AC3Gain);

  SetupStore("BlackVideoForAudioOnly", \
      Pvr350Setup.BlackVideoForAudioOnly = newPvr350Setup.BlackVideoForAudioOnly);

  SetupStore("RecodeMP2", \
      Pvr350Setup.RecodeMP2 = newPvr350Setup.RecodeMP2);
}




