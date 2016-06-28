############## BEGIN CONFIGURATION ##############

command_code="power"
command_parameter="deviceon"

############### END CONFIGURATION ###############

import sysv_ipc

msq_key=sysv_ipc.ftok('/tmp/synchronator.msq', 66, silence_warning = True)
msq  = sysv_ipc.MessageQueue(msq_key)
msq_command=command_code + "=" + command_parameter + "\0"
msq.send(msq_command)