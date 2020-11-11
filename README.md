# Synchronator

Synchronator brings bit perfect volume control to Hi-Fi systems with Linux as source. 

This enables control of your Hi-Fi amplifier volume level for Airplay, DLNA, OpenHome, MPD, Squeezelite, and Roon a.o.


## Technical background

Contrary to many other operating systems in Linux it is not uncommon that audio applications, such as MPD and Shairport, allow audio data and mixer/volume data to be send to different (audio) devices. By sending mixer data to a dummy/virtual soundcard\*, volume control can be enabled without touching the audio data. Synchronator in turn can synchronise that volume level with any Hi-Fi system/amplifier that can be externally controlled (RS232/I2C/TCP/IR)\*\*. In addition, changes in volume level at the amplifier side are synced back.

\*) For Roon and the like a dummy mixer is created for the actual audio device/dac instead.

\*\*) Support for IR control (via LIRC) is experimental. Tester(s) is (are) needed.

## Requirements for audio applications

The only requirement for audio applications is that it allows Linux (Alsa) to take care of volume instead of some internal algorithm.

### Known supported applications
- [Music Player Daemon (MPD)](https://www.musicpd.org/)
- [UPMPD](http://www.lesbonscomptes.com/upmpdcli/) (DLNA/OpenHome)
- [Roon](https://roonlabs.com/) (RoonBridge, RoonServer)
- [Shairport](https://github.com/abrasive/shairport) and [derivatives](https://github.com/mikebrady/shairport-sync/) (Airplay)
- [Squeezelite](https://github.com/ralph-irving/squeezelite) [(required patch pending for approval)](https://github.com/therealmuffin/squeezelite)
- [Mopidy](https://www.mopidy.com/)
- [Kodi](https://kodi.tv/) (XBMC) (requires small change in sourcecode)


## Requirements for Hi-Fi amplifiers

Obviously, for a computer to control an amplifier that amplifier needs to be controllable. Many amplifiers are controllable via a serial connection (e.g. RS232, TTL, etc). 

Synchronator supports serial (RS232, TTL, etc), TCP and I2C connections. IR support is experimental. At this moment I2C and IR devices can only be controlled, changes at that end will not be synced back to Synchronator. If there is any use for this functionality (I didn't find any): post a request.

### Known supported amplifiers/brands
- Cambridge Audio
- Carry Audio Design
- Classé
- Devialet
- Dynaudio (Connect/Xeo/Focus-XD)
- Leema Acoustics
- Lyngdorf
- NAD
- Parasound
- ...


## Features summary

- Synchronisation of volume level changes between Linux and Hi-Fi amplifier
- Enable absolute volume control for amplifiers with relative volume control only
- HTTP/PHP interface for controlling miscellaneous amplifier options (power, input, etc)
- Replying to requests (e.g. status input, power, etc)

## Alternative uses

Synchronator is designed to synchronise the volume level between Linux and Hi-Fi. However, other applications can be thought of also. By running multiple instances of Synchronator, commands between incompatible devices can be translated. A few examples follow:

- With the NAD M50 Bluesound player one can control the volume level of other NAD equipment (dac or amplifier). While this is nice if you have that NAD equipment, it is less useful if you have an amplifier of another brand. Synchonator can translate commands between these two incompatible devices.

- It is possible to control your amplifier volume level from within Denon Heos and by extension Spotify Connect. The Denon Heos Link and Drive can control Denon and Marantz amplifiers via TCP/IP. Synchronator can translate these commands and enable this functionality for other brands as well.

## Installation and configuration

For installation check the [installation manual](INSTALL_MANUAL.md) or the [extended installation manual](INSTALL_MANUAL_EXT.md)

For setting the appropriate configuration settings check the [configuration manual](CONFIG_MANUAL.md)

For using Synchronator in combination with Roon, check the [Roon configuration manual](INSTALL_ROON.md).

