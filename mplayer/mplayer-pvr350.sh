#!/bin/bash
#
# $Id: mplayer.sh for pv350$
# heavy modified by Dr. Seltsam for PVR350
# -based on 0.8.7 from juri
# -includes NTSC fixes by jha from forthcoming 0.8.8
#- add -aid to mplayer when AID is given (Matthias Schwarzott <zzam@gentoo.org>)

unset LANG
declare VERSION="cvidix-pvr350-2009-09-24"


function initialize () {
  # source config file
  source "$CFGFIL"

  # Debug Mode ?
  if test -z "$DEBUG" -o "$DEBUG" != "true"; then DEBUG=false; fi

  # use AC3?
  if test -z "$USEAC3" -o "$USEAC3" != "true"; then USEAC3=false; fi
  debugvar USEAC3 "$USEAC3"

  # AC3 command line
  if test -z "$AC3AOUT"; then
    errorcfg AC3AOUT
    exit
  else
    debugvar AC3AOUT "$AC3AOUT"
  fi

  # TV aspect ratio
  if test "$TV_ASPECT" == "16/9"; then
       MONITORASPECT="-monitoraspect 16:9";
  elif test "$TV_ASPECT" == "auto"; then
         if grep -q "^VideoFormat = 1" /etc/vdr/setup.conf; then
           MONITORASPECT="-monitoraspect 16:9";
         else
           MONITORASPECT="-monitoraspect 4:3";
         fi
  else
       MONITORASPECT="-monitoraspect 4:3";
  fi
  debugvar MONITORASPECT "$MONITORASPECT"


  # can do PAL?
  if test -z "$PAL" -o "$PAL" != "true"; then PAL=false; fi
  debugvar PAL "$PAL"

  # can do NTSC?
  if test -z "$NTSC" -o "$NTSC" != "true"; then NTSC=false; fi
  debugvar NTSC "$NTSC"

  if test $NTSC == "false" -a $PAL == "false"; then
    echolog "*** FATAL: Config Options NTSC and PAL both set to false ... Exiting."
    exit
  fi

  # use speed setting?
  SPEED=""
  if test -z "$USE_SPEED"; then USE_SPEED=false; fi
  debugvar USE_SPEED "$USE_SPEED"

  # use dvdnav for playing DVDs?
  if test -z "$DVDNAV" -o "$DVDNAV" == "false"; then DVDNAV=false; fi
  debugvar DVDNAV "$DVDNAV"

  # which detelecining filter should be used, if configured
  # to play NTSC as PAL
  if test -z "$DETC_FILTER"; then DETC_FILTER="detc=dr=2:am=1"; fi
  debugvar DETC_FILTER "$DETC_FILTER"

  # where is Mplayer
  if ! test -x "$MPLAYER" -a -f "$MPLAYER"; then
    echolog "*** Option MPLAYER not found in config file or not set correctly"
    exit
  else
    debugvar MPLAYER "$MPLAYER"
  fi

  # where is svdrpsend.pl
  if ! test -x "$SVDRPSEND" -a -f "$SVDRPSEND"; then
    echolog "*** Option SVDRPSEND not found in config file or not set correctly"
    exit
  else
    debugvar SVDRPSEND "$SVDRPSEND"
  fi

  if test -z "$VO"; then errorcfg VO; exit; else debugvar VO "$VO"; fi

  if test -z "$AO"; then errorcfg AO; exit; else debugvar AO "$AO"; fi
  AOUT="-ao $AO"

  if test -z $CACHE; then
    echolog "*** Option CACHE not set in config file - calling mplayer without Cache!"
    CACHESTR="-nocache"
  else
    CACHESTR="-cache $CACHE"; debugvar CACHE "$CACHE"; debugvar CACHESTR "$CACHESTR"
  fi

  if test -z "$FRAMEDROP" -o "$FRAMEDROP" != "true"; then FRAMEDROP=false; fi
  debugvar FRAMEDROP "$FRAMEDROP"
  if $FRAMEDROP; then FDSTR="-framedrop"; fi 
  debugvar FDSTR "$FDSTR"

  declare LIRCSTR="" # no extra Lirc option!
  if ! test -z "$LIRCRC"; then LIRCSTR="-lircconf $LIRCRC"; fi
  debugvar LIRCRC "$LIRCRC"; debugvar LIRCSTR "$LIRCSTR"

  if ! test -z "$SUBPOS"; then SUBTITLES=" -subpos $SUBPOS"; fi
  if ! test -z "$SUBCOLOR"; then SUBTITLES="$SUBTITLES -sub-bg-color $SUBCOLOR"; fi
  if ! test -z "$SUBALPHA"; then SUBTITLES="$SUBTITLES -sub-bg-alpha $SUBALPHA"; fi

  debugvar SUBTITLE "$SUBTITLES"

  if test "$SLAVE" != "SLAVE"; then
    REMOTE="$LIRCSTR"
  else
    REMOTE="-slave -nolirc"
  fi
  debugvar REMOTE "$REMOTE"

  if test -n "${AID}"; then
    AUDIO="-aid ${AID}"
  fi
  debugvar AUDIO "${AUDIO}"

  if ! test -z "$USERDEF"; then echolog "*** Use Option USERDEF at your own risk!"; fi
  debugvar USERDEF "$USERDEF"

  if ! test -d "$DVDFiles"; then debugmsg "*** Option DVDFiles not set correctly! You will not be able to play VCD/DVD" ""; DVDFiles=""; fi
  debugvar DVDFiles "$DVDFiles"
  if ! test -b "$DVD"; then debugmsg "*** Option DVD not set correctly! You will not be able to play VCD/DVD" ""; DVD=""; fi
  debugvar DVD "$DVD"
  if test -z $DVDLANG; then DVDLANG="en"; fi
  debugvar DVDLANG "$DVDLANG"

  # extra DVD options
  debugvar DVDOPTIONS "$DVDOPTIONS"

  # extra VCD options
  debugvar VCDOPTIONS "$VCDOPTIONS"

  # play MPEG files using the v4l2 output driver (hardware mpeg2) instead of cvidix?
  if test -z "$MPEG_DIRECT" -o "$MPEG_DIRECT" != "false"; then MPEG_DIRECT="true"; fi
  debugvar MPEG_DIRECT "$MPEG_DIRECT"

  # get the file extension of the video
  SUFFIX=$(echo -e "${FILE:$[${#FILE}-4]:4}" | tr [A-Z] [a-z])
  debugvar SUFFIX $SUFFIX

  # volume gain for AC3
  debugvar AC3_GAIN "$AC3_GAIN"

  # Special options for h264 streams
  debugvar H264_OPTIONS "$H264_OPTIONS"

  debugvar FILE "$FILE"

  return;
}


function getvidxy () {

  # call: getvidxy
  # determine x and y resolution of the file! 
  # output: variable ORIG_X and ORIG_Y (global)

  # variable definitions
  local TEMP1 MPLAYER_RETURN
  ORIG_X=0; ORIG_Y=0

  if test "$FILE" == "$DVDFiles/DVD"; then
    TEMP1=`$MPLAYER -identify -vo null -ao null -frames 0 -dvd-device $DVD dvd:// 2>/dev/null | grep -i -e "^ID_"`
  else
    TEMP1=`$MPLAYER -identify -vo null -ao null -frames 0 "$FILE" 2>/dev/null | grep -i -e "^ID_"`
  fi
  MPLAYER_RETURN=$?
  debugmsg "OutputFromMPLAYER:" "$TEMP1"
  debugmsg "MPLAYER_RETURN: " "$MPLAYER_RETURN"

  if test $MPLAYER_RETURN -ne 0; then
     echolog "*** FATAL: something went wrong analyzing the video; mplayer reported an error!" 
     echolog "*** FATAL: check your mplayer installation. Exiting..." 
     exit
  fi

  ORIG_X=`echo "$TEMP1"|grep ID_VIDEO_WIDTH|cut -d"=" -f2`
  debugmsg "parsed output for ORIG_X:" $ORIG_X

  ORIG_Y=`echo "$TEMP1"|grep ID_VIDEO_HEIGHT|cut -d"=" -f2`
  debugmsg "parsed output for ORIG_Y:" $ORIG_Y

  ORIG_FPS=`echo "$TEMP1"|grep ID_VIDEO_FPS|cut -d"=" -f2`
  debugmsg "parsed output for ORIG_FPS:" $ORIG_FPS

  ORIG_ASPECT=`echo "$TEMP1"|grep ID_VIDEO_ASPECT|cut -d"=" -f2`
  debugmsg "parsed output for ORIG_ASPECT:" $ORIG_ASPECT

  VIDEO_FORMAT=`echo "$TEMP1"|grep ID_VIDEO_FORMAT|cut -d"=" -f2`
  debugmsg "parsed output for VIDEO_FORMAT:" $VIDEO_FORMAT

  VIDEO_CODEC=`echo "$TEMP1"|grep ID_VIDEO_CODEC|cut -d"=" -f2`
  debugmsg "parsed output for VIDEO_CODEC:" $VIDEO_CODEC

  AUDIO_CODEC=`echo "$TEMP1"|grep ID_AUDIO_CODEC|cut -d"=" -f2`
  debugmsg "parsed output for AUDIO_CODEC:" $AUDIO_CODEC

  AUDIO_FORMAT=`echo "$TEMP1"|grep ID_AUDIO_FORMAT|cut -d"=" -f2`
  debugmsg "parsed output for AUDIO_FORMAT:" $AUDIO_FORMAT

  if test $ORIG_Y -ge $MINIMUM_Y_FOR_FS; then 
    FS="-fs";
    echolog "*** INFO: use full screen"
  else
    echolog "*** INFO: resolution too small, will not use full screen"
  fi

  return;
}


function checkforac3 () {
  if test "X$AUDIO_CODEC" == "Xa52" && $USEAC3; then AOUT="$AC3AOUT"; fi
  if test "X$AUDIO_CODEC" == "Xa52"; then AC3GAIN="$AC3_GAIN"; fi
  return; 
}

function checkforh264 () {
  if test "X$VIDEO_CODEC" == "Xffh264"; then H264OPTIONS="$H264_OPTIONS"; fi
  return; 
}

function checktvnorm () {
local -i TEMP_FPS
TEMP_FPS=`echo $ORIG_FPS|sed 's/\.//'`
if [ $TEMP_FPS -ge "23000" -a $TEMP_FPS -le "24499" ]; then
   debugmsg "Film"
   if $NTSC; then
      NEW_FPS="-fps 24000/1001" # 23.976
   else
      NEW_FPS="25"
      $USE_SPEED && SPEED="-speed 1.0427" # 25/23.976 = 1.0427
   fi
elif [ \( $TEMP_FPS -ge "14000" -a $TEMP_FPS -le "16000" \) -o \( $TEMP_FPS -ge "29000" -a $TEMP_FPS -le "30499" \) ]; then
   debugmsg "NTSC"
   if $NTSC; then
      NEW_FPS="-fps 30000/1001"
   else
      NEW_FPS="-fps 25"
      if $USE_SPEED; then
      SPEED="-speed 1.0427" # 25/23.976 = 1.0427
      #SPEED="-speed 0.8324" # 25/29.97 = 0.8342 #for progressive ?
      DETC=",$DETC_FILTER"
      fi
   fi
elif [ "$TEMP_FPS" == "25" ] || [ "$TEMP_FPS" == "25000" ] || [ "$TEMP_FPS" == "12500" ]; then
   debugmsg "PAL"
    if $PAL; then
      NEW_FPS="-fps 25"
    else
      NEW_FPS="30000/1001" # 29.97
      $USE_SPEED && SPEED="-speed 1.20" # 30/25 = 1.20
    fi
else
    debugmsg "unknown"  $ORIG_FPS
    echolog "*** INFO: Potentially unreliable framerate, will give mplayer no fps option"
    NEW_FPS=""
fi
return;
}


function echolog () {
  # prints the string on stdout and into /var/log/messages using logger!
  logger -s -- "$1"
  return;
}


function errorcfg () {
  echolog "*** FATAL: Config Option $1 not found in config file ... Exiting."
  exit;
}


function debugvar () {
  if $DEBUG; then echolog "*** DEBUG: Variable $1 has value \"$2\""; fi
  return;
}


function debugmsg () {
  if $DEBUG; then echolog "*** DEBUG: $1 \"$2\""; fi
  return;
}


# begin main!
#
#
# ---------------------------------------------------------------------------
#
#

declare -i ORIG_X ORIG_Y
declare ORIG_FPS NEW_FPS ORIG_ASPECT VIDEO_FORMAT VIDEO_CODEC AUDIO_CODEC AUDIO_FORMAT  WSS
declare CMDLINE AOUT H264OPTIONS AC3GAIN REMOTE AUDIO USERDEF SUFFIX
declare FILE="$1"
shift
declare FDSTR=""
declare SUBTITLES=""
declare DETC=""
declare FS=""
declare AC3_GAIN=""
declare H264_OPTIONS=""

while [[ -n $1 ]]; do
	case ${1} in
	SLAVE) SLAVE=SLAVE ;;
	AID) AID=${2}; shift ;;
	esac

	shift
done

echolog "*** Starting mplayer.sh Version $VERSION"

if test -z "$FILE"; then echolog "*** USAGE: mplayer.sh <File_to_be_played>"; exit; fi
if ! test -r "$FILE"; then echolog "*** ERROR: Make sure $FILE exists and is readable - otherwise it cannot be played ;-)"; exit; fi

# Check if config file exists!
declare CFGFIL="${0}.conf"
debugvar CFGFIL $CFGFIL
if ! test -f $CFGFIL; then echolog "*** FATAL: mplayer.sh.conf not found!!! Exiting." ; exit; fi

# read config file and initialize the variables
initialize

if test \( "$FILE" == "$DVDFiles/DVD" -o "$FILE" == "$DVDFiles/VCD" \) -a -n "$DVDFiles" -a -n "$DVD"; then
    if test "$FILE" == "$DVDFiles/DVD"; then
    	$USEAC3 && AOUT="$AC3AOUT"
    	if test $MPEG_DIRECT == "true"; then
          getvidxy
          if test $AUDIO_FORMAT == "80"; then
             CMDLINE="$MPLAYER -vo v4l2 -ao v4l2 -ac hwmpa -noaspect $FDSTR $CACHESTR $AUDIO -alang de,en $REMOTE $USERDEF -dvd-device $DVD dvd://"
          fi
        else
           if $DVDNAV; then
             CMDLINE="$MPLAYER -vo $VO -fs $AOUT $MONITORASPECT  $DVDOPTIONS $FDSTR -nocache $AUDIO -alang de,en  $REMOTE $USERDEF -dvd-device $DVD dvdnav://"
           else
             CMDLINE="$MPLAYER -vo $VO -fs $AOUT $MONITORASPECT  $DVDOPTIONS $FDSTR $AUDIO -alang de $REMOTE $USERDEF -dvd-device $DVD dvd://"
           fi
        fi
    fi
    if test "$FILE" == "$DVDFiles/VCD"; then
        CMDLINE="$MPLAYER -vo $VO $AOUT $VCDOPTIONS $FDSTR $CACHESTR $AUDIO $REMOTE $USERDEF -cdrom-device $DVD vcd://"
    fi
    unset FILE
elif test "${SUFFIX}" == ".cue"; then
    CMDLINE="$MPLAYER -vo $VO $AOUT $FDSTR $CACHESTR $AUDIO $REMOTE $USERDEF cue://$FILE:2"
    unset FILE
elif test "${SUFFIX}" == ".iso"; then
#elif if test \( "$SUFFIX" == ".img" -o "$SUFFIX" == ".iso" \); then
     if $DVDNAV; then
       CMDLINE="$MPLAYER -vo $VO -fs $AOUT $MONITORASPECT  $DVDOPTIONS $FDSTR -nocache $AUDIO -alang de,en  $REMOTE $USERDEF -dvd-device $FILE dvdnav://"
     else
       CMDLINE="$MPLAYER -vo $VO -fs $AOUT $MONITORASPECT  $DVDOPTIONS $FDSTR $AUDIO -alang de $REMOTE $USERDEF dvd://$FILE"
     fi
    unset FILE
elif  (( [[ "$FILE" == *VIDEO_TS* ]] || [[ "$FILE" == *video_ts* ]] || [[ "$FILE" == *VTS_* ]] || [[ "$FILE" == *vts_* ]] ) && $DVDNAV ) ; then
     d=$(dirname $FILE)
     logger -s -- "$d"
       CMDLINE="$MPLAYER -vo $VO -fs $AOUT $MONITORASPECT  $DVDOPTIONS $FDSTR -nocache $AUDIO -alang de,en  $REMOTE $USERDEF -dvd-device $d dvdnav://"
    unset d
    unset FILE
else
    # Try to determine the video attributes
    if $DEBUG; then echolog "*** DEBUG: Calling getvidxy function to analyze source video stream ..."; fi
    getvidxy
    echolog "*** INFO: Source Video has Resolution of $ORIG_X x $ORIG_Y ..."
    if test $ORIG_X -eq 0 -o $ORIG_Y -eq 0; then echolog "*** FATAL: something went wrong analyzing the video; check your mplayer version ..."; exit; fi

    checkforac3
    checkforh264
    checktvnorm
    debugvar SPEED "$SPEED"

    if test $MPEG_DIRECT == "true" -a \( $VIDEO_FORMAT == "0x10000001" -o $VIDEO_FORMAT == "0x10000002" \) -a \
             \( $AUDIO_FORMAT == "80" -a $AUDIO_CODEC == "mp3" \) -a \( \
             \( $PAL  -a $ORIG_FPS == "25.000" -a \( \( $ORIG_X == "352" -a $ORIG_Y == "288" \) -o \( $ORIG_Y == "576" -a \
             \( $ORIG_X == "352" -o $ORIG_X == "480" -o $ORIG_X == "528" -o $ORIG_X == "544" -o $ORIG_X == "704" -o $ORIG_X == "720" \) \) \) \) \) ; then
      CMDLINE="$MPLAYER -vo v4l2 -ao v4l2 -ac hwmpa $FDSTR $CACHESTR $AUDIO $REMOTE"
    else
      if test $ORIG_X -le 720 -a $ORIG_Y -le 576; then
        CMDLINE="$MPLAYER -vo $VO $FS $AOUT $AC3GAIN $MONITORASPECT $NEW_FPS $SPEED $FDSTR $CACHESTR $AUDIO $REMOTE $SUBTITLES $USERDEF $FORCEIDX"
      else # resolution exceeds hardware capability of PVR350, so we need to scale it down to the maximum resolution of 720x576
      #CMDLINE="$MPLAYER -vo $VO $AOUT $AC3GAIN $MONITORASPECT -vf scale=720:576${DETC} $NEW_FPS $SPEED $FDSTR $CACHESTR $AUDIO $REMOTE $SUBTITLES $USERDEF $FORCEIDX"
      CMDLINE="$MPLAYER -vo $VO $AOUT $AC3GAIN $MONITORASPECT -vf scale=720:576${DETC} $H264OPTIONS $NEW_FPS $SPEED $FDSTR $CACHESTR $AUDIO $REMOTE $SUBTITLES $USERDEF $FORCEIDX"

      fi
    fi
fi

if grep -q "^pvr350.UseWssBits = 1" /etc/vdr/setup.conf; then
  if test "$MONITORASPECT" == "-monitoraspect 16:9"; then
    WSS="WSS_16:9";
    echolog "*** INFO: Setting 16:9 wss bit"
  else
    WSS="WSS_4:3"
    echolog "*** INFO: Setting 4:3 wss bit"
  fi
  exec $SVDRPSEND plug pvr350 $WSS &

fi

debugvar CMDLINE "$CMDLINE"
#exec $CMDLINE "$FILE" > /var/log/mplayer
exec $CMDLINE "$FILE"
# the following line includes mplayer-multi, see http://www.js-home.org/vdr/mplayer-multi/index.php
#exec /usr/bin/mplayer-multi $CMDLINE "$FILE"
exit
