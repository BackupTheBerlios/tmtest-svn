/* main.c
 * 28 Dec 2004
 * Copyright (C) 2004 Scott Bronson
 * This entire file is covered by the GNU GPL V2.
 * 
 * The main routine for tmtest.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <assert.h>

#include "test.h"
#include "qscandir.h"
#include "vars.h"


enum {
    outmode_normal,
    outmode_output,
    outmode_diff
} outmode;

int verbose = 0;
double timeout = 10.0;		// timeout in seconds


// exit values:
enum {
    no_error = 0,
    argument_error,
    runtime_error,
};


#define xstringify(x) #x
#define stringify(x) xstringify(x)


void add_define(char *def)
{
    printf("Adding define %s\n", def);
}


/** Returns zero if s1 ends with s2, nonzero if not.
 */

int strcmpend(const char *s1, const char *s2)
{
    size_t n1 = strlen(s1);
    size_t n2 = strlen(s2);

    if(n2 <= n1) {
        return strncmp(s1+n1-n2, s2, n2);
    } else {
        return 1;
    }
}


/** Prints the given template to the given file, performing substitutions.
 */

void print_template(struct test *test, const char *tmpl,  FILE *fp)
{
    char varbuf[32];
    const char *cp, *ocp, *ce;
    int len;

    for(ocp=cp=tmpl; (cp=strchr(cp,'%')); cp++) {
        if(cp[1] == '(') {
            // perform a substitution
            fwrite(ocp, cp - ocp, 1, fp);
            cp += 2;
            ce = strchr(cp,')');
            if(!ce) {
                fprintf(stderr, "Unterminated template variable: '%.20s'\n", cp);
                exit(runtime_error);
            }
            len = ce - cp;
            if(len <= 0) {
                fprintf(stderr, "Garbage template variable: '%.20s'\n", cp);
                exit(runtime_error);
            }
            // truncate variable name if it doesn't fit into varbuf.
            if(len > sizeof(varbuf)-1) {
                len = sizeof(varbuf)-1;
            }
            memcpy(varbuf, cp, len);
            varbuf[len] = '\0';
            if(printvar(test,fp,varbuf) != 0) {
                // printvar has already printed the error message
                exit(runtime_error);
            }
            ocp = cp = ce+1;
        }
    }

    fputs(ocp, fp);
}


void dump_test(struct test *test)
{
    printf("test: filename='%s'\n", test->filename);
    printf("test: outfile='%s'\n", test->outfile);
    printf("test: errfile='%s'\n", test->errfile);
    printf("test: oobfile='%s'\n", test->oobfile);
    printf("test: oobsep='%s'\n", test->oobsep);
}


/** Runs the named testfile.
 *
 * If warn_suffix is true and the ffilename doesn't end in ".test"
 * then we'll print a warning to stderr.  This is used when
 * processing the cmdline args so the user will know why a file
 * explicitly named didn't run.
 */

void process_file(const char *name, int warn_suffix)
{
    struct test test;

    // defined in the exec.c file generated by exec.tmpl.
    extern const char exec_template[];

    if(strcmpend(name, ".test") != 0) {
        if(warn_suffix) {
            fprintf(stderr, "%s was skipped because it doesn't end in '.test'.\n", name);
        }
        return;
    }

    memset(&test, 0, sizeof(struct test));
    test.filename = name;

    // fork subprocess
    //   redirect stdout & stderr to /dev/null
    //   exec shell and pass this file:

    print_template(&test, exec_template, stdout);
    // dump_test(&test);
    // printf("ok   %s\n", name);
}


/** This routine filters out any dirents that begin with '.'.
 *  We don't want to process any hidden files or special directories.
 */

int select_nodots(const struct dirent *d)
{
    return d->d_name[0] != '.';
}


// forward declaration for recursion
void process_dir();


/** Process all entries in a directory.
 *
 * See process_file() for an explanation of warn_suffix.
 */

void process_ents(char **ents, int warn_suffix)
{
    static char pathbuf[PATH_MAX];
    struct stat st;
    mode_t *modes;
    int i, n;
    char *slash;

    for(n=0; ents[n]; n++)
        ;

    modes = malloc(n * sizeof(mode_t));
    if(!modes) {
        fprintf(stderr, "Could not allocate %d mode_t objects.\n", n);
        exit(runtime_error);
    }
    
    // first collect the stat info for each entry
    for(i=0; i<n; i++) {
        if(stat(ents[i], &st) < 0) {
            fprintf(stderr, "stat error on '%s': %s\n", ents[i], strerror(errno));
            exit(runtime_error);
        }
        modes[i] = st.st_mode;
    }

    // process all files in dir
    for(i=0; i<n; i++) {
        if(S_ISREG(modes[i])) {
            process_file(ents[i], warn_suffix);
            modes[i] = 0;
        }
    }

    // process all subdirs
    for(i=0; i<n; i++) {
        if(modes[i] == 0) continue;
        if(S_ISDIR(modes[i])) {
            chdir(ents[i]);
            if(pathbuf[0]) strncat(pathbuf, "/", sizeof(pathbuf));
            strncat(pathbuf, ents[i], sizeof(pathbuf));
            printf("\nProcessing %s\n", pathbuf);
            process_dir();
            slash = strrchr(pathbuf, '/');
            if(slash) {
                *slash = '\0';
            } else {
                pathbuf[0] = '\0';
            }
            chdir("..");
        }
    }

    free(modes);
}


/** Runs all tests in the current directory and all its subdirectories.
 */

void process_dir()
{
    char **ents;
    int i;

    ents = qscandir(".", select_nodots, qdirentcoll);
    if(!ents) {
        // qscandir has already printed the error message
        exit(runtime_error);
    }

    process_ents(ents, 0);

    for(i=0; ents[i]; i++) {
        free(ents[i]);
    }
    free(ents);
}


void usage()
{
	printf(
			"Usage: tmtest [OPTION]... [DLDIR]\n"
			"  -o: output the test file with the new output.\n"
			"  -d: output a diff between the expected and actual outputs.\n"
            "  -D NAME=VAL: define a variable on the command line.\n"
            "  -t --timeout: time in seconds before test is terminated.\n"
			"  -v --verbose: increase verbosity.\n"
			"  -V --version: print the version of this program.\n"
			"  -h --help: prints this help text\n"
			"Run tmtest with no arguments to run all tests in the current directory.\n"
		  );
}


void process_args(int argc, char **argv)
{
    char buf[256], *cp;
    int optidx, i, c;

	while(1) {
		optidx = 0;
		static struct option longopts[] = {
			// name, has_arg (1=reqd,2=opt), flag, val
			{"diff", 0, 0, 'd'},
			{"define", 1, 0, 'D'},
			{"help", 0, 0, 'h'},
			{"output", 0, 0, 'o'},
			{"timeout", 1, 0, 't'},
			{"verbose", 0, 0, 'v'},
			{"version", 0, 0, 'V'},
			{0, 0, 0, 0},
		};

        // dynamically create the option string from the long
        // options.  Why oh why doesn't glibc do this for us???
        cp = buf;
        for(i=0; longopts[i].name; i++) {
            *cp++ = longopts[i].val;
            if(longopts[i].has_arg > 0) *cp++ = ':';
            if(longopts[i].has_arg > 1) *cp++ = ':';
        }
        *cp++ = '\0';

		c = getopt_long(argc, argv, buf, longopts, &optidx);
		if(c == -1) break;

		switch(c) {
            case 'd':
                outmode = outmode_diff;
                break;

			case 'D':
                add_define(optarg);
				break;

			case 'h':
				usage();
				exit(0);

			case 'o':
                outmode = outmode_output;
				break;

			case 't':
				sscanf(optarg, "%lf", &timeout);
				break;

			case 'v':
				verbose++;
				break;

			case 'V':
				printf("tmtest version %s\n", stringify(VERSION));
				exit(0);

                /*
			case 0:
			case '?':
				break;
                */

			default:
				exit(argument_error);

		}
	}

	if(verbose) {
        switch(outmode) {
            case outmode_normal:
                printf("Running tests.\n");
                break;
            case outmode_output:
                printf("Outputting tests.\n");
                break;
            case outmode_diff:
                printf("Diffing tests.\n");
                break;
            default:
                assert(0);
        }
		printf("Timeout is %lf seconds\n", timeout);
	}
}


int main(int argc, char **argv)
{
	process_args(argc, argv);

    if(optind < argc) {
        process_ents(argv+1, 1);
    } else {
        process_dir();
    }

	return 0;
}

