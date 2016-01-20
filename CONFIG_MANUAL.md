# Synchronator Configuration Manual

synchronator.conf - synchronator configuration file

Description
synchronator.conf is the configuration file for synchronator. If not specified on the command line, synchronator searches first at ../etc/synchronator.conf and then at /etc/synchronator.conf.

The configuration file is read using libconfig, therefore it should be formatted accordingly. It's manual can be found at:
http://www.hyperrealm.com/libconfig/libconfig_manual.html

This is a preliminary configuration manual. Synchronator may change as different requirements are defined and implemented.

## Interface configuration

interface - set the interface for communication with amplifier [serial`*`|i2c|tcp]


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


### TCP specific options

tcp_address - ip address of amplifier

tcp_port - tcp port of amplifier [1-99999]

tcp_slave - don't initiate connect to address [TRUE|FALSE`*`]

tcp_listen - allow incoming connections [TRUE|FALSE`*`]

tcp_max - maximum of incoming connections [1-100|10`*`]

tcp_echo - return an echo of incoming data [TRUE|FALSE`*`]


### ALSA options

mixer_name - sets the mixer name on device snd_dummy

mixer_index - sets the mixer index on mixer_name


## Data processing configuration

data_type - sets data type [ascii|numerical]

sync_2way - specifies whether to listen to incoming commands from the serial port [TRUE|FALSE*]

diff_commands - specifies whether to use different commands for listening and sending commands [TRUE|FALSE`*`]


### Main section settings

Parameters that could differ between incoming and outgoing commands are defined between parentheses (regardless of wether diff_commands is TRUE or not). If incoming and outgoing  parameters differ, the incoming version is defined first, followed by a comma and then the outgoing version. 

header - prefix to every command

event_delimiter - delimiter between command code and its parameter

tail - suffix to every command


#####[volume section]

The boundaries set by 'min' and 'max' define the minimum and maximum volume levels. The volume level can not be increased past this level via e.g. MPD. However, if 'sync_2way' is 'TRUE', manually moving the volume level past the boundaries at the amplifier end will adjust the level boundaries accordingly. When moving back to the original boundaries set in the configuration file (e.g. via MPD or amplifier), so will the applied boundaries by synchronator. This prevents large volume jumps when manually pushing the level past boundaries set in synchronator.

discrete - specifies whether to use absolute (TRUE) or relative (FALSE) volume levels [TRUE`*`|FALSE]

- discrete volume levels use 'min' and 'max' to set volume level boundaries
- non-discrete volume levels use 'min' and 'plus' to specify command for adjusting relative volume levels

header - prefix added to every volume command code (thus post the header defined in the main section)

curve - Volume curve can be linear or logarithmic. If volume control is relative, only linear is allowed [linear`*`|log]

###### [response volume sub-section]

The following settings allow translation of the incoming volume range to the outgoing volume range (if different).

pre_offset - offset added pre multiplier

multiplier - multiplier applied on the incoming volume command

post_offset - offset added post multiplier

invert_multiplier - for ease in use the multiplier can be inverted [TRUE`*`|False]


#####[query section]

Some amplifiers do not give automatic status updates. By setting a trigger and an appropriate interval (sec, 0 is only once at startup), a request for a status update can be issued through the interface port.


#####[other sections]

Sections other than those discussed above can have any random name. However, it is recommended to use section name as used in the sample configurations, namely input and power, if appropriate.

register - if set to TRUE it will be searched when matching incoming commands codes [TRUE|FALSE`*`]

header - prefix added to every command code (thus post the header defined in the main section)

Command codes defined in other sections can have any random name. Their definition is formatted as described in this manual in their appropriate section (e.g. ASCII or Numeric). These commands can be triggered via the http interface. If an incoming command is detected, its name and parameter is written to /tmp/synchronator.'name' for use by the http interface.


### ASCII specific options

ASCII values must be delimited by double quotes

All ASCII variables (header, tail, delimiter, etc) are mandatory. If they're not necessary just leave them empty. One exception is tail, this variable must at least be one character long (otherwise it would be impossible to distinguish between the beginning and end of commands).


#####[volume section]

tail - suffix added to every volume command code (thus pre the tail defined in the main section)

length - does the amplifier expect '02' set to 2, if '002' set to 3, '2' set to 1 (default).

precision - how many numbers behind the dot.

#####[response section]

indicator - commands stripped from their header and tail, without even_delimiter present as set in the configuration, if this character is matched, it will be processed as an incoming command.

To match incoming requests, requests and their reply's need to be defined. The name of the request (array) needs to match either 'volume' or the name of any of the 'other sections' defined in the configuration file. 

Each name defines an array containing three values (e.g. 'input=(FALSE, "command code = parameter", "usb")').

1) The first value defines whether to reply a default reply or the actual value (either way defined by the 3rd value in the array) [TRUE|False]

2) The second value defines the request to be matched, stripped from the header and tail defined in the main section

3) In case of a:

- default reply: the 3rd value will be send over the interface (e.g. serial). No header or tail is added. 

- custom reply: Synchronator will attempt to reply the status of that category (e.g. power, input). If the status is unknown, synchronator will reply with the default value as set in this 3rd value. Obviously, this value needs to match any of the names as defined in the section of which this array lends its name. Check the NAD-M51.conf for an example.


### Numeric specific options

Numerical values must be within 0-255 bounds (unsigned 8 bit values).

The header in the main section is formatted as an array delimited by square brackets

None of the numeric data variables are mandatory except for volume. If no other variables are defined (header, tail, etc), every incoming byte is assumed to be volume data. Every outgoing byte represents volume data

### Glossary

command - complete command send to target device

command code - section that defines the (type of) instruction to device

parameter - parameter to command code

### Help

Take a look at the sample configuration files. If still not clear, post a message at Github.

---------------------------------------------------------------------
*) the * character indicates the default option/setting