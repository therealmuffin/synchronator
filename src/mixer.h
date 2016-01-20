#ifndef MIXER_H
	#define MIXER_H

	int initMixer(void);
    int getMixer(long *volume);
	int setMixer(int volume);
	void *watchMixer(void *arg);
	void deinitMixer(void);
#endif