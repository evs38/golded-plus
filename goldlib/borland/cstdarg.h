//  Borland C++ 5.2 has no <cstdarg>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cstdarg
#define __gold_borland_cstdarg
#include <stdarg.h>
#endif
