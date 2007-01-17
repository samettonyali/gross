
#ifndef COMMON_H
#define COMMON_H

/* autoconf */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/*
 * common system includes
 */

/* socket(), inet_pton() etc */
#include <sys/types.h>
#include <sys/socket.h>
#if NETINET_IN_H
# include <netinet/in.h>
#endif
#include <arpa/inet.h>

#include <assert.h>
#include <string.h> 	/* memcpy(), memset() etc */
#include <stdlib.h>	/* malloc(), atoi() etc */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <semaphore.h>

#ifdef HAVE_ARES_H
# include <ares.h>
# define DNSBL
#endif

#if PROTOCOL == POSTFIX
# define WORKER_PROTO_TCP
#elif PROTOCOL == SJSMS
# define WORKER_PROTO_UDP
#else
# error "No PROTOCOL defined!"
#endif

/* what clock type to use */
#if HAVE_DECL_CLOCK_MONOTONIC
# define CLOCK_TYPE CLOCK_MONOTONIC
#elif HAVE_DECL_CLOCK_HIGHRES
# define CLOCK_TYPE CLOCK_HIGHRES
#else
# error No suitable clock type found (CLOCK_MONOTONIC or CLOCK_HIGHRES)
#endif

/*
 * project includes 
 */
#include "bloom.h"

/*
 * common defines and macros
 */
#define MSGSZ           1024
#define MAXLINELEN      MSGSZ
#define GROSSPORT	1111	/* default port for server */

#define STARTUP_SYNC ((uint32_t)0x00)
#define OPER_SYNC ((uint32_t)0x01)
#define AGGREGATE_SYNC ((uint32_t)0x02)

#define FLG_NODAEMON (int)0x01
#define FLG_NOREPLICATE (int)0x02
#define FLG_UPDATE_ALWAYS (int)0x04

#define MAX(a,b) 	((a) > (b) ? (a) : (b))
#define MIN(a,b) 	((a) < (b) ? (a) : (b))

/*
 * common types
 */
typedef struct {
	struct sockaddr_in peer_addr;
	pthread_mutex_t peer_in_mutex;
	int peerfd_in;
	int peerfd_out;
	int connected;
} peer_t;

typedef struct {
  unsigned int greylisted;
  unsigned int match;
  unsigned int trust;
} statistics_t;

typedef struct {
	struct sockaddr_in gross_host;
	struct sockaddr_in sync_host;
	struct sockaddr_in status_host;
	peer_t peer;
	int max_connq;
	int max_threads;
	time_t rotate_interval;
	bitindex_t filter_size;
	unsigned int num_bufs;
	char *statefile;
	int loglevel;
	int acctmask;
	int flags;
} gross_config_t;

#ifdef DNSBL
typedef struct dnsbl_s {
        const char *name;
        int weight;
        sem_t *failurecount_sem;
        struct dnsbl_s *next; /* linked list */
} dnsbl_t;
#endif /* DNSBL */

typedef int mseconds_t;
typedef void (*tmout_action)(void *arg, mseconds_t timeused);

/* timeout action list */
typedef struct tmout_action_s {
        mseconds_t timeout;             /* milliseconds */
        tmout_action action;
        void *arg;
        struct tmout_action_s *next;
} tmout_action_t;

typedef struct {
  pthread_t* thread;
  time_t watchdog;
} thread_info_t;

typedef struct {
  thread_info_t bloommgr;
  thread_info_t syncmgr;
  thread_info_t worker;
} thread_collection_t;

typedef struct {
        bloom_ring_queue_t *filter;
        sem_t *workercount_sem;
        int log_q;
        int update_q;
        sem_t* sync_guard;
        pthread_mutex_t bloom_guard;
        time_t* last_rotate;
#ifdef DNSBL
        dnsbl_t *dnsbl;
#endif /* ENDBL */
        gross_config_t config;
        mmapped_brq_t *mmap_info;
        thread_collection_t process_parts;
        statistics_t stats;
} gross_ctx_t;

#ifndef HAVE_USECONDS_T
typedef unsigned long useconds_t;
#endif /* HAVE_USECONDS_T */

extern int cleanup_in_progress;

#endif