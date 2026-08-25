//  Borland C++ 5.2 has no <clocale>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_clocale
#define __gold_borland_clocale
#include <locale.h>
#endif
