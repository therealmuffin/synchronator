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

tcp_send_all - send all data to all open connections except for specific requests [TRUE|FALSE`*`]

tcp_max - maximum of incoming connections [1-100|10`*`]

tcp_echo - return an echo of incoming data [TRUE|FALSE`*`]

tcp_persistent - persist in trying to recover a broken connection to defined tcp_address/port [TRUE`*`|FALSE]


### LIRC specific options (experimental)

lirc_socket - location of LIRC socket

remote_name - name of remote as set in LIRC configuration

This interface only functions with 'data_type' set to 'ascii'. The resulting output send to this interface must correspond with the key names as configured in LIRC. 2way synchronisation is not implemented (how could it?).


### ALSA options

The following options are only necessary if Synchronator is used in combination with Roon or if multiples zones (and therefore multiple instances of Synchronator) run on the same computer.

mixer_device - sets the mixer device [hw:Dummy`*`]

mixer_name - sets the mixer name on mixer_device [Master`*`]

mixer_index - sets the mixer index on mixer_name [0`*`]

mixer_normalize - correct for audio applications using volume normalization [TRUE|FALSE`*`]


## Data processing configuration

data_type - sets data type [ascii|numerical]

sync_2way - specifies whether to listen to incoming commands from the serial port [TRUE|FALSE*]

diff_commands - specifies whether to use different commands for listening and sending commands [TRUE|FALSE`*`]

[comment]: function under consideration:  powerup_time - specifies the time it takes after a device-on event to properly process commands (from a previous device-off state). Currently this is only used for specific 'device-on' actions, thus default volume level at start [0-9|1`*`].

### Main section settings

Parameters that could differ between incoming and outgoing commands are defined between parentheses (regardless of wether diff_commands is TRUE or not). If incoming and outgoing  parameters differ, the outgoing version is defined first, followed by a comma and then the incoming version. 

header - prefix to every command

event_delimiter - delimiter between command code and its parameter

tail - suffix to every command


#####[volume section]

The boundaries set by 'min' and 'max' define the minimum and maximum volume levels. The volume level can not be increased past this level via e.g. MPD. However, if 'sync_2way' is 'TRUE', manually moving the volume level past the boundaries at the amplifier end will adjust the level boundaries accordingly. When moving back to the original boundaries set in the configuration file (e.g. via MPD or amplifier), so will the applied boundaries by synchronator. This prevents large volume jumps when manually pushing the level past boundaries set in synchronator.

discrete - specifies whether to use absolute (TRUE) or relative (FALSE) volume levels [TRUE`*`|FALSE]

- discrete volume levels use 'min' and 'max' to set volume level boundaries
- non-discrete volume levels use 'min' and 'plus' to specify command for adjusting relative volume levels

default -  default volume level when a 'deviceon' command is detected from a previous 'deviceoff' state. In such event, Synchronator will pauze for one second to allow to the target device to settle before sending any commands. The volume scale of the amplifier is used for this function and works only if Synchronator is in discrete mode or if it mimics this mode (see section below). In the latter case, a volume scale of 0-100 is used. If set, this function can also be triggered by sending the message 'reinit=volume' over via the message queue (see an example of this in the scripts folder). By default this function is not enabled.

header - prefix added to every volume command code (thus post the header defined in the main section)

curve - Volume curve can be linear or logarithmic. If volume control is relative, only linear is allowed [linear`*`|log]

###### additional non-discrete specific options

Synchronator can mimic discrete volume control for non-discrete controlable amplifiers. To activate this functionality, the option 'range' needs to be set. To sync Synchronator's volume level with that of the amplifier, volume level is set to zero at initialisation. Sample configuration files that have this functionality enabled have a suffix of MAVC (Mimic Absolute Volume Control).

range - number of steps to go from zero to maximum volume (may be higher than the actual value, not lower as that would render the initialisation process ineffective)

max - limits the maximum volume level to a value below that set in range [x < range|range`*`]

default - in addition to the description above, in this mode this function also triggers a volume reinitialisation.

timeout - sets a timeout in milliseconds between each volume command set to prevent dataloss due to an overload of data sent to the amplifier [0`*`]

double_zero_interval - Synchronator relies for this functionality on its ability to keep track of the current volume level. This process can be disrupted by user intervention at the amplifier end. By moving volume twice to zero within the set interval (in seconds),  Synchronator reinitialises its volume level. Setting this value to zero, enables initialization everytime volume level reaches zero [2`*`]

###### response volume sub-section

The following settings allow translation of the incoming volume range to the outgoing volume range (if different). Additional types can easily be created in C if necessary.

type - set type of translation [conditional_resize|range]

###### range settings

pre_offset - offset added pre multiplier

multiplier - multiplier applied on the incoming volume command (must be formatted as a floating point (e.g. 2.0 instead of 2)

post_offset - offset added post multiplier

invert_multiplier - for ease in use the multiplier can be inverted [TRUE`*`|False]


###### conditional_resize settings

limit - if a volume level is received above this limit, it will be divided by the diviser

type - type of limit [upper_limit|lower_limit]

diviser - conditional diviser if limit is crossed


##### [command_mod section]

Device specific mods can be created in C, variables can be set in this section

profile - checksum profile to be implemented [dynaudio|denon-avr]

###### dynaudio settings

This mod generates the status value and checksum bytes. In addition it processes the status byte, checking for zone and input.

zone - sets zone to control [1-3]

default_input - default input number [1-7]

###### denon-avr settings

This mod standardises incoming volume data, e.g. MV085 to MV08. Otherwise, Synchronator would not be able to distinguish between MV85 and MV085.

#####[query section]

Some amplifiers do not give automatic status updates. By setting a trigger and an appropriate interval (sec, 0 is only once at startup), a request for a status update can be issued through the interface port. This can also be used to set some default values at start of Synchronator (e.g. volume/input).


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

The header and tail in the main section are formatted as arrays delimited by square brackets

None of the numeric data variables are mandatory except for volume. If no other variables are defined (header, tail, etc), every incoming byte is assumed to be volume data. Every outgoing byte represents volume data

cc_position - If set, the command code is positioned at position x (starting at 1) in the header rather than after as per default. The original value in the header at that position will be overwritten.

### Glossary

command - complete command send to target device

command code - section that defines the (type of) instruction to device

parameter - parameter to command code

### Help

Take a look at the sample configuration files. If still not clear, post a message at Github.

---------------------------------------------------------------------
*) the * character indicates the default option/setting