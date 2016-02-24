#ifndef MODS_H
    #define MODS_H
    
    typedef struct {
        char *name;
        void (*help)(void);
        int (*init)(void);
        void (*process)(double *volume_level);
        void (*deinit)(void);
    
    } modVolumeResponse_t;
    
    typedef struct {
        char *name;
        void (*help)(void);
        int (*init)(int x, ...);
        void (*produce)(const void *message);
        int (*process)(void *command, int command_length);
        void (*deinit)(void);
    
    } modCommand_t;
    
    typedef struct {
        int (*init)(void);
        modVolumeResponse_t *volumeResponse;
        modCommand_t *command;
        void (*deinit)(void);
    
    } mods_t;

    mods_t mod;

#endif