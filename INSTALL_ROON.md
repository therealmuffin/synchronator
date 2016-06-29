# Synchronator Installation for Roon - additional steps

Roon does not allow setting different devices for audio and mixer data. Therefore, two (or one) additional steps are necessary to make Roon work with Synchronator


## Roon required additional steps

Two additional steps are required to make Roon work with Synchronator.<sup>*</sup>

After finishing the steps below, you can continue with the regular installation manual. However, you can skip the step on setting up the dummy mixer. If you want to configure apps like MPD, Shairport do notice that you do not point the mixer to the dummy audio device as instructed but to your actual device and mixer (e.g. hw:0, PCM respectively). Since these applications expect the mixer to always be there, you might want to start and stop them together with your audio device (see the second step on how to do that).<sup>*</sup>

*) If your audio device is internal (and therefore always active), you can skip the second step.


### Create a dummy volume control for your audio device (or dac)

To enable the volume function in Roon we need to create a dummy volume control for the audio device.

Install Alsa utilities with the command
```sudo apt-get install alsa-utils```

We need to add a dummy volume control to the audio device. Therefore edit the file ```/var/lib/alsa/asound.state```. If this file does not exist, you can create it with the command ```sudo alsactl store```.

In the file, find your audio device and add the following two controls to its entry:

<pre>
        control.2 {
                iface MIXER
                name 'PCM Playback Switch'
                value true
                comment {
                        access 'read write user'
                        type BOOLEAN
                        count 1
                }
        }
        control.3 {
                iface MIXER
                name 'PCM Playback Volume'
                value 15
                comment {
                        access 'read write user'
                        type INTEGER
                        count 1
                        range '0 - 64'
                        dbmin -6400
                        dbmax 0
                        dbvalue.0 -4900
                }
        }
</pre>

It now should look something like this:

<pre>
state.DAC {
        control.1 {
                iface PCM
                name 'Playback Channel Map'
                value.0 3
                value.1 4
                comment {
                        access read
                        type INTEGER
                        count 2
                        range '0 - 36'
                }
        }
        control.2 {
                iface MIXER
                name 'PCM Playback Switch'
                value true
                comment {
                        access 'read write user'
                        type BOOLEAN
                        count 1
                }
        }
        control.3 {
                iface MIXER
                name 'PCM Playback Volume'
                value 15
                comment {
                        access 'read write user'
                        type INTEGER
                        count 1
                        range '0 - 64'
                        dbmin -6400
                        dbmax 0
                        dbvalue.0 -4900
                }
        }
}
</pre>

Run ```alsactl restore```, now you should have a control called PCM for your audio device. Check ```alsamixer```. Restart Roon and the control should be detected and the device volume function be enabled.

When configuring Synchronator, remember to define the newly created mixer accordingly (mixer_device, mixer_name). The same goes for any other applications you might have running (e.g. Shairport, MPD).


### Make Synchronator start and stop when connecting your external audio device.

Contrary to the dummy soundcard normally used in combination with Synchronator, an external audio device (e.g. usb) may or may not be available to the computer. The audio device may be in standby, for example. Since Synchronator expects the mixer controls to be available at all times, we need to start and stop Synchronator together with the audio device. Luckily that's quite easy. Udev can take care of that.

To make this work we need the following value for the audio device: ID_MODEL

Run the following command and unplug and replug your audio device device. It should list the value for 'ID_MODEL'.
```udevadm monitor --property |grep ID_MODEL``` (previously ```udevmonitor --env```)


Replace that value in the script below and place it in a file called ```synchronator.rules``` in ```/lib/udev/rules.d```.
<pre>
SUBSYSTEM!="sound", GOTO="synchronator_end"

ACTION=="change", ENV{ID_MODEL}=="Leema_Elements_DAC", ENV{SOUND_INITIALIZED}=="1", RUN+="/etc/init.d/synchronator start"
ACTION=="remove", ENV{ID_MODEL}=="Leema_Elements_DAC", RUN+="/etc/init.d/synchronator stop"

LABEL="synchronator_end"
</pre>
*) You can also run an intermediate script to start other applications along, such as Shairport or MPD/UPMPDCLI for DLNA functionality.

Run ```chmod 644 /lib/udev/rules.d/synchronator.conf``` and ```chmod root:root /lib/udev/rules.d/synchronator.conf``` to allow proper access to the file.

Execute the following command for the changes to take effect:
```udevadm control --reload-rules```

When plugging and unplugging the external audio device, Synchronator should start automatically. It follows that you do not have to activate the init.d script anymore with ```update-rc.d``` as instructed in the extended manual.
