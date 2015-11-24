#ifndef INTERFACES_H
	#define INTERFACES_H

	typedef struct {
		void (*help)(void);
		char *name;

		// initialises interfaces
		int (*init)(void);
		// deinitialises interfaces
		int (*deinit)(void);

		int (*send)(const void *buf, size_t count);
		void *(*listen)(void *arg);
	
	} interface_t;


	interface_t *getInterface(const char *name);


#endif