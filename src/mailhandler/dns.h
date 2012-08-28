#ifndef _DNS_H_
#define _DNS_H_

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#define PACKETSZ 2046

char *dns_gettxt(const char *name, int *ttl);

#endif
