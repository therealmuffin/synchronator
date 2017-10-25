#ifndef SMOOTHVOLUME_H
    #define SMOOTHVOLUME_H
    
    typedef struct {
        char *name;
        int (*init)(int (*setDescreteVolume)(double *internalVolume), char *volMin, char *volPlus);
        void (*deinit)(void);
        int (*process)(long *volumeExternal);
    } smoothVolume_t;

    extern smoothVolume_t smoothVolume;
#endif