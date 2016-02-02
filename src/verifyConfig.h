#ifndef VERIFY_CONFIG_H
    #define VERIFY_CONFIG_H

    #define VALCONF_NO_ARRAY -1
    #define VALCONF_LOG_FAILURE -1
    #define VALCONF_LOG_NOTICE -2

    #include <libconfig.h>

    int validateConfigString(   config_t *config, 
                                char *configSettingSource, 
                                const char **configSettingDestination, 
                                int position);      // -1 for no position (not an array)
                                
    int validateConfigInt(      config_t *config, 
                                char *configSettingSource, 
                                int *configSettingDestination, 
                                int position,       // -1 for no position (not an array)
                                int valueMin, 
                                int valueMax, 
                                int valueDefault);  // -1 setting is required, -2 setting 
                                                    // is ignored (both return EXIT_FAILURE)
                                
    int validateConfigDouble(   config_t *config, 
                                char *configSettingSource, 
                                double *configSettingDestination, 
                                int position,       // -1 for no position (not an array)
                                int valueMin, 
                                int valueMax, 
                                double valueDefault);  // -1 setting is required, -2 setting 
                                                    // is ignored (both return EXIT_FAILURE)
                                
    int validateConfigBool(     config_t *config, 
                                char *configSettingSource, 
                                int *configSettingDestination, 
                                int valueDefault);
                            
#endif