//  Borland C++ 5.2 has no <cctype>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cctype
#define __gold_borland_cctype
#include <ctype.h>
#endif
