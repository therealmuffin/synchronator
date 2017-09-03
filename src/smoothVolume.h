#ifndef SMOOTHVOLUME_H
    #define SMOOTHVOLUME_H
    
    typedef struct {
        char *name;
        int (*init)(int (*volumeDown)(double *externalVolume), char *volMin, char *volPlus);
        void (*deinit)(void);
        int (*process)(double *volumeExternal);
    } smoothVolume_t;

    extern smoothVolume_t smoothVolume;
#endif