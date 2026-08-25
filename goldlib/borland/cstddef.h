//  Borland C++ 5.2 has no <cstddef>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cstddef
#define __gold_borland_cstddef
#include <stddef.h>
#endif
