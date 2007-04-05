/* log.h
 * Scott Bronson
 * 25 Jan 2005
 * 
 * Logging infrastructure.
 */

// TODO: need to add logging categories too.  If I'm debugging
// a listener thread problem, I don't want to be buried under
// clerk I/O.

#include <syslog.h>
#include <stdarg.h>


// Some suggestions:
//
// log_emerg: the entire computer system is on fire (almost never used).
// log_wtf: something is really wrong and needs to be be looked at soon.
// log_crit: an error has been found that will affect more than just
//           the currently running program.
// log_err: definite errors that will impede the working of this program.
// log_note: major events, possible strangeness, chatty.
// log_info: notify size and type of each I/O operation, real talkative.
// log_dbg: log the actual bytes from each I/O operation, bury you in data.


// we only use logerr, logwarn, loginfo, and logdebug.
#define log_emerg(...) log_msg(LOG_EMERG, __VA_ARGS__)		// 0
#define log_emergency(...) log_msg(LOG_EMERG, __VA_ARGS__)
// I've usurped LOG_ALERT to mean "wtf?!"
//#define log_alert(...) log_msg(LOG_ALERT, __VA_ARGS__)	// 1
#define log_wtf(...) log_msg(LOG_ALERT, __VA_ARGS__)
#define log_crit(...) log_msg(LOG_CRIT, __VA_ARGS__)		// 2
#define log_critical(...) log_msg(LOG_CRIT, __VA_ARGS__)
#define log_err(...) log_msg(LOG_ERR, __VA_ARGS__)			// 3
#define log_error(...) log_msg(LOG_ERR, __VA_ARGS__)
#define log_warn(...) log_msg(LOG_WARNING, __VA_ARGS__)		// 4
#define log_warning(...) log_msg(LOG_WARNING, __VA_ARGS__)
#define log_note(...) log_msg(LOG_NOTICE, __VA_ARGS__)		// 5
#define log_info(...) log_msg(LOG_INFO, __VA_ARGS__)		// 6
#define log_dbg(...) log_msg(LOG_DEBUG, __VA_ARGS__)		// 7
#define log_debug(...) log_msg(LOG_DEBUG, __VA_ARGS__)


#ifdef NDEBUG

#define log_set_priority(x)
#define log_get_priority(x) 0
#define log_init(x)
#define log_close()
#define log_msg(x,y,...)
#define log_vmsg(x,y,...)

#else

void log_set_priority(int prio);
int log_get_priority();

void log_init(const char *file);
void log_close();
void log_msg(int priority, const char *fmt, ...);
void log_vmsg(int priority, const char *fmt, va_list ap);

#endif
