//  Borland C++ 5.2 has no <cstdio>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_cstdio
#define __gold_borland_cstdio
#include <stdio.h>
#endif
