#ifndef PROCESS_DATA_H

    #define PROCESS_DATA_H

    typedef struct {
        void (*help)(void);
        char *name;
        int (*init)(void);
        void (*deinit)(void);
        int (*compileVolumeCommand)(long *volumeLevel);
        int (*compileDeviceCommand)(char *header, char *action);
        int (*strip_raw_input)(unsigned char *device_status_message, ssize_t bytes_read);
    
    } process_method_t;
    
    process_method_t *getProcessMethod(const char **name);
    
    /* Status info functions */

    typedef struct {
        int (*update)(const char *name, const char *value);
        void (*retrieve)(const char *name, const char **value);
        void (*deinit)(void);
    
    } statusInfo_t;
    
    statusInfo_t statusInfo;


#endif