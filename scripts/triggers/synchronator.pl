#!/usr/bin/perl
############## BEGIN CONFIGURATION ##############

$command_code="power";
$command_parameter="deviceon";

############### END CONFIGURATION ###############

use IPC::SysV qw(S_IRUSR);
use IPC::SysV qw(ftok);

$msq_key = ftok("/tmp/synchronator.msq", 'B');
$msq = msgget($msq_key, S_IRUSR);
defined($msq) || die "msgget failed: $!";
$msq_command = $command_code . '=' . $command_parameter . "\0";
$type_sent = 1;
msgsnd($msq, pack("l! a*", $type_sent, $msq_command), 0) || die "msgsnd failed: $!";

exit;