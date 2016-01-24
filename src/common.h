#ifndef COMMON_H
    #define COMMON_H

    #include <pthread.h>
    #include <libconfig.h>
    #include "interfaces.h"
    #include "processData.h"
    #include "mixer.h"
    #include "volume.h"

    /* Variables are set while validating/processing config, some are useful shortcuts */
    typedef struct {
    
        /* Options */
        int sync_2way, discrete_volume, diff_commands, send_query;
    
        /* Dynamic device mixer variables + backup of original */
        int volume_min, volume_max, initial_volume_min, initial_volume_max;
        /* Alsa mixer variables */
        long alsa_volume_min, alsa_volume_max, alsa_volume_range;
        
        /* Status variable, external volume volume level if discrete, if relative internal 
            volume level */
        double volume_level_status;
    
        /* Multiplies non-discrete volume commands (not/partially implemented) */
        int nd_vol_multiplier;
        
        /* Amp response offsets and multiplier if applicable */
        int responsePreOffset, responsePostOffset, responseDivergent;
        double responseMultiplier;
    
        /* Ignore X incoming commands after outgoing command and vice versa to prevent a loop */
        int volume_out_timeout;
        int volume_in_timeout;
    
        int statusQueryLength, statusQueryInterval;
        const char *statusQuery;
    
        process_method_t *process;
        interface_t *interface;
        volume_functions_t *volume;
    } common_data_t;

    extern common_data_t common_data;
    extern config_t config;
    extern pthread_mutex_t lockProcess;
    extern pthread_mutex_t lockConfig;
    extern pthread_t mainThread;

#endif