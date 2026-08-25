//  Borland C++ 5.2 has no <cstring>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cstring
#define __gold_borland_cstring
#include <string.h>
#endif
