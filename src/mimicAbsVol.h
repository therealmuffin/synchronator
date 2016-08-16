#ifndef MIMICABSVOL_H
    #define MIMICABSVOL_H
    
    typedef struct {
        char *name;
        int (*init)(char *volMin, char *volPlus);
        void (*deinit)(void);
        int (*process)(double volumeExternal);
    } mimicAbsVol_t;

    extern mimicAbsVol_t mimicAbsVol;
#endif