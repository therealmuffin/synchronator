# Synchronator Extended Installation Manual

Name: Synchronator

This file provides a more detailed description of the Synchronator installation procedure.

## Debian (and derivatives)

run 'apt-get update'

#####Install required packages
<pre>
apt-get install libasound2-dev
apt-get install pkg-config
apt-get install git
</pre>

#####Install I2C development package (if required)
<pre>
apt-get install libi2c-dev
</pre>

#####Install libconfig:
<pre>
wget http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz
tar -zxf libconfig-1.5.tar.gz
cd libconfig-1.5
./configure
make
make install
</pre>

Assuming there were no errors, remove the libconfig directory as it is
not required anymore:
<pre>
cd ..
rm -r libconfig-1.5
</pre>

Execute the following command to add the libconfig to LD_LIBRARY_PATH:
<pre>
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
</pre>

#####Install synchronator:
<pre>
git clone https://github.com/therealmuffin/synchronator.git
cd synchronator/src
./configure
make
make install
</pre>

if I2C support is required replace
<pre>
./configure
</pre>

by...

<pre>
./configure --enable-i2c
</pre>

If all this went without issues we need to load a dummy ALSA soundcard:
<pre>
modprobe snd_dummy
</pre>

To make sure the dummy device won't occupy index 0 when loaded at boot, which is preferred for the actual audio device, add the following line to /etc/modprobe.d/alsa-base.conf:
<pre>
options snd_dummy index=-2
</pre>

Add the following line to /etc/modules to make the module load at boot:
<pre>
snd_dummy
</pre>

#####MPD

Now we need to edit MPD configuration, e.g. /etc/mpd.conf

Under audio output, below to following lines:
<pre>
audio_output {
        type            "alsa"
        name            ...?
        device          ...?
</pre>

add the following three lines:
<pre>
        mixer_type      "hardware"
        mixer_device    "hw:Dummy"
        mixer_control   "Master"
</pre>

Comment out following line by adding a # in front of it (if present anywhere in the file).
<pre>
mixer_type                      "disabled"
</pre>
becomes:
<pre>
#mixer_type                      "disabled"
</pre>

...restart MPD:
<pre>
/etc/init.d/mpd restart
</pre>

Now if you change the volume in MPD you should be able to see the change
going to the dummy soundcard in alsa, run
<pre>
apt-get install alsa-utils
alsamixer
</pre>
type F6 and select Dummy.

Now we need to make a final adjustment in the synchronator configuration
file and change the serial port. Go to its directory:
<pre>
cd sample_configurations
</pre>

Read the readme file and take a look at the different configuration files. This 
information in combination with the documentation of your amplifier should enable you to 
create a configuration file for you amplifier.

Copy the configuration file to the directory /usr/local/etc/ or /etc/ with the name 
synchronator.conf:
<pre>
/usr/local/etc/synchronator.conf
</pre>

##### Testing the setup

It's time to test the setup. Run the following:
<pre>
synchronator -v2
</pre>

Now it runs in verbose mode so you can see exactly what it's
doing.

If you don't see any extra output open a new ssh screen and type:
<pre>
tail -f /var/log/syslog | grep synchronator
</pre>

Test your setup (without source or speakers connected). 

##### Making Synchronator boot at start-up

If it runs without issues make it
start at boot, for Debian/Ubuntu/Voyage/etc systems:
<pre>
cp the synchronator script in the scripts/init.d directory to /etc/init.d
</pre>

Then run:
<pre>
sudo chmod 755 /etc/init.d/synchronator
sudo update-rc.d synchronator defaults 
</pre>

To make Synchronator run as a different user, create a user. Do note that for Synchronator to listen to privileged ports (below 1024), e.g. Heos, it needs to run with root rights.
<pre>
adduser --no-create-home synchronator
</pre>

... and add it to the appropriate groups to allow access and control over Alsa and the interface (e.g. serial port or i2c).

<pre>
usermod -a -G audio synchronator
usermod -a -G dialout synchronator
</pre>

Set the directory /tmp to permissions 1777

<pre>
chmod 1777 /tmp
</pre>

####KNOWN ISSUES:
- One known case where baud rate of the serial port was not set correctly. I do not yet
know why and where the problem lies. If it does happen (please report), you can set the 
port manually with the following stty, e.g. in the case of the serial port at /dev/ttyS0 
and the required baud rate at 9600:
stty -F /dev/ttyS0 9600
