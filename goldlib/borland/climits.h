//  Borland C++ 5.2 has no <climits>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_climits
#define __gold_borland_climits
#include <limits.h>
#endif
