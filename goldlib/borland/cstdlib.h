//  Borland C++ 5.2 has no <cstdlib>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cstdlib
#define __gold_borland_cstdlib
#include <stdlib.h>
#endif
