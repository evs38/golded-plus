//  Borland C++ 5.2 has no <cassert>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cassert
#define __gold_borland_cassert
#include <assert.h>
#endif
