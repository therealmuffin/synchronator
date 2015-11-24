#ifndef PROCESS_DATA_H

	#define PROCESS_DATA_H

	typedef struct {
		void (*help)(void);
		char *name;
		int (*init)(void);
		int (*deinit)(void);
		int (*compileCommand)(char *header, void *command);
		int (*strip_raw_input)(unsigned char *device_status_message, ssize_t bytes_read);
	
	} process_method_t;


	process_method_t *getProcessMethod(const char *name);


#endif