#ifndef MIXER_H
	#define MIXER_H

	int initMixer(void);
	int setMixer(int volume);
	void *watchMixer(void *arg);
	void deinitMixer(void);

#endif