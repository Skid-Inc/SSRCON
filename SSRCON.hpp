#ifndef	_SSRCON_H
#define _SSRCON_H

#define VERSION "1.00"

// Technically not needed but used by the logger
#define lock(x) (pthread_mutex_lock(&x))
#define trylock(x) (pthread_mutex_trylock(&x))
#define release(x) (pthread_mutex_unlock(&x))

// Short cuts to parse messages
#define parseInt16(x,y) ((x[y]<<8) | x[y+1])
#define parseInt32(x,y) ((x[y]<<24) | (x[y+1]<<16) | (x[y+2]<<8) | x[y+3])
#define parseInt64(x,y) (((uint64_t)x[y]<<56) | ((uint64_t)x[y+1]<<48) | ((uint64_t)x[y+2]<<40) | ((uint64_t)x[y+3]<<32) | ((uint64_t)x[y+4]<<24) | ((uint64_t)x[y+5]<<16) | ((uint64_t)x[y+6]<<8) | (uint64_t)x[y+7])

#define MAXDATAREAD	4089
#define DEFAULT_RCON_PORT	27015

// Message types
#define SERVERDATA_AUTH				3
#define SERVERDATA_AUTH_RESPONSE	2
#define SERVERDATA_EXECCOMMAND		2
#define SERVERDATA_RESPONSE_VALUE	0

// Socket task defines
#define RCON_CONNECT	0
#define RCON_AUTH		1
#define RCON_RUNNING	2
#define RCON_CLOSE		3

#endif