/* zutest.h
 * Scott Bronson
 * 6 Mar 2006
 *
 * TODO: make tests self-documenting.  The test name is the same as the
 * function name, but they should also have a short and long description.
 * TODO: make zutest suites able to be arranged in a hierarchy.
 *
 * Version 0.62, 22 Jan 2007
 * Version 0.61, 30 Apr 2006
 */


/* @file zutest.h
 *
 * This file contains the declarations and all the Assert macros
 * required to use Zutest in your own applications.
 *
 * Zutest is a ground-up rewrite of Asim Jalis's "CuTest" library.
 * It is released under the MIT License.
 *
 * To compile Zutest to run its own unit tests, do this:
 * 
 * <pre>
 * 	$ cc -DZUTEST_MAIN zutest.c -o zutest
 * 	$ ./zutest
 * 	4 tests run, 4 successes (132 assertions).
 * </pre>
 *
 * If your non-gcc compiler complains about a missing __func__ macro,
 * add -D__func__='"test"' to the compiler's command line.
 *
 * See ::zutest_tests for instructions on how to add zutest's
 * built-in unit tests to your application's test suite.
 */


#ifndef ZUTEST_H
#define ZUTEST_H

// Note that Fail doesn't increment zutest_assertions (the number of assertions
// that have been made) because it doesn't assert anything.  It only fails.
// If you call fail, you might want to increment zutest_assertions
// manually if you care about this number.  Normally you won't care.
#define Fail(...) zutest_fail(__FILE__, __LINE__, __func__, __VA_ARGS__)

// If the expression returns false, it is printed in the failure message.
#define Assert(x) do { zutest_assertions++; \
		if(!(x)) { Fail(#x); } } while(0)

// If the expression returns false, the given format string is printed.
// This is the same as Assert, just with much more helpful error messages.
// For instance: AssertFmt(isdigit(x), "isdigit but x=='%c'", x);
#define AssertFmt(x,...) do { zutest_assertions++; \
		if(!(x)) { Fail(__VA_ARGS__); } } while(0)

// integers, longs, chars...
#define AssertEq(x,y) AssertOp(x,y,==)
#define AssertNe(x,y) AssertOp(x,y,!=)
#define AssertGt(x,y) AssertOp(x,y,>)
#define AssertGe(x,y) AssertOp(x,y,>=)
#define AssertLt(x,y) AssertOp(x,y,<)
#define AssertLe(x,y) AssertOp(x,y,<=)

#define AssertZero(x) AssertOpToZero(x,==)
#define AssertNonzero(x) AssertOpToZero(x,!=)
#define AssertNonZero(x) AssertNonzero(x)
#define AssertPositive(x) AssertOpToZero(x,>);
#define AssertNegative(x) AssertOpToZero(x,<);
#define AssertNonNegative(x) AssertOpToZero(x,>=);
#define AssertNonPositive(x) AssertOpToZero(x,<=);

// Also integers but failure values are printed in hex rather than decimal.
#define AssertEqHex(x,y) AssertHexOp(x,y,==)
#define AssertNeHex(x,y) AssertHexOp(x,y,!=)
#define AssertGtHex(x,y) AssertHexOp(x,y,>)
#define AssertGeHex(x,y) AssertHexOp(x,y,>=)
#define AssertLtHex(x,y) AssertHexOp(x,y,<)
#define AssertLeHex(x,y) AssertHexOp(x,y,<=)

#define AssertZeroHex(x) AssertHexOpToZero(x,==)
#define AssertNonzeroHex(x) AssertHexOpToZero(x,!=)
#define AssertNonZeroHex(x) AssertNonzeroHex(x)
#define AssertPositiveHex(x) AssertHexOpToZero(x,>);
#define AssertNegativeHex(x) AssertHexOpToZero(x,<);
#define AssertNonNegativeHex(x) AssertHexOpToZero(x,>=);
#define AssertNonPositiveHex(x) AssertHexOpToZero(x,<=);

// Pointers...
#define AssertPtr(p)  AssertFmt(p != NULL, \
		#p" != NULL but "#p"==0x%lX!", (unsigned long)p)
#define AssertNull(p) AssertFmt(p == NULL, \
		#p" == NULL but "#p"==0x%lX!", (unsigned long)p)
#define AssertNonNull(p) AssertPtr(p)

#define AssertPtrNull(p) AssertNull(p)
#define AssertPtrNonNull(p) AssertNonNull(p)
#define AssertPtrEq(x,y) AssertPtrOp(x,y,==)
#define AssertPtrNe(x,y) AssertPtrOp(x,y,!=)
#define AssertPtrGt(x,y) AssertPtrOp(x,y,>)
#define AssertPtrGe(x,y) AssertPtrOp(x,y,>=)
#define AssertPtrLt(x,y) AssertPtrOp(x,y,<)
#define AssertPtrLe(x,y) AssertPtrOp(x,y,<=)

// These work with floats and doubles
// (everything is handled internally as double)
#define AssertFloatEq(x,y) AssertFloatOp(x,y,==)
#define AssertFloatNe(x,y) AssertFloatOp(x,y,!=)
#define AssertFloatGt(x,y) AssertFloatOp(x,y,>)
#define AssertFloatGe(x,y) AssertFloatOp(x,y,>=)
#define AssertFloatLt(x,y) AssertFloatOp(x,y,<)
#define AssertFloatLe(x,y) AssertFloatOp(x,y,<=)
// supply Doubles so people don't worry about precision when they see Float
#define AssertDoubleEq(x,y) AssertFloatOp(x,y,==)
#define AssertDoubleNe(x,y) AssertFloatOp(x,y,!=)
#define AssertDoubleGt(x,y) AssertFloatOp(x,y,>)
#define AssertDoubleGe(x,y) AssertFloatOp(x,y,>=)
#define AssertDoubleLt(x,y) AssertFloatOp(x,y,<)
#define AssertDoubleLe(x,y) AssertFloatOp(x,y,<=)

// Strings (uses strcmp)...
#define AssertStrEq(x,y) AssertStrOp(x,y,eq,==)
#define AssertStrNe(x,y) AssertStrOp(x,y,ne,!=)
#define AssertStrGt(x,y) AssertStrOp(x,y,gt,>)
#define AssertStrGe(x,y) AssertStrOp(x,y,ge,>=)
#define AssertStrLt(x,y) AssertStrOp(x,y,lt,<)
#define AssertStrLe(x,y) AssertStrOp(x,y,le,<=)

// ensures a string is non-null but zero-length
#define AssertStrEmpty(p) do { zutest_assertions++; \
		if(!(p)) { Fail(#p" should be empty but it is NULL!"); } \
		if((p)[0]) { Fail(#p" should be empty but it is: %s",p); } \
	} while(0)
// ensures a string is non-null and non-zero-length
#define AssertStrNonEmpty(p) do { zutest_assertions++; \
		if(!(p)) { Fail(#p" should be nonempty but it is NULL!"); } \
		if(!(p)[0]) { Fail(#p" should be nonempty but "#p"[0] is 0"); } \
	} while(0)



//
// helper macros, not intended to be called directly.
//

#define AssertExpType(x,y,op,type,fmt) \
	AssertFmt((type)x op (type)y, #x" "#op" "#y" failed because " \
	#x"=="fmt" and "#y"=="fmt"!", (type)x,(type)y)
// The failure "x==0 failed because x==1 and 0==0" s too wordy so we'll
// special-case checking against 0: x==0 failed because x==1).
#define AssertExpToZero(x,op,type,fmt) \
	AssertFmt((type)x op 0,#x" "#op" 0 failed because "#x"=="fmt"!", (type)x)

#define AssertOp(x,y,op) AssertExpType(x,y,op,long,"%ld")
#define AssertHexOp(x,y,op) AssertExpType(x,y,op,long,"0x%lX")
#define AssertOpToZero(x,op) AssertExpToZero(x,op,long,"%ld")
#define AssertHexOpToZero(x,op) AssertExpToZero(x,op,long,"0x%lX")
#define AssertPtrOp(x,y,op) AssertExpType(x,y,op,unsigned long,"0x%lX")
#define AssertFloatOp(x,y,op) AssertExpType(x,y,op,double,"%lf")
#define AssertStrOp(x,y,opn,op) AssertFmt(strcmp(x,y) op 0, \
	#x" "#opn" "#y" but "#x" is \"%s\" and "#y" is \"%s\"!",x,y)



/** Keeps track of how many assertions have been made.
 * This needs to be updated manually each time an assertion
 * is made.  The Zutest built-in assertion macros all
 * update this variable properly.
 */

extern int zutest_assertions;


/** A single test
 *
 * This routine is called to run the test.  If it returns, the test
 * succeeds.  If zutest_fail() is called (either directly or indirectly
 * via an Assert macro), then the test fails.
 */
typedef void (*zutest_proc)();


/** A suite of tests
 *
 * A zutest_suite is simply a list of tests.  Generally, each .c file
 * in your project will include a test suite that ensures all the tests
 * contained in the .c file are run.  A suite is just a NULL-terminated
 * list of tests.
 */
typedef zutest_proc *zutest_suite;


/** A suite of test suites
 *
 * Zutests runs through each test suite in your project, running all the
 * tests in each suite.  A suite of suites is just a NULL-terminated list
 * of suites.  This is the topmost data structure used by zutest.
 * TODO: make it so zutest chan handle an arbitrary hierarchy of suites.
 * That way this data structure can go away.
 */
typedef zutest_suite *zutest_suites;


/** Fails the current test.
 *
 * This function may only be called from within a ::zutest_proc.
 *
 * If none of the built-in Assert macros fit your fancy, you can do the
 * check on your own and call zutest_fail in the event that it fails.
 * 
 * Example:
 * 
 * <pre>
 * if(my_error) {
 *    zutest_fail(__FILE__, __LINE__, __func__, "Error Message %d", 1);
 * }
 * </pre>
 *
 * But, really, it's easier just to call the Fail() macro.
 */

void zutest_fail(const char *file, int line, const char *func,
		const char *msg, ...);


/** Runs all the tests in a suite. */
void run_zutest_suite(const zutest_suite suite);
/** Runs all the tests in all the suites passed in. */
void run_zutest_suites(const zutest_suites suites);

void print_zutest_results();


/** 
 *
 * Call this on the very first line of your application.  If the user
 * ran your program with the first arg of "--run-unit-tests", this will
 * run the tests and exit.  Otherwise your program will run as normal.
 * If you would rather create a dedicated executable, just call
 * run_zutest_suites() directly.
 */

void unit_test_check(int argc, char **argv, const zutest_suites suites);

/**
 *
 * This runs all the unit tests supplied and then exits.  Use this
 * if you want to handle the arguments yourself.
 */

void run_unit_tests(const zutest_suites suites);


/** Zutest's built-in test suite.
 *
 * This allows you to add the Zutest unit test suite to your application's
 * test suites.  This way, you can ensure that Zutest's unit tests pass
 * before running your application's.  This is for the especially pedantic. :)
 *
 * Unfortunately, there is one test that cannot be run if you do this:
 * ensuring that zutest properly handles empty test suites.
 * Other than this one test, adding zutest_tests
 * to your application's test suite is equivalent to causing zutest to
 * compile and run its unit tests as described in zutest.h.
 */

extern zutest_proc zutest_tests[];

#endif
