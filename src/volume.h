#ifndef VOLUME_H
    #define VOLUME_H
    
    typedef struct {
        void (*help)(void);
        char *name;
        void (*regenerateMultipliers)(void);
        void (*convertExternal2Mixer)(double *volume);
        void (*convertMixer2Internal)(long *volume);
        void (*convertInternal2External)(long *volumeInternal, double *volumeExternal);
        void (*convertExternal2Internal)(double *volumeExternal, long *volumeInternal);


    } volumeCurve_t;

    typedef struct {
        int (*init)(void);
        int (*process)(double *volumeExternal);
        void (*deinit)(void);
        
        char *curve;
        void (*regenerateMultipliers)(void);
        void (*convertExternal2Mixer)(double *volume);
        void (*convertMixer2Internal)(long *volume);
        void (*convertInternal2External)(long *volumeInternal, double *volumeExternal);
        void (*convertExternal2Internal)(double *volumeExternal, long *volumeInternal);
    
    } volume_functions_t;
    
    volume_functions_t *getVolume(const char **name);

#endif