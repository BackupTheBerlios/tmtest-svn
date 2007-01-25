/* zutest.c
 * Scott Bronson
 * 6 Mar 2006
 *
 * Version 0.6, 26 Apr 2006
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include "zutest.h"


/** @file zutest.c
 *
 * This file contains all of the test mechanisms provided by the
 * Zutest unit testing framework.
 *
 * A single function is called a test.  If any of the asserts fail
 * within a test, the test itself is stopped and printed as a failure
 * but all other tests in the current test suite, and all other test
 * suites, will still be run.
 *
 * A test suite consists of a number of tests.  Typically a C file
 * will include a test suite that lists all the tests in the file.
 *
 * TODO: print test results, test suites, etc as they run.
 *    Add a quiet flag that will suppress printing unless a test fails.
 *    quiet=0, full printing
 *    quiet=1, test results not printed
 *    quiet=2, suite results not printed
 *    quiet=3, summary not printed.
 */


int zutest_assertions = 0;		///< A goofy statistic, updated by the assertion macros
static int tests_run = 0;		///< The number of tests that we have run.  successes+failures==tests_run (if not, then there's a bug somewhere).
static int successes = 0;		///< The number of successful tests run
static int failures = 0;		///< The number of failed tests run.
static jmp_buf test_bail;		///< If an assertion fails, and we're not inverted, this is where we end up.
static jmp_buf *inversion;		///< If an assertion fails, and we're inverted, this is where we end up.  This is NULL except when running Zutest's internal unit tests.  See test_fail().
static int show_failures = 0; 	///< Set this to 1 to print the failures.  This allows you to view the output of each failure to ensure it looks OK.


void zutest_fail(const char *file, int line, const char *func, 
		const char *msg, ...)
{
	va_list ap;
	if(!inversion || show_failures) {
		fprintf(stderr, "%s:%d: In %s, assert ", file, line, func);
		va_start(ap, msg);
		vfprintf(stderr, msg, ap);
		va_end(ap);
		fputc('\n', stderr);
	}

	// If inversion is set, then an assert correctly failed.
	if(inversion) {
		longjmp(*inversion, 1);
	}

	longjmp(test_bail, 1);
}


void run_zutest_suite(const zutest_suite suite)
{
	const zutest_proc *test;

	for(test=suite; *test; test++) {
		tests_run += 1;
		if(!setjmp(test_bail)) {
			(*test)();
			successes += 1;
		} else {
			failures += 1;
		}
	}
}


void run_zutest_suites(const zutest_suites suites)
{
	zutest_suite *suite;

	for(suite=suites; *suite; suite++) {
		run_zutest_suite(*suite);
	}
}


void print_zutest_results()
{
	if(failures == 0) {
		printf("All OK.  %d test%s run, %d successe%s (%d assertion%s).\n",
				successes, (successes == 1 ? "" : "s"),
				successes, (successes == 1 ? "" : "s"),
				zutest_assertions, (zutest_assertions == 1 ? "" : "s"));
		return;
	}

	printf("ERROR: %d failure%s in %d test%s run!\n",
			failures, (failures == 1 ? "" : "s"), 
			tests_run, (tests_run == 1 ? "" : "s"));
}


/** Runs all the unit tests in all the passed-in test suites.
 */

void run_unit_tests(const zutest_suites suites)
{
	run_zutest_suites(suites);
	print_zutest_results();
	exit(failures < 100 ? failures : 100);
}


void run_unit_tests_showing_failures(const zutest_suites suites)
{
	show_failures = 1;
	run_unit_tests(suites);
}


/**
 * Examines the command-line arguments.  If "--run-unit-tests" is
 * the first argument, then it runs the unit tests (further arguments
 * may affect how the tests are processed).  This routine exits with
 * a nonzero result code if any test fails; otherwise it exits with 0.
 * It never returns.
 *
 * If --run-unit-tests is not on the command line, this routine returns
 * without doing anything.
 */

void unit_test_check(int argc, char **argv, const zutest_suites suites)
{
	if(argc > 1 && strcmp(argv[1],"--run-unit-tests") == 0) {
		run_unit_tests(suites);
	}
}





#if defined(ZUTEST) || defined(ZUTEST_MAIN)

/* This code runs the zutest unit tests to ensure that zutest itself
 * is working properly.
 */


/** This macro is used to reverse the sense of the tests. 
 *
 * To properly test Zutest, we need to ensure that the Assert macros
 * handle failures too.  Therefore, we occasionally want to reverse
 * the sense of the macro, where a failure indicates a successful test
 * and a passing assert means that the test has failed.
 *
 * This macro inverts the sense of the contained assertion.
 * test_failure(AssertEq(a,b)) causes the test to pass
 * only when the assertion fails (i.e. when a != b).
 */

#define test_failure(test) 				\
	do { 								\
		jmp_buf jb; 					\
		int val = setjmp(jb); 			\
		if(val == 0) { 					\
			inversion = &jb;			\
			do { test; } while(0);		\
			inversion = NULL;			\
			Fail("This test should have failed: " #test);	\
		}								\
		inversion = NULL;				\
	} while(0)



void test_assert_int()
{
	int a=4, b=3, c=4, z=0, n=-1;

	AssertEq(a,c);
	AssertNe(a,b);
	AssertGt(a,b);
	AssertGe(a,b);
	AssertGe(a,c);
	AssertLt(b,a);
	AssertLe(b,a);
	AssertLe(c,a);

	test_failure( AssertEq(a,b) );
	test_failure( AssertNe(a,c) );
	test_failure( AssertGt(a,c) );
	test_failure( AssertGt(b,c) );
	test_failure( AssertGe(b,a) );
	test_failure( AssertLt(c,a) );
	test_failure( AssertLt(c,b) );
	test_failure( AssertLe(a,b) );

	AssertZero(z);
	test_failure( AssertZero(a) );
	AssertNonzero(a);
	test_failure( AssertNonzero(z) );

	AssertPositive(a);
	test_failure( AssertPositive(z) );
	test_failure( AssertPositive(n) );

	AssertNonPositive(n);
	AssertNonPositive(z);
	test_failure( AssertNonPositive(a) );

	AssertNegative(n);
	test_failure( AssertNegative(z) );
	test_failure( AssertNegative(a) );

	AssertNonNegative(a);
	AssertNonNegative(z);
	test_failure( AssertNonNegative(n) );
}


void test_assert_hex()
{
	int a=4, b=3, c=4, z=0, n=-1;

	AssertEqHex(a,c);
	AssertNeHex(a,b);
	AssertGtHex(a,b);
	AssertGeHex(a,b);
	AssertGeHex(a,c);
	AssertLtHex(b,a);
	AssertLeHex(b,a);
	AssertLeHex(c,a);

	test_failure( AssertEqHex(a,b) );
	test_failure( AssertNeHex(a,c) );
	test_failure( AssertGtHex(a,c) );
	test_failure( AssertGtHex(b,c) );
	test_failure( AssertGeHex(b,a) );
	test_failure( AssertLtHex(c,a) );
	test_failure( AssertLtHex(c,b) );
	test_failure( AssertLeHex(a,b) );

	AssertZeroHex(z);
	test_failure( AssertZeroHex(a) );
	AssertNonzeroHex(a);
	test_failure( AssertNonzeroHex(z) );

	AssertPositiveHex(a);
	test_failure( AssertPositiveHex(z) );
	test_failure( AssertPositiveHex(n) );

	AssertNonPositiveHex(n);
	AssertNonPositiveHex(z);
	test_failure( AssertNonPositiveHex(a) );

	AssertNegativeHex(n);
	test_failure( AssertNegativeHex(z) );
	test_failure( AssertNegativeHex(a) );

	AssertNonNegativeHex(a);
	AssertNonNegativeHex(z);
	test_failure( AssertNonNegativeHex(n) );
}


void test_assert_ptr()
{
	int a, b;
	int *ap = &a;
	int *bp = &b;
	int *cp = &a;
	int *n = NULL;

	AssertPtr(ap);
	AssertNull(n);

	test_failure( AssertPtr(n) );
	test_failure( AssertNull(ap) );

	AssertPtrEq(ap,cp);
	AssertPtrNe(ap,bp);
	AssertPtrGt(ap,bp);
	AssertPtrGe(ap,bp);
	AssertPtrGe(ap,cp);
	AssertPtrLt(bp,ap);
	AssertPtrLe(bp,ap);
	AssertPtrLe(cp,ap);

	test_failure( AssertPtrEq(ap,bp) );
	test_failure( AssertPtrNe(ap,cp) );
	test_failure( AssertPtrGt(ap,cp) );
	test_failure( AssertPtrGt(bp,cp) );
	test_failure( AssertPtrGe(bp,ap) );
	test_failure( AssertPtrLt(cp,ap) );
	test_failure( AssertPtrLt(cp,bp) );
	test_failure( AssertPtrLe(ap,bp) );
}


void test_assert_float()
{
	float a=0.0004, b=0.0003, c=0.0004;

	AssertFloatEq(a,c);
	AssertFloatNe(a,b);
	AssertFloatGt(a,b);
	AssertFloatGe(a,b);
	AssertFloatGe(a,c);
	AssertFloatLt(b,a);
	AssertFloatLe(b,a);
	AssertFloatLe(c,a);

	test_failure( AssertFloatEq(a,b) );
	test_failure( AssertFloatNe(a,c) );
	test_failure( AssertFloatGt(a,c) );
	test_failure( AssertFloatGt(b,c) );
	test_failure( AssertFloatGe(b,a) );
	test_failure( AssertFloatLt(c,a) );
	test_failure( AssertFloatLt(c,b) );
	test_failure( AssertFloatLe(a,b) );

	AssertDoubleEq(a,c);
	AssertDoubleNe(a,b);
	AssertDoubleGt(a,b);
	AssertDoubleGe(a,b);
	AssertDoubleGe(a,c);
	AssertDoubleLt(b,a);
	AssertDoubleLe(b,a);
	AssertDoubleLe(c,a);

	test_failure( AssertDoubleEq(a,b) );
	test_failure( AssertDoubleNe(a,c) );
	test_failure( AssertDoubleGt(a,c) );
	test_failure( AssertDoubleGt(b,c) );
	test_failure( AssertDoubleGe(b,a) );
	test_failure( AssertDoubleLt(c,a) );
	test_failure( AssertDoubleLt(c,b) );
	test_failure( AssertDoubleLe(a,b) );
}


void test_assert_strings()
{
	const char *a = "Bogozity";
	const char *b = "Arclamp";
	const char *c = "Bogozity";
	const char *e = "";
	const char *n = NULL;

	AssertStrEq(a,c);
	AssertStrNe(a,b);
	AssertStrGt(a,b);
	AssertStrGe(a,b);
	AssertStrGe(a,c);
	AssertStrLt(b,a);
	AssertStrLe(b,a);
	AssertStrLe(c,a);

	test_failure( AssertStrEq(a,b) );
	test_failure( AssertStrNe(a,c) );
	test_failure( AssertStrGt(a,c) );
	test_failure( AssertStrGt(b,c) );
	test_failure( AssertStrGe(b,a) );
	test_failure( AssertStrLt(c,a) );
	test_failure( AssertStrLt(c,b) );
	test_failure( AssertStrLe(a,b) );

	AssertStrEmpty(e);
	test_failure( AssertStrEmpty(a) );
	test_failure( AssertStrEmpty(n) );

	AssertStrNonEmpty(a);
	test_failure( AssertStrNonEmpty(e) );
	test_failure( AssertStrNonEmpty(n) );
}


zutest_proc zutest_tests[] = {
	test_assert_int,
	test_assert_hex,
	test_assert_ptr,
	test_assert_float,
	test_assert_strings,
	NULL
};


// Ensure that zutest doesn't crash if handed an empty suite.
zutest_proc zutest_empty_suite[] = {
	NULL
};


zutest_suite all_zutests[] = {
	zutest_empty_suite,
	zutest_tests,
	NULL
};


#ifdef ZUTEST_MAIN
int main(int argc, char **argv)
{
	if(argc > 1) {
		// "zutest -f" prints all the failures in the zutest unit tests.
		// This allows you to check the output of each macro.
		run_unit_tests_showing_failures(all_zutests);
	} else {
		run_unit_tests(all_zutests);
	}
	// this will never be reached
	return 0;
}
#endif

#endif

