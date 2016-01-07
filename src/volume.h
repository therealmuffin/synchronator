#ifndef VOLUME_H
	#define VOLUME_H
	
    typedef struct {
        void (*help)(void);
        char *name;
        void (*regenerateMultipliers)(void);
        void (*convertExternal2Mixer)(double *volume);
        void (*convertMixer2Internal)(long *volume);


    } volumeCurve_t;

	int initVolume(void);
	int processVolume(double *action_lookup);
	void deinitVolume();

#endif