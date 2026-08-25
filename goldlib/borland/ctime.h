//  Borland C++ 5.2 has no <ctime>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_ctime
#define __gold_borland_ctime
#include <time.h>
#endif
