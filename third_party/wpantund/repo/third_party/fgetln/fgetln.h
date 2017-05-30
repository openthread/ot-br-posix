/*	@file fgetln.h
**	@author Robert Quattlebaum <darco@deepdarc.com>
**
**	Copyright (C) 2012 Robert Quattlebaum
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
**	-------------------------------------------------------------------
**
**	This is a simple implementation of the BSD function `fgetln()`,
**	for use on operating systems which do not have a copy.
**
**	Man page URL: <http://www.openbsd.org/cgi-bin/man.cgi?query=fgetln>
**
**	NOTE: This implementation is *NOT* thread safe!
*/

#define	_WITH_GETLINE 1

#include <stdio.h>

#if !defined(HAVE_FGETLN)
#define HAVE_FGETLN \
	defined(__DARWIN_C_LEVEL) && \
	(__DARWIN_C_LEVEL >= __DARWIN_C_FULL)
#endif

#if !defined(fgetln) && !HAVE_FGETLN
static char *
fgetln_(
    FILE *stream, size_t *len
    )
{
	static char* linep = NULL;
	static size_t linecap = 0;
	ssize_t length = getline(&linep, &linecap, stream);

	if (length == -1) {
		free(linep);
		linep = NULL;
		linecap = 0;
		length = 0;
	}
	if (len)
		*len = length;
	return linep;
}
#define fgetln  fgetln_
#endif
