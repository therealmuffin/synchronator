# Synchronator Configuration Manual

synchronator.conf - synchronator configuration file

Description
synchronator.conf is the configuration file for synchronator. If not specified on the command line, synchronator searches first at ../etc/synchronator.conf and then at /etc/synchronator.conf.

The configuration file is read using libconfig, therefore it should be formatted accordingly. It's manual can be found at:
http://www.hyperrealm.com/libconfig/libconfig_manual.html


## Interface configuration

interface - set the interface for communication with amplifier [serial`*`|i2c]


### Serial specific options

serial_port - location of serial device (e.g. /dev/ttyS0)

serial_baud - must be a valid baud rate

serial_bits - data bits [2-8|8`*`]

serial_parity - sets whether a parity bit is used [TRUE|FALSE`*`]

serial_even - even parity bit, only used when parity bit is used [TRUE|FALSE`*`]

serial_2stop - 2 stop bits [TRUE|FALSE`*`]


### I2C specific options

i2c_device - location of serial device

i2c_address - i2c address


## Data processing configuration

data_type - sets data type [ascii|numerical]

sync_2way - specifies whether to listen to incoming commands from the serial port [TRUE|FALSE*]

diff_commands - specifies whether to use different commands for listening and sending commands [TRUE|FALSE*]


#####[volume section]

The boundaries set by 'min' and 'max' define the minimum and maximum volume levels. The volume level can not be increased past this level via e.g. MPD. However, if 'sync_2way' is 'TRUE', manually moving the volume level past the boundaries at the amplifier end will adjust the level boundaries accordingly. When moving back to the original boundaries set in the configuration file (e.g. via MPD or amplifier), so will the applied boundaries by synchronator. This prevents large volume jumps when manually pushing the level past boundaries set in synchronator.

discrete - specifies whether to use absolute (TRUE) or relative (FALSE) volume levels [TRUE*|FALSE]

- discrete volume levels use 'min' and 'max' to set volume level boundaries
- non-discrete volume levels use 'min' and 'plus' to specify command for adjusting relative volume levels

#####[query section]

Some amplifiers do not give automatic status updates. By setting a trigger and an appropriate interval (sec, 0 is only once at startup), a request for a status update can be issued through the interface port.


#####[other sections]

Sections other than those discussed above can have any random name. However, it is recommended to use section name as used in the sample configurations, namely input and power, if appropriate.

- register - if set to TRUE it will be searched for matching incoming commands [TRUE|FALSE*]


### ASCII specific options

ASCII values must be delimited by double quotes

#####[volume section]

length - does the amplifier expect '02' set to 2, if '002' set to 3, '2' set to 1 (default).

precision - how many numbers behind the dot.


### Numeric specific options

Numerical values must be within 0-255 bounds (unsigned 8 bit values).


---------------------------------------------------------------------
*) the * character indicates the default option/setting