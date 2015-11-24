<?
/****************************************************************************
 * Copyright (C) 2014 Muffinman
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
 
/* Set verbose to false by default. */
if(!isset($_REQUEST['verbose']))
{
    error_reporting(0);
    ini_set('display_errors', 'Off');
}

/* Loads required configuration. */
if((include 'synchronator.config.php') != 1)
{
    echo 'Failed to load configuration: terminating program.';
    return -1;
}

/* Check for verbose setting in configuration file. */
if(isset($verbose) && $verbose)
{
    error_reporting(-1);
    ini_set('display_errors', 'On');
}

/* basic configuration verification */
if(!isset($deviceCommandList) || !isset($tempLocation) || !isset($applicationServer) || 
    !isset($deviceCommandStart) || !isset($deviceCommandStop))
{
    echo 'Failed to verify configuration.';
    return -1;
}

/* These variables are provided MPOD/MPAD when switching zones.
/* If it requests current status they are not. Then the script
/* will pass on to output the XML with that info. */
(isset($_REQUEST['enabled']) ? $deviceEnabled = $_REQUEST['enabled'] : $deviceEnabled = null);
(isset($_REQUEST['name']) ? $deviceCommand = $_REQUEST['name'] : $deviceCommand = null);
(isset($_REQUEST['start']) ? $actionStart = $_REQUEST['start'] : $actionStart = null);
(isset($_REQUEST['stop']) ? $actionStop = $_REQUEST['stop'] : $actionStop = null);

$deviceCommand = rawurldecode($deviceCommand);

/* Define start and stop commands and add them to the list */
if($actionStart)
{
    $deviceCommand = 'START';
    $deviceCommandList['START'] = $deviceCommandStart;
}
if($actionStop)
{
    $deviceCommand = 'STOP';
    $deviceCommandList['STOP'] = $deviceCommandStop;
}

/* Check if input is valid. If not, set input to null and output the xml */
if(!isset($deviceCommandList[$deviceCommand])) $deviceCommand = null;
/* If input and command is set create message and sends it to queue */
if($deviceCommand)
{
    /* This list will be send to the message queue: */
    $syncList = array();
    $setVolume = 0;
    /* Create entry for input and set boolean to true if volume must be set */
    $syncList[$deviceCommandList[$deviceCommand][1]] = $deviceCommandList[$deviceCommand][0];
    if(is_int($deviceCommandList[$deviceCommand][2]))  $setVolume = 1;
    
    /* if array is longer than 3, there are some options selected, id them, create message, and add them to array */
    if($sendTotalCommands = count($deviceCommandList[$deviceCommand]) > 3)
    {
        foreach($deviceCommandList[$deviceCommand] as $index => $currentCommand)
        {
            if($index < 3) continue;
            if(isset($deviceOptionList[$currentCommand]))
                $syncList[$deviceOptionList[$currentCommand]] = $deviceCommandList[$deviceCommand][$index];
        }
        unset($index,$currentCommand);
    }
    
    $syncList = array_reverse($syncList);

    /* Establish connection to message queue and send syncList */
    if(($msqKey = ftok("$tempLocation/".$applicationServer.".msq" , 'B')) == -1)
    {
        echo 'Location does not exist, could not create token: terminating program.';
        return(-1);
    }
    
    if(!msg_queue_exists($msqKey))
    {
        echo 'Message queue does not exist: terminating program.';
        exit(-1);
    }
    $msqId = msg_get_queue($msqKey, 0666);
    
    foreach($syncList as $currentCategory => $currentCommand)
    {
        $msqMessage = "$currentCategory=$currentCommand\0";
        if(!msg_send($msqId, 1, $msqMessage, false, true, $msqErr)) 
            echo "Failed to send message to queue: $msqErr\n";
    }
    unset($currentCategory, $currentCommand);
    
    if($setVolume) exec('sudo amixer -c Dummy sset Master '.$deviceCommandList[$deviceCommand][2].'% &');
    
    /* Sets a delay and gives the amplifier to process changes and this script to return
     * the new and correct value. */
    usleep($return_status_delay*1000);
    
    exit;   
}

/* Default: Outputs the XML for mPod/mPad to id the options. */
reset($deviceCommandList);

/* Make a list of all command categories*/
$deviceState = array();
while($settingOptions = each($deviceCommandList))
{
    $deviceState[$settingOptions[1][1]] = '';
}

/* Check if a state file exists for each of the command categories and fill array */
foreach($deviceState as $examineStateKey => &$examineStateValue)
{
    if(!$currentState = @file_get_contents($tempLocation.'/'.$applicationServer.'.'.$examineStateKey))
    {
        unset($deviceState[$examineStateKey]);
        continue;
    }
    $examineStateValue = $currentState;
}
unset($examineStateKey, $examineStateValue);

reset($deviceCommandList);

header('Content-Type: text/xml');
echo 
'<?xml version="1.0" encoding="UTF-8"?>
<speakers>';

/* Check for each of the state files whether they correspond with available options */
while($settingOptions = each($deviceCommandList))
{
    $stateEnabled = 0;
    if(array_key_exists($settingOptions[1][1], $deviceState) && $deviceState[$settingOptions[1][1]] == $settingOptions[1][0])
        $stateEnabled = 1;

    echo '
    <speaker name="'.$settingOptions[0].'" enabled="'.$stateEnabled.'"/>';
}
    
echo '
</speakers>';
?>