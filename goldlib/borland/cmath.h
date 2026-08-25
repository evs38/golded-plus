//  Borland C++ 5.2 has no <cmath>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cmath
#define __gold_borland_cmath
#include <math.h>
#endif
