#ifndef COMMON_H
    #define COMMON_H

    #include <time.h>
    #include <pthread.h>
    #include <libconfig.h>
    #include "synchronator.h"
    #include "verifyConfig.h"
    #include "interfaces.h"
    #include "processData.h"
    #include "mixer.h"
    #include "volume.h"
    #include "mods.h"

    /* Variables are set while validating/processing config, some are useful shortcuts */
    typedef struct {
    
        /* Options */
        int sync_2way, discrete_volume, diff_commands, send_query;
        const char *dataType;
        
        /* Dynamic device mixer variables + backup of original */
        int volume_min, volume_max, initial_volume_min, initial_volume_max;
        /* Alsa mixer variables */
        long alsa_volume_min, alsa_volume_max, alsa_volume_range;
        
        /* Status variable, external volume volume level if (mimicked) discrete, if relative internal 
            volume level */
        double volume_level_status;
        
        /* If different from 0, smoothVolume is activated */
        int volume_timeout;
        
        /* Multiplies non-discrete volume commands (not/partially implemented) */
        int nd_vol_multiplier;
        
        /* Ignore X incoming commands after outgoing command and vice versa to prevent a loop */
        
#ifdef TIME_DEFINED_TIMEOUT
        struct timespec timestampLastRX;
        struct timespec timestampLastTX;
#else
        int volume_out_timeout;
        int volume_in_timeout;
#endif
        int statusQueryLength, statusQueryInterval;
        const char *statusQuery;
    
        process_method_t *process;
        interface_t *interface;
        volume_functions_t *volume;
        mods_t *mod;
        int (*reinitVolume)(void);
        int defaultExternalVolume;
    } common_data_t;

    extern common_data_t common_data;
    extern config_t config;
    extern pthread_mutex_t lockProcess;
    extern pthread_mutex_t lockConfig;
    extern pthread_t mainThread;
    
    #define VOLUME_UP 20
    #define VOLUME_DOWN 10
    
    
    #define CONFIG_REQUIRED -1
    #define CONFIG_IGNORE -2

#endif