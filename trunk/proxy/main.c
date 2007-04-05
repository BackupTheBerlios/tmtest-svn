/* main.c
 * 8 Mar 2007
 * Scott Bronson
 * 
 * The main routine for the lb1 proxy.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <values.h>
#include <signal.h>


#include "log.h"



enum {
	internal_error = 1,
	argument_error = 2,
	runtime_error = 3,
};


int done = 0;
io_poller poller;


#define super_stringify(x) #x
#define stringify(x) super_stringify(x)


void usage()
{
	printf(
			"Usage: proxy ARGS (duh...)\n"
		  );
}


void process_args(int argc, char **argv)
{
	char optstring[256], *cp;
	int optidx = 0, i, c;

	enum {
		LOG_LEVEL = CHAR_MAX + 1,
		LOG_FILE,
	};

	static struct option longopts[] = {
		// name, has_arg (1=reqd,2=opt), flag, val
		{"loglevel", 1, 0, LOG_LEVEL},
		{"log-level", 1, 0, LOG_LEVEL},
		{"logfile", 1, 0, LOG_FILE},
		{"log-file", 1, 0, LOG_FILE},
		{"config", 1, 0, 'c'},
		{"verbose", 1, 0, 'v'},
		{0, 0, 0, 0}
	};

	// dynamically create the option string from the long
	// options.  Why oh why doesn't glibc do this for us???
	cp = optstring;
	for(i=0; longopts[i].name; i++) {
		*cp++ = longopts[i].val;
		if(longopts[i].has_arg > 0) *cp++ = ':';
		if(longopts[i].has_arg > 1) *cp++ = ':';
	}
	*cp++ = '\0';

	while(1) {
		c = getopt_long(argc, argv, optstring, longopts, &optidx);
		if(c == -1) break;

		switch(c) {
			case 'c':
				config_add(&poller, optarg);
				break;

			case LOG_LEVEL:
				if(!io_safe_atoi(optarg, &i)) {
					fprintf(stderr, "Invalid number for log level: " "\"%s\"\n", optarg);
					exit(argument_error);
				}
				log_set_priority(i);
				break;

			case LOG_FILE:
				log_init(optarg);
				break;

			case 'v':
				log_set_priority(log_get_priority()+2);
				break;

			case '?':
				// user passed an unknown argument.
				// getopt_long has already printed the error message
				exit(argument_error);

			case 0:
				// an option was automatically set.
				break;

			default:
				fprintf(stderr, "getopt_long returned something weird: "
						"%d (%c)\n", c, c);
				exit(internal_error);
		}
	}
}


void sig_int(int sig)
{
    printf("Interrupted!  Gracefully exiting...\n");
    done += 1;
}


int main(int argc, char **argv)
{
	if(io_poller_init(&poller) < 0) {
		perror("io_init");
		exit(runtime_error);
	}
	
    log_set_priority(LOG_WARNING);
    log_init(NULL);

	process_args(argc, argv);
    if(optind >= argc) {
    	// we don't take any just regular arguments
        usage();
        exit(argument_error);
    }

    for(;;) {
		if(io_wait(&poller, MAXINT) < 0) {
			perror("io_wait");
		}
		io_dispatch(&poller);
	}

    // TODO: should delete all known resources?
	io_poller_dispose(&poller);

	return 0;
}

