<?
/****************************************************************************
 * Copyright (C) 2013 Muffinman
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 ****************************************************************************/
 
################################### Options ###################################
###############################################################################

/***************************** deviceCommandList ***************************************
/***************************************************************************************
 * Device commands are accessible through http by providing the name parameter through
 * http. They will show up in the XML list created when accessing synchronator.php without 
 * (valid) parameters This is conforms to the requirements of MPD clients MPoD and MPaD:
 * e.g. http://mpd-box.local/synchronator.php?name=command_title
 * 
 * The entries in the $deviceCommandList array are structured as follows:
 * command title => command value, command category, default volume (, deviceOption, ...) 
 * 
 * command title - Some arbitrary name for this specific command/macro
 * command value - the value of the command as set in synchronator.conf
 * command category - the category of the command as set in synchronator.conf
 * default volume - if desired a specific default volume level (0-100) can be set when 
 *      selecting this command/macro. To leave volume untouched just set it to null
 * deviceOption - one or more device options can be triggered when selecting a command. 
 *      The options are set in $deviceOptionList, there is not limit    
 ***************************************************************************************/
$deviceCommandList = 
    array(
        'MPD' => array('usb', 'input', 35, 'deviceon'),
        'Auxiliary' => array('aux', 'input', null, 'deviceon'),
        'Coax-1' => array('coax1', 'input', null, 'deviceon'),
        'Coax-2' => array('coax2', 'input', null, 'deviceon'),
        'Coax-3' => array('coax3', 'input', null, 'deviceon'),
        'Optical-1' => array('opt1', 'input', null, 'deviceon'),
        'Optical-2' => array('opt2', 'input', null, 'deviceon'),
        'Optical-3' => array('opt3', 'input', null, 'deviceon'),
        'Device-Off' => array('deviceoff', 'power', null),
        'Device-On' => array('deviceon', 'power', null)
    );

/***************************** deviceOptionList ****************************************
/***************************************************************************************
 * Device options are not accessable through http and will not show up in the XML list. 
 * They can be added at the end of a command in the command list above.
 *
 * The entries in the $deviceOptionList array are structured as follows:
 * command value => command category
 *
 * command value - the value of the command as set in synchronator.conf
 * command category - the category of the command as set in synchronator.conf
 ***************************************************************************************/ 
$deviceOptionList = 
    array(
        'deviceon' => 'power',
        'deviceoff' => 'power',
        'ledon' => 'device',
        'ledoff' => 'device'
    );

/************************** Miscellaneous commands *************************************
/***************************************************************************************
 * The following commands are accessible through http by adding a start or stop parameter 
 * through http. They will not show up in the XML list. These commands are automatically 
 * triggered by MPD clients MPoD and MPaD:
 * e.g. http://mpd-box.local/synchronator.php?start=
 *
 * Save for the arbitrary command title, this array is structured identical to Device 
 * commands 
 ***************************************************************************************/
$deviceCommandStart =  array('usb', 'input', null, 'deviceon');
$deviceCommandStop = array('deviceoff', 'power', null);

################################## Advanced ##################################

$tempLocation = "/tmp";
$applicationServer = "synchronator";
/* Sets a delay after each set of send commands (measured in milliseconds).
 * This gives the amplifier time to process the change and for Synchronator to
 * pick it up. */
$return_status_delay = 0;
$query_status = 0; // unused for now
$verbose = 1;

################################# End Options #################################
###############################################################################
?>