/****************************************************************************
 * Copyright (C) 2014 Muffinman
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 ****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h> // for O_CREATE etc...
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <syslog.h>
#include <libconfig.h>

#include "synchronator.h"
#include "common.h"
#include "verifyConfig.h"
#include "volume.h"
#include "interfaces.h"
#include "processData.h"
#ifndef DISABLE_MSQ
	#include "messageQueue.h"
#endif

/* This data has to be global so the can be accessed by the termination handler */
common_data_t common_data;
int lock_file = -1;
config_t config;
pthread_mutex_t lockProcess;
pthread_mutex_t lockConfig;

pthread_t mainThread;
pthread_t watchMixerThread;
pthread_t interfaceListenThread;
pthread_t statusQueryThread;

/* Prototypes of main functions */
int synchronatord(char *customConfigLocation);
int init(void);
int daemonize(void);
void *queryStatus(void *arg);
void terminate(int signum);

int main(int argc, char *argv[]) {   
    /* variables used on multiple locations are set at the top of the function */
    int status = 0;
    int daemon = 0;
    char *customConfigLocation = NULL;
    
    setlogmask(LOG_UPTO(LOG_NOTICE));
    
    int option = 0;
    opterr = 0;
    while((option = getopt(argc, argv, "v:Vhdi:")) != -1)
        switch(option) {
            case 'v':
                if(strcmp(optarg, "0") == 0)
                    setlogmask(LOG_UPTO(LOG_ERR));
                else if(strcmp(optarg, "1") == 0)
                    setlogmask(LOG_UPTO(LOG_INFO));
                else if(strcmp(optarg, "2") == 0)
                    setlogmask(LOG_UPTO(LOG_DEBUG));
                break;
            case 'V':
                fprintf(stdout, "%s %s\n\n%s\n", PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_LEGAL);
                exit(EXIT_SUCCESS);
            case 'h':
                fprintf(stdout, "Usage: %s %s", argv[0], TEXT_USAGE);
                exit(EXIT_SUCCESS);   
            case 'd':
                daemon = 1;
                break;
            case 'i':
                customConfigLocation = optarg;
                break;
            case '?':
                if(optopt == 'i')
                    fprintf(stderr, "You forgot to include a path with option '-%c'.\n", optopt);
                else if(optopt == 'v')
                    setlogmask(LOG_UPTO(LOG_INFO));
                else if(isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                exit(EXIT_FAILURE);
            default:
                exit(EXIT_FAILURE);
        }
    
    /* All files created without revoked permissions (thus result: 0666) */
    umask(0);
    
    if(daemon) {
        if((status = daemonize()) == EXIT_FAILURE)
            exit(EXIT_FAILURE);
    }
    else {
        /* Setup syslog, print also to stderr, and give notice of execution */
        openlog(PROGRAM_NAME, LOG_NDELAY | LOG_PID | LOG_PERROR, LOG_DAEMON);
        syslog(LOG_NOTICE, "Program started by User %d", getuid());
    }
        
    /* Lock process and print pid to lock file */
    if((lock_file = open(LOCKFILE, O_RDWR|O_CREAT|O_CLOEXEC, LOCKMODE)) < 0) {
        syslog(LOG_ERR, "Failed to open lock file: %s", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }
    if(lockf(lock_file, F_TLOCK, 0) < 0) {
        syslog(LOG_WARNING, "Exiting: only one instance of this application can run: %s", strerror(errno));
        closelog();
        exit(EXIT_SUCCESS);
    }
    ftruncate(lock_file, 0);
    dprintf(lock_file, "%d\n", getpid());
    
    /* Get main thread id and make it the default value for all other threads.
     * If a thread has been initialized, it changes the default value and the
     * termination handler knows whether to cancel it or not. */
    mainThread = statusQueryThread = watchMixerThread = interfaceListenThread = pthread_self();
	common_data.process = NULL;
	common_data.interface = NULL;
    common_data.volume = NULL;
    
    struct sigaction signal_action;
    signal_action.sa_handler = terminate;
    sigfillset(&signal_action.sa_mask);
    signal_action.sa_flags = 0;
    if(sigaction(SIGINT, &signal_action, NULL) < 0 || sigaction(SIGTERM, &signal_action, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore signals: %s", strerror(errno));
		closelog();
		exit(EXIT_FAILURE);
    }
    
    /* Execute synchronatord function, if all goes well, it will never return */
    status = synchronatord(customConfigLocation);
        
    syslog(LOG_ERR, "Exiting: %i", status);
    closelog();
    exit(status);
} /* end main */

int daemonize(void) {
    int count;
    pid_t synchronatord_pid;
    
    /* Get maximum number of file descriptors, close them, create fd to /dev/null,
     * and duplicate it twice */
    struct rlimit resource_limit;
    int fd0, fd1, fd2;
    if(getrlimit(RLIMIT_NOFILE, &resource_limit) < 0)
        fprintf(stderr, "Unable to retrieve resource limit: %s\n", strerror(errno));
    if(resource_limit.rlim_max == RLIM_INFINITY)
        resource_limit.rlim_max = 1024;
    for(count = 0; count < resource_limit.rlim_max; count++)
        close(count);
    fd0 = open("/dev/null", O_RDWR);
    fd1 = fd2 = dup(0);
    
	/* Setup syslog and give notice of execution */
    openlog(PROGRAM_NAME, LOG_NDELAY | LOG_PID, LOG_DAEMON);
    syslog(LOG_NOTICE, "Program started by User %d", getuid());
    
    /* Detach from tty and make it session leader */
    if((synchronatord_pid = fork()) < 0) {
        syslog(LOG_ERR, "Failed to detach from TTY: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    else if(synchronatord_pid > 0) {
        closelog();
        exit(EXIT_SUCCESS);   
    }
    
    /* Create new session and make current process its leader */
    setsid();
    
    struct sigaction signal_action;
    signal_action.sa_handler = SIG_IGN;
    sigemptyset(&signal_action.sa_mask);
    signal_action.sa_flags = 0;
    if(sigaction(SIGHUP, &signal_action, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore SIGHUP: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    /* Second forking: attach process to init */
    if((synchronatord_pid = fork()) < 0) {
        syslog(LOG_ERR, "Failed to attach to init: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }
    else if(synchronatord_pid > 0) {
        closelog();
        exit(EXIT_SUCCESS);
    }
    
    return EXIT_SUCCESS;
} /* end daemonize */

/* initializes the application and executes the pthreads */
int synchronatord(char *customConfigLocation) {
    int status = 0;
    int count = 0;
    
    /* List of config locations */
    char *configLocations[] = {customConfigLocation, CONFIG_LOCATION1, CONFIG_LOCATION2};
    /* If a path was given as option than the loop will start at 0 otherwise 1 */
    for(count = (configLocations[0] == NULL) ? 1:0; count < 3; count++) {
        config_init(&config);
        
        if(!config_read_file(&config, configLocations[count])) {
            if(count == 2 || count == 0) {
                syslog(LOG_WARNING, "Configuration file was not found or is invalid: %s\n%s:%d - %s", 
                    configLocations[count], config_error_file(&config), config_error_line(&config), 
                    config_error_text(&config));
				raise(SIGTERM);
            }
        }
        else {
            syslog(LOG_NOTICE, "Configuration file has been read: %s", configLocations[0]);
            break;
        }
    }

    /* initialise variables, read configuration and set proper interface and processing methods */
    if((status = init()) == EXIT_FAILURE) {
        syslog(LOG_ERR, "Initialisation failed: %i", status); 
        raise(SIGTERM);
    }
    
    /* Set signals to be blocked by threads */
    sigset_t thread_sigmask;
    sigemptyset(&thread_sigmask);
    sigaddset(&thread_sigmask, SIGINT);
    sigaddset(&thread_sigmask, SIGTERM);
    
    if((status = pthread_mutex_init(&lockProcess, NULL)) != 0) {
        syslog(LOG_ERR, "Failed to create process mutex: %i", status);
        raise(SIGTERM);
    }
    pthread_attr_t thread_attr;
    if((status = pthread_attr_init(&thread_attr)) != 0) {
        syslog(LOG_ERR, "Failed to initialize thread attributes: %i", status);
        raise(SIGTERM);
    }
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    if(status = pthread_create(&watchMixerThread, &thread_attr, watchMixer, &thread_sigmask)) {
        pthread_attr_destroy(&thread_attr);
        syslog(LOG_ERR, "Failed to open mixer thread: %i", status);
        raise(SIGTERM);
    }
    
    if(common_data.sync_2way && (status = pthread_create(&interfaceListenThread, &thread_attr, 
    								common_data.interface->listen, &thread_sigmask))) {
        pthread_attr_destroy(&thread_attr);
        syslog(LOG_ERR, "Failed to open interface listen thread: %i", status);
        raise(SIGTERM);
    }
    
    if(common_data.send_query && (status = pthread_create(&statusQueryThread, &thread_attr, 
    queryStatus, &thread_sigmask))) {
        pthread_attr_destroy(&thread_attr);
        syslog(LOG_ERR, "Failed to open interface query thread: %i", status);
        raise(SIGTERM);
    }
    
    /* Has served its purpose and can be destroyed */
    pthread_attr_destroy(&thread_attr);
    
    #ifndef DISABLE_MSQ
		if(status = watchMsQ()) {
			syslog(LOG_ERR, "Message queue failure: %i", status);
			raise(SIGTERM);
		}
    #else
		pause();
    #endif
    
    /* Shouldn't have gotten here */
    raise(SIGTERM);
    return EXIT_FAILURE;
} /* end synchronatord */

/* Will validate the data retrieved from the configuration file */
int init(void) {
    int status;
    config_setting_t *conf_setting = NULL;
	const char *conf_value = NULL;
    
	/* Now the configuration file is read, we don't need to know the location
	 * of the application anymore. We'll change working directory to root */
	if(chdir("/") < 0) {
        syslog(LOG_ERR, "Unable to change working directory to root: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    syslog(LOG_INFO, "Checking presence and validity of required variables:");
    
    validateConfigBool(&config, "sync_2way", &common_data.sync_2way, 0);
    
    validateConfigBool(&config, "diff_commands", &common_data.diff_commands, 0);
    
    common_data.send_query = config_lookup(&config, "query") ? 1:0;
    if(common_data.send_query && ((conf_setting = config_lookup(&config, "query.trigger")) == NULL || 
    config_setting_is_array(conf_setting) == CONFIG_FALSE ||  (common_data.statusQueryLength = 
    config_setting_length(conf_setting)) == 0)) {
        syslog(LOG_ERR, "[ERROR] Setting is not present or not formatted as array: query.trigger");
        return EXIT_FAILURE;
    }
    if(common_data.send_query && validateConfigInt(&config, "query.interval", 
    &common_data.statusQueryInterval, -1, 0, 255, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    /* Set volume (curve) functions */
	conf_value = NULL;
	config_lookup_string(&config, "volume.curve", &conf_value);
    if(!(common_data.volume = getVolume(&conf_value))) {
    	syslog(LOG_ERR, "[Error] Volume curve is not recognised: %s", conf_value);
        return EXIT_FAILURE;
    }
    else {
    	syslog(LOG_INFO, "[OK] volume.curve: %s", conf_value);
    }
    /* initialise volume variables */
    if(common_data.volume->init() == EXIT_FAILURE)
    	return EXIT_FAILURE;
    
    /* Set and initialise process type */
	conf_value = NULL;
	config_lookup_string(&config, "data_type", &conf_value);
    if(!(common_data.process = getProcessMethod(conf_value))) {
    	syslog(LOG_ERR, "[Error] Setting 'data_type' is not recognised: %s", conf_value);
        return EXIT_FAILURE;
    }
    else {
    	syslog(LOG_INFO, "[OK] data_type: %s", conf_value);
    }
    if(common_data.process->init() == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    /* Set and initialise communication interface */
    conf_value = NULL;
	config_lookup_string(&config, "interface", &conf_value);
    if(!(common_data.interface = getInterface(conf_value))) {
    	syslog(LOG_ERR, "[Error] Setting 'interface' is not recognised: %s", conf_value);
        return EXIT_FAILURE;
    }
    else {
    	syslog(LOG_INFO, "[OK] interface: %s", conf_value);
    }
    if(common_data.interface->init() == EXIT_FAILURE)
        return EXIT_FAILURE;
        
    /* initialise mixer device */
    if(initMixer() == EXIT_FAILURE)
    	return EXIT_FAILURE;

	/* init multipliers */
	common_data.volume->regenerateMultipliers();

#ifndef DISABLE_MSQ
	/* initialise message queue */
	if(initMsQ() == EXIT_FAILURE)
		return EXIT_FAILURE;
#endif
    
    if((status = pthread_mutex_init(&lockConfig, NULL)) != 0) {
        syslog(LOG_ERR, "Failed to create config mutex: %i", status);
    	return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
} /* end init */

void *queryStatus(void *arg) {
    sigset_t *thread_sigmask = (sigset_t *)arg;
    if(pthread_sigmask(SIG_BLOCK, thread_sigmask, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore signals in statusQuery: %i", errno);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    while(1) {
        if(common_data.interface->send(common_data.statusQuery, common_data.statusQueryLength) == -1) {
            syslog(LOG_ERR, "Failed to write trigger to serial port");
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
    
        syslog(LOG_DEBUG, "Trigger written to serial port");
        if(common_data.statusQueryInterval == 0)
            break;
        sleep(common_data.statusQueryInterval);
    }
    
    pthread_exit(EXIT_SUCCESS);
} /* queryStatus */

/* Handles the termination process and frees if required */
void terminate(int signum) {	
    /* Check if pthread_t has been initialized from default value and is still alive,
     * if so, cancel and join it. */
    if(!pthread_equal(watchMixerThread, mainThread) && pthread_kill(watchMixerThread, 0) == 0) {
        if(pthread_cancel(watchMixerThread) != 0)
            syslog(LOG_WARNING, "Unable request cancelation for mixer thread: %s", strerror(errno));
        if((errno = pthread_join(watchMixerThread, NULL)) != 0)
            syslog(LOG_WARNING, "Unable to join mixer thread: %s\n", strerror(errno));
    }
    if(!pthread_equal(interfaceListenThread, mainThread) && pthread_kill(interfaceListenThread, 0) == 0) {
        if(pthread_cancel(interfaceListenThread) != 0)
            syslog(LOG_WARNING, "Unable request cancelation for serial thread: %s", strerror(errno));
        if((errno = pthread_join(interfaceListenThread, NULL)) != 0)
            syslog(LOG_WARNING, "Unable to join serial thread: %s\n", strerror(errno));
    }
    if(!pthread_equal(statusQueryThread, mainThread) && pthread_kill(statusQueryThread, 0) == 0) {
        if(pthread_cancel(statusQueryThread) != 0)
            syslog(LOG_WARNING, "Unable request cancelation for query thread: %s", strerror(errno));
        if((errno = pthread_join(statusQueryThread, NULL)) != 0)
            syslog(LOG_WARNING, "Unable to join query thread: %s\n", strerror(errno));
    }
    
    if(common_data.interface)
		common_data.interface->deinit();
	if(common_data.process)
		common_data.process->deinit();
	
	deinitMixer();
	if(common_data.volume)
		common_data.volume->deinit();
	statusInfo.deinit();
    config_destroy(&config);
	
    pthread_mutex_destroy(&lockConfig);
    pthread_mutex_destroy(&lockProcess);
    
#ifndef DISABLE_MSQ
    deinitMsQ();
#endif
	
	if(lock_file > 0) {
		lockf(lock_file, F_ULOCK, 0);
		close(lock_file);
		unlink(LOCKFILE);
	}
    
    syslog(LOG_NOTICE, "Program terminated: %s (%i)", strsignal(signum), signum);
    closelog();
	exit(EXIT_SUCCESS);
} /* end terminate */