//  Borland C++ 5.2 has no <csignal>; its library predates the <cxxx>
//  headers. Forward to the C header, which puts the names where this
//  tree expects them.
#ifndef __gold_borland_csignal
#define __gold_borland_csignal
#include <signal.h>
#endif
