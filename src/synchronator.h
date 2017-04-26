#ifndef SYNCHRONATOR_H
    #define SYNCHRONATOR_H
    
    #include <unistd.h>

    #ifndef NULL
        #define NULL (void *)0
    #endif // #ifndef NULL
    
    #ifdef INCLUDE_CONFIG
        #include "config.h"
    #endif // #ifdef INCLUDE_CONFIG
    
    #ifndef INSTALL_LOCATION
        #define INSTALL_LOCATION = "/usr/local"
    #endif // #ifndef INSTALL_LOCATION
    
    #ifdef INCLUDE_VERSION
        #include "version.h"
    #endif // #ifdef INCLUDE_VERSION
    
    #ifndef DISABLE_SERIAL
        #define USAGE_SERIAL " Serial interface\n"
    #else
        #define USAGE_SERIAL
    #endif // #ifdef USAGE_SERIAL
    
    #ifdef ENABLE_I2C
        #define USAGE_I2C " I2C interface\n"
    #else
        #define USAGE_I2C
    #endif // #ifdef ENABLE_I2C
    
    #ifdef ENABLE_TCP
        #define USAGE_TCP " TCP interface\n"
    #else
        #define USAGE_TCP
    #endif // #ifdef ENABLE_TCP
    
    #ifdef ENABLE_LIRC
        #define USAGE_LIRC " LIRC interface (experimental)\n"
    #else
        #define USAGE_LIRC
    #endif // #ifdef ENABLE_LIRC
    
    #ifndef DISABLE_MSQ
        #define USAGE_MSQ " Message Queue (basic local control)\n"
    #else
        #define USAGE_MSQ
    #endif // #ifdef DISABLE_MSQ
    
    #ifdef _POSIX_MONOTONIC_CLOCK
        #define TIME_DEFINED_TIMEOUT
    #endif

    #ifndef PROGRAM_PREFIX
        #define PROGRAM_PREFIX
    #endif // #ifndef PROGRAM_PREFIX

    #ifndef PROGRAM_SUFFIX
        #define PROGRAM_SUFFIX
    #endif // #ifndef PROGRAM_SUFFIX
    
    #define PROGRAM_NAME PROGRAM_PREFIX "synchronator" PROGRAM_SUFFIX
    #ifndef PROGRAM_VERSION
        #define PROGRAM_VERSION "UNKOWN VERSION"
    #endif // #ifndef PROGRAM_VERSION
    #define PROGRAM_LEGAL \
    "Copyright (C) 2013-2016 Muffinman\n" \
    "This is free software; see the source for copying conditions. There is NO\n" \
    "warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"

    #define CONFIG_LOCATION1 INSTALL_LOCATION "/etc/" PROGRAM_NAME ".conf"
    #define CONFIG_LOCATION2 "/etc/" PROGRAM_NAME ".conf"

    #define TEXT_USAGE \
    "[options...]\n" \
    "   -V          print version information\n" \
    "   -h          print this help message\n" \
    "   -d          daemonize application\n" \
    "   -v0         set level of verbose to a minimum\n" \
    "   -v1         set level of verbose to include informational messages\n" \
    "   -v2         set level of verbose to include debug messages\n" \
    "   -i          specify path to a configuration file\n\n" \
    "If no location for the configuration file is given, the application will look in:\n" \
    "   primary     "CONFIG_LOCATION1"\n" \
    "   secondary   "CONFIG_LOCATION2"\n" \
    "\n" \
    "Synchronator is compiled with support for:\n" USAGE_SERIAL USAGE_I2C USAGE_TCP USAGE_LIRC USAGE_MSQ

    #define CONFIG_QUERY_SIZE 100
    #define SERIAL_READ_BUFFER 100

    /* msg variables */
    #define FTOK_PATH "/tmp/"PROGRAM_NAME".msq"
    #define FTOK_ID 'B'
    #define MSQ_SIZE CONFIG_QUERY_SIZE/2

    #define LOCKFILE "/var/run/synchronator/"PROGRAM_NAME".pid"
    #define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

    /* Location for setting status files */
    #define TEMPLOCATION "/tmp"
    #define MAX_PATH_LENGTH 50

    #ifdef TIME_DEFINED_TIMEOUT
    /* Ignore incoming commands within seconds of outgoing command and vice versa to prevent a loop */
        #define DEFAULT_PROCESS_TIMEOUT_IN 2
        #define DEFAULT_PROCESS_TIMEOUT_OUT 2
    #else
    /* Ignore X incoming commands after outgoing command and vice versa to prevent a loop */
        #define DEFAULT_PROCESS_TIMEOUT_IN 20
        #define DEFAULT_PROCESS_TIMEOUT_OUT 20
    #endif
    
    #ifdef CUSTOM_PROCESS_TIMEOUT_RX
        #undef DEFAULT_PROCESS_TIMEOUT_IN
        #define DEFAULT_PROCESS_TIMEOUT_IN CUSTOM_PROCESS_TIMEOUT_RX
    #endif // #ifdef CUSTOM_PROCESS_TIMEOUT_RX

    #ifdef CUSTOM_PROCESS_TIMEOUT_TX
        #undef DEFAULT_PROCESS_TIMEOUT_OUT
        #define DEFAULT_PROCESS_TIMEOUT_OUT CUSTOM_PROCESS_TIMEOUT_TX
    #endif // #ifdef CUSTOM_PROCESS_TIMEOUT_TX

#endif