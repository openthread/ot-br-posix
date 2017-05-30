/*!	@file assert-macros.h
**	@author Robert Quattlebaum <darco@deepdarc.com>
**	@brief "Assert" and "Require" macros
**
**  MIT-Licensed Version
**
**	Copyright (C) 2013 Robert Quattlebaum
**
**	Permission is hereby granted, free of charge, to any person
**	obtaining a copy of this software and associated
**	documentation files (the "Software"), to deal in the
**	Software without restriction, including without limitation
**	the rights to use, copy, modify, merge, publish, distribute,
**	sublicense, and/or sell copies of the Software, and to
**	permit persons to whom the Software is furnished to do so,
**	subject to the following conditions:
**
**	The above copyright notice and this permission notice shall
**	be included in all copies or substantial portions of the
**	Software.
**
**	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
**	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
**	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
**	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
**	OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
**	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
**	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
**	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
**	See <http://www.deepdarc.com/> for other cool stuff.
**
**	-------------------------------------------------------------------
**
**	See <http://www.mactech.com/articles/develop/issue_11/Parent_final.html>
**	for an explanation about how to use these macros and justification
**	for using this pattern in general.
*/

#ifndef __DARC_ASSERT_MACROS__
#define __DARC_ASSERT_MACROS__

#ifndef assert_error_stream
#if CONTIKI
#define assert_error_stream     stdout
#else
#define assert_error_stream     stderr
#endif
#endif

#if !DEBUG && !defined(NDEBUG)
#define NDEBUG 1
#endif

#if ASSERT_MACROS_USE_SYSLOG
#include <syslog.h>
#endif

#ifndef ASSERT_MACROS_SQUELCH
#define ASSERT_MACROS_SQUELCH !DEBUG
#endif

#ifndef ASSERT_MACROS_SYSLOG_LEVEL
#define ASSERT_MACROS_SYSLOG_LEVEL LOG_WARNING
#endif

#ifndef ASSERT_REQUIRE_FAILURE_HOOK
#define ASSERT_REQUIRE_FAILURE_HOOK       do { } while (false)
#endif

#if HAVE_ASSERTMACROS_H
 #include <AssertMacros.h>
#else
#include <stdio.h>
#include <assert.h>
#if ASSERT_MACROS_SQUELCH
 #define assert_printf(fmt, ...) do { } while (0)
 #define check_string(c, s)   do { } while (0)
 #define check_noerr(c)   do { } while (0)
 #define require_action_string(c, l, a, s) \
	do { if (!(c)) { \
		ASSERT_REQUIRE_FAILURE_HOOK; \
		a; \
		goto l; \
	} } while (0)
#else
 #if __AVR__
  #define assert_printf(fmt, ...) \
	fprintf_P(assert_error_stream, \
			  PSTR(__FILE__ ":%d: " fmt "\n"), \
			  __LINE__, \
			  __VA_ARGS__)
 #else
  #if ASSERT_MACROS_USE_SYSLOG
   #define assert_printf(fmt, ...) \
	syslog(ASSERT_MACROS_SYSLOG_LEVEL, \
		   __FILE__ ":%d: " fmt, \
		   __LINE__, \
		   __VA_ARGS__)
  #elif ASSERT_MACROS_USE_VANILLA_PRINTF
   #define assert_printf(fmt, ...) \
	printf(__FILE__ ":%d: " fmt "\n", \
		   __LINE__, \
		   __VA_ARGS__)
  #else
   #define assert_printf(fmt, ...) \
	fprintf(assert_error_stream, \
			__FILE__ ":%d: " fmt "\n", \
			__LINE__, \
			__VA_ARGS__)
  #endif
 #endif
 #define check_string(c, s) \
	do { if (!(c)) { \
		assert_printf("Check Failed (%s)", s); \
		ASSERT_REQUIRE_FAILURE_HOOK; \
	} } while (0)
 #define check_noerr(c) \
	do { \
		int ASSERT_MACROS_c = (int)c; \
		if (ASSERT_MACROS_c != 0) { \
		assert_printf("Check Failed (error %d)", ASSERT_MACROS_c); \
		ASSERT_REQUIRE_FAILURE_HOOK; \
	} } while (0)
 #define require_action_string(c, l, a, s) \
	do { if (!(c)) { \
		assert_printf("Requirement Failed (%s)", s); \
		ASSERT_REQUIRE_FAILURE_HOOK; \
		a; \
		goto l; \
	} } while (0)
#endif

 #define __ASSERT_MACROS_check(c)   check_string(c, # c)
#if !defined(__cplusplus)
 #define check(c)   __ASSERT_MACROS_check(c)
#endif
 #define require_quiet(c, l)   do { if (!(c)) goto l; } while (0)
 #define require(c, l)   require_action_string(c, l, {}, # c)

 #define require_noerr(c, l)   require((c) == 0, l)
 #define require_action(c, l, a)   require_action_string(c, l, a, # c)
 #define require_noerr_action(c, l, a)   require_action((c) == 0, l, a)
 #define require_string(c, l, s) \
	require_action_string(c, l, \
						  do {} while (0), s)
#endif

#if OVERRIDE_ASSERT_H
#undef assert
#undef __assert
#define assert(e)  \
	((void)((e) ? 0 : __assert(#e, __FILE__, __LINE__)))
#define __assert(e, file, line) \
	do { \
		assert_printf("failed assertion `%s'", e) ; \
		abort(); \
	} while (0)
#endif // OVERRIDE_ASSERT_H

// Ignores return value from function 's'
#define IGNORE_RETURN_VALUE(s)  do { if (s){} } while (0)

#endif // __DARC_ASSERT_MACROS__
