# Synchronator Installation Manual

## Requirements
- amplifier that can be controlled externally (rs232, (inverted-)ttl, i2c, tcp/ip/network)
- computer (obviously)
- Alsa (if there is any interest I can investigate adding OSS)
- http server with PHP (only if using the MPoD/MPaD php companion script)

## Preparation serial connection:
- If the amplifier has an RS232 connection: simply, take the appropriate cable and hook it up.
- For TTL signals: Soekris and some ALIX boards have a TTL output. Otherwise see below.
- For inverted TTL signals (e.g. Leema): use a RS232 to INV-TTL board.
I've used: http://www.robotshop.com/eu/en/droids-db9-serial-adapter.html
- It can convert RS232 to TTL and TX to INV-TTL. for RX you can take on of the RS232 header, that should work fine.

## Installation (in a nutshell):
- read the [Roon manual](INSTALL_ROON.md) first if you're using Synchronator with Roon
- modprobe snd_dummy and add it to /etc/modules
- To ensure that the actual audio device can always take position 0 and allow for easier configuration in your audio program, add to /etc/modprobe.d/alsa-base.conf: 
<pre>
    options snd_dummy index=-2
</pre>
- in mpd.conf: add to audio_output with type 'alsa':
<pre>
	mixer_type	    "hardware"
	mixer_device	"hw:Dummy"
	mixer_control	"Master"
</pre>
- download, compile, etc. libconfig from the source below. 
    It is available from apt but that was somehow kind of buggy.
    Don't forget to add it to your path if it isn't already
- apt-get install libasound-dev
- configure, make, and make install to build synchronator.
- create/get configuration file
- modify php configuration file (optional)
- manually place all files at their appropriate location

## BUILD REQUIREMENTS
- libconfig (best to build it from [source](http://www.hyperrealm.com/libconfig/) rather than via apt).
- alsa development library
- i2c development library when configured with i2c support


## HOW TO GET STARTED
type ```.configure --help``` for all configuration options, e.g. to enable I2C:
```./configure --enable-i2c```

make

make install

synchronator -i synchronator.conf
