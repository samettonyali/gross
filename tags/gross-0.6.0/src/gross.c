/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
 *                    Antti Siira <antti@utu.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <signal.h>
#include <syslog.h>

#include "common.h"
#include "conf.h"
#include "srvutils.h"
#include "msgqueue.h"

#ifdef DNSBL
#include "dnsblc.h"
#endif /* DNSBL */

/* maximum simultaneus tcp worker threads */
#define MAXWORKERS 1

#define MAXCONNQ 5

#define SECONDS_IN_HOUR ((time_t)60*60)
#define MAX_PEER_NAME_LEN 1024

/* function prototypes */
void bloommgr_init();
void syncmgr_init();
void worker_init();
void srvstatus_init();

gross_ctx_t *
initialize_context()
{
	gross_ctx_t *ctx;
	sem_t *sp;
	int ret;
	
	ctx = Malloc(sizeof(gross_ctx_t));

	/* Clear flags  */
	ctx->config.flags = 0;

	ctx->filter = NULL;
	
	memset(&ctx->config.gross_host, 0, sizeof(ctx->config.gross_host));
	memset(&ctx->config.sync_host, 0, sizeof(ctx->config.sync_host));
	memset(&ctx->config.peer.peer_addr, 0, sizeof(ctx->config.peer.peer_addr));
	memset(&ctx->config.status_host, 0, sizeof(ctx->config.status_host));

	ctx->config.peer.peerfd_out = -1;
	ctx->config.peer.peerfd_in = -1;
	
	ctx->last_rotate = Malloc(sizeof(time_t));

#ifdef DNSBL
	ctx->dnsbl = NULL;
#endif /* DNSBL */

	return ctx;
}

void
configure_grossd(configlist_t *config)
{
	sem_t *sp;
	int ret;
	configlist_t *cp;
	const char *tmp;
	struct timespec *delay;
	const char *updatestr;
	
#ifdef DEBUG_CONFIG
	while (config) {
		printf("%s = %s\n", config->name, config->value);
		config = config->next;
	}
	exit(1);
#endif

	/* initialize the message queue system for 4 message queues */
	ret = queue_init(4);
	assert(ret == 0);
	
	/* initialize the update queue */
	delay = Malloc(sizeof(struct timespec));
	delay->tv_sec = 10;
	delay->tv_nsec = 0;
	ctx->update_q = get_delay_queue(delay);
	if (ctx->update_q < 0)
		daemon_perror("get_delay_queue");

	/* initialize semaphore for worker thread counting */
	sp = Malloc(sizeof(sem_t));

	ret = sem_init(sp, 0, (unsigned int) MAXWORKERS);
	if (ret != 0)
		daemon_perror("sem_init");
	ctx->workercount_sem = sp;

	ctx->sync_guard = Malloc(sizeof(sem_t));
	sem_init(ctx->sync_guard, 0, 1); /* Process local (0), initial count 1. */

	pthread_mutex_init(&ctx->bloom_guard, NULL);
	
	pthread_mutex_init(&ctx->config.peer.peer_in_mutex, NULL);

	ctx->config.gross_host.sin_family = AF_INET;
	inet_pton(AF_INET, dconf(config, "host", "127.0.0.1"),
		  &(ctx->config.gross_host.sin_addr));

	ctx->config.sync_host.sin_family = AF_INET;
	inet_pton(AF_INET, dconf(config, "synchost", "127.0.0.1"),
		  &(ctx->config.sync_host.sin_addr));

	ctx->config.sync_host.sin_port =
		htons(atoi(dconf(config, "syncport", "1112")));
	ctx->config.gross_host.sin_port =
		htons(atoi(dconf(config, "port", "1111")));
	ctx->config.max_connq = 50;
	ctx->config.max_threads = 10;
	ctx->config.peer.connected = 0;

	ctx->config.peer.peer_addr.sin_family = AF_INET;
	inet_pton(AF_INET, dconf(config, "peerhost", "127.0.0.1"),
		&(ctx->config.peer.peer_addr.sin_addr));

	ctx->config.peer.peer_addr.sin_port = htons(atoi(dconf(config, "peerport", "1112")));

	if (strncmp(dconf(config, "peerhost", ""), "", 1) == 0) {
	  logstr(LOG_INFO, "No peerhost configured. Replication suppressed.");
	  ctx->config.flags |= FLG_NOREPLICATE;	  
	} else {
	  logstr(LOG_INFO, "Peerhost %s configured. Replicating.", dconf(config, "peerhost", ""));
	}

	updatestr = dconf(config, "update", "grey");
	if (strncmp(updatestr, "always", 7) == 0) {
		logstr(LOG_INFO, "updatestyle: ALWAYS");
		ctx->config.flags |= FLG_UPDATE_ALWAYS;
	} else {
		logstr(LOG_INFO, "updatestyle: GREY");
	}

	ctx->config.status_host.sin_family = AF_INET;
	inet_pton(AF_INET, dconf(config, "status_host", "127.0.0.1"),
		  &(ctx->config.status_host.sin_addr));

	ctx->config.status_host.sin_port =
		htons(atoi(dconf(config, "status_port", "1121")));

	ctx->config.rotate_interval = atoi(dconf(config, "rotate_interval", "3600"));
	ctx->config.filter_size = atoi(dconf(config, "filter_bits", "22"));
	ctx->config.num_bufs = atoi(dconf(config, "number_buffers", "8"));

	tmp = dconf(config, "statefile", NULL);
	if (tmp)
		ctx->config.statefile = strdup(tmp);
	else
		ctx->config.statefile = NULL;

	if ((ctx->config.filter_size<5) || (ctx->config.filter_size>32)) {
	  daemon_shutdown(1, "filter_bits should be in range [4,32]");
	}

	ctx->config.acctmask = 0x003f;
	ctx->config.loglevel = LOGLEVEL;

	*(ctx->last_rotate) = time(NULL);

#ifdef DNSBL
	ctx->dnsbl = NULL;

	cp = config;
	while (cp) {
		if (strcmp(cp->name, "dnsbl") == 0)
			add_dnsbl(&ctx->dnsbl, cp->value, 1);
		cp = cp->next;
	}
#endif /* DNSBL */

	cp = config;
	while (cp) {
	  configlist_t* next = cp->next;
	  free((char *)cp->name);
	  free((char *)cp->value);
	  free(cp);
	  cp = next;
	}
}

/* 
 * mrproper	- tidy upon exit
 */
void
mrproper(int signo)
{
  static int cleanup_in_progress = 0;

  if (cleanup_in_progress)
    raise(signo);

  cleanup_in_progress = 1;
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  raise(signo);
}

void
usage(void)
{
	printf("Usage: grossd [-d] [-r] [-f configfile]\n");
	printf("       -d	Run grossd as a foreground process.\n");
	printf("       -f	override default configfile\n");
	printf("       -r	disable replication\n");
	printf("       -V	version information\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ret;
	char ipstr[INET_ADDRSTRLEN];
	int cont = 1;
	update_message_t rotatecmd;
	time_t toleration;
	configlist_t *config;
	char *configfile = CONFIGFILE;
	extern char *optarg;
	extern int optind, optopt;
	int c;

	/* mind the signals */
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, &mrproper);
	signal(SIGINT, &mrproper);

	ctx = initialize_context();

	if ( ! ctx ) {
		fprintf(stderr, "Couldn't initialize context\n");
		return 1;
	}

	/* command line arguments */
	while ((c = getopt(argc, argv, ":drf:V")) != -1) {
		switch (c) {
		case 'd':
			ctx->config.flags |= FLG_NODAEMON;
			break;
		case 'f':
			configfile = optarg;
			break;
		case ':':
			fprintf(stderr,
				"Option -%c requires an operand\n", optopt);
			usage();
			break;
		case 'r':
                        ctx->config.flags |= FLG_NOREPLICATE;
			break;
		case 'V':
                        printf("grossd - Greylisting of Suspicious Sources. Version %s.\n", VERSION);
			exit(0);
                        break;
		case '?':
			fprintf(stderr,
				"Unrecongnized option: -%c\n", optopt);
			usage();
			break;
		}
	}

	/* daemonize must be run before any pthread_create */
        if ((ctx->config.flags & FLG_NODAEMON) == 0) {
		daemonize();
		openlog("grossd", 0x00, LOG_MAIL);
	}

	config = read_config(configfile);
	configure_grossd(config); 		/* implicit pthread_create */

	/* start the bloom manager thread */
	bloommgr_init();

	if ( (ctx->config.flags & FLG_NOREPLICATE) == 0) {
		syncmgr_init();
	}

	WITH_SYNC_GUARD(logstr(GLOG_INFO, "Filters in sync. Starting..."););
	
	/*
	 * now that we are in synchronized state we can start listening
	 * for client requests
	 *
	 */

	/* start the worker thread */
	worker_init();

	/* start the server status thread */
	srvstatus_init();


	/*
	 * run some periodic maintenance tasks
	 */
	toleration = time(NULL);
	for ( ; ; ) {
		if ((time(NULL) - *ctx->last_rotate) > ctx->config.rotate_interval) {
			/* time to rotate filters */
			rotatecmd.mtype = ROTATE;
			ret = instant_msg(ctx->update_q, &rotatecmd, 0, 0);
			if (ret < 0)
				perror("rotate put_msg");
		}

#ifdef DNSBL
		if (time(NULL) >= toleration + 10) {
			toleration = time(NULL);
			increment_dnsbl_tolerance_counters(ctx->dnsbl);
		}
#endif /* DNSBL */

		/* not so busy loop */
		sleep(1);
	}
}