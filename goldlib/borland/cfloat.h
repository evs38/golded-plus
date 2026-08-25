//  Borland C++ 5.2 has no <cfloat>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cfloat
#define __gold_borland_cfloat
#include <float.h>
#endif
