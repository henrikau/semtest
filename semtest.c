/* semtest.c
 *
 * Copyright (C) 2013 Cisco Systems, Henrik Austad <haustad@cisco.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <getopt.h>
#include <linux/unistd.h>

#define gettid() syscall(__NR_gettid)

/* TODO: clean up this mess, reorganize what is stored where (and why) */
struct sem_pair {
	struct sem_test *st;

	int idmarco;					/* CPU of sender */
	int idpolo;					/* CPU of replier */
	sem_t polo;
	sem_t marco;
	pthread_t tpolo;
	pthread_t tmarco;
	pid_t pmarco;
	pid_t ppolo;

	uint64_t start_us;
	uint64_t stop_us;
	unsigned long long max_us;
	unsigned long long min_us;
	uint64_t sum_us;
	uint64_t ctr;
};

struct sem_test {
	unsigned int num_cpus;
	unsigned int iters;
	int policy;
	int pri;
	unsigned long long start;
	unsigned long long end;
	unsigned char force_affinity;
	unsigned long long cpumask;

	unsigned long trace_limit_us;
	unsigned char trace_on;
	struct sem_pair sp[0];
};

static float _find_middle(unsigned long long *list, int size);
static void _init_sem_pair(struct sem_pair *sp);
static uint64_t _now64_us(void);
static int * _init_cpuidx(struct sem_test *st);
static void _set_affinity(int cpu);
static void _set_priority(int prio, int policy);
static char * _pretty_print_time_us(unsigned long long diff);

/* thread handlers */
void * polo(void *data);
void * marco(void *data);

struct sem_test * create_sem_test(uint32_t num_cpus,
								  int policy)
{
	struct sem_test *st;
	int i;
	if (!num_cpus)
		return NULL;
	st = calloc(1, sizeof(*st) + sizeof(struct sem_pair) * num_cpus);
	if (!st) {
		perror("create_sem_test() Could not allocate memory for sem_test");
		return NULL;
	}

	/* set default values */
	st->num_cpus = num_cpus;
	st->trace_on = 0;
	st->trace_limit_us = -1;
	st->policy = SCHED_OTHER;
	st->pri = 0;
	st->force_affinity = 1;
	st->iters = 10000;
	st->cpumask = -1;
	for (i=0;i<st->num_cpus;i++) {
		st->sp[i].st = st;
		_init_sem_pair(&st->sp[i]);
	}
	return st;
}

void st_set_affinity(struct sem_test *st)
{
	if (!st)
		return;
	st->force_affinity = 1;
}
void st_clear_affinity(struct sem_test *st)
{
	if (!st)
		return;
	st->force_affinity = 0;
}

void st_set_pri(struct sem_test *st, int pri)
{
	if (!st)
		return;
	st->pri = pri;
}

void st_set_policy(struct sem_test *st, int policy)
{
	if (!st)
		return;
	st->policy = policy;
}
void st_set_iters(struct sem_test *st, int iters)
{
	if (!st)
		return;
	st->iters = iters;
}

void st_clear_cpu(struct sem_test *st, int cpu)
{
	if (!st)
		return;
	if (cpu >= 0 && cpu < st->num_cpus)
		st->cpumask &= ~(1<<cpu);
}

void st_set_max_cpus(struct sem_test *st, int max_cpus)
{
	if (!st)
		return;
	st->num_cpus = max_cpus;
}
void free_sem_test(struct sem_test *st)
{
	if (st) {
		free(st);
	}
}

void enable_tracing(struct sem_test *st, signed long trace_limit_us)
{
	if (!st)
		return;
	/* if not mounted, throw error, we don't want to mount this from inside the application */
	st->trace_limit_us = trace_limit_us;
	st->trace_on = 1;
}

void run_test(struct sem_test *st)
{
	int c = 0;
	int * cpuidx = NULL;
	if (!st)
		return;

	/* final preparations */
	if (st->force_affinity) {
		cpuidx = _init_cpuidx(st);
	}


	for (c=0;c<st->num_cpus;c++) {
		if (!(st->cpumask & (1<<c)))
			continue;
		st->sp[c].ctr = st->iters;
		if (st->force_affinity && cpuidx) {
			st->sp[c].idmarco = c;
			st->sp[c].idpolo = cpuidx[c];
		}
	}

	/* run */
	st->start = _now64_us();
	for (c=0;c<st->num_cpus;c++) {
		if (!(st->cpumask & (1<<c)))
			continue;
		pthread_create(&st->sp[c].tpolo, NULL,  polo, (void *)&st->sp[c]);
		pthread_create(&st->sp[c].tmarco, NULL, marco, (void *)&st->sp[c]);
	}
	/* join */
	for (c=0;c<st->num_cpus;c++) {
		if (!(st->cpumask & (1<<c)))
			continue;
		pthread_join(st->sp[c].tpolo, NULL);
		pthread_join(st->sp[c].tmarco, NULL);
	}
	st->end = _now64_us();
}

void print_summary(struct sem_test * st)
{
	int c = 0;
	unsigned long long max_us = 0;
	unsigned long long min_us = 0;
	unsigned long long max_us_sum = 0;
	unsigned long long min_us_sum = 0;
	unsigned long long *max_us_list = calloc(st->num_cpus, sizeof(unsigned long long));
	unsigned long long *min_us_list = calloc(st->num_cpus, sizeof(unsigned long long));

	printf("Summary of %d iterations\n", st->iters);
	for (;c<st->num_cpus;c++) {
		float tavg = 0.0f;
		if (!(st->cpumask & (1<<c)))
			continue;
		tavg = (float)st->sp[c].sum_us *1.0f / st->iters;
		printf("P: %2d,%2d\tPri: %d\tMax:\t%8llu\tMin:\t%8llu\tAvg:\t%8.4f\n",
			   st->sp[c].idmarco,
			   st->sp[c].idpolo,
			   st->pri,
			   st->sp[c].max_us,
			   st->sp[c].min_us,
			   tavg);
		max_us_sum += st->sp[c].max_us;
		min_us_sum += st->sp[c].min_us;
		max_us_list[c] = st->sp[c].max_us;
		min_us_list[c] = st->sp[c].min_us;

		if (!max_us || max_us < st->sp[c].max_us) {
			max_us = st->sp[c].max_us;
		}
		if (!min_us || min_us < st->sp[c].min_us) {
			min_us = st->sp[c].min_us;
		}
	}

	printf("(Max) Global: %12llu us\avg: %.4f us\tmiddle: %f\n",
		   max_us, (float)max_us_sum/st->num_cpus, _find_middle(max_us_list, st->num_cpus));
	printf("(Min) Global: %12llu us\avg: %.4f us\tmiddle: %f\n",
		   min_us, (float)min_us_sum/st->num_cpus, _find_middle(min_us_list, st->num_cpus));
	/* fixme memleak from pretty_print */
	printf("Test took %s to complete\n", _pretty_print_time_us( (unsigned long long)(st->end - st->start)));
}


static float _find_middle(unsigned long long *list, int size)
{
	int i,j;
	unsigned long long tmp;
	float middle = 0.0f;
	if (!list)
		return 0;				/* if list is NULL, all bets are off */
	if (size < 0)
		return list[0];

	/* sort, use simple bubblesort, array should be fairly short (famous last words..) */
	for(i=0;i<size;i++) {
		for(j=i;j<size;j++) {
			if (list[j] < list[i]) {
				tmp = list[i];
				list[i] = list[j];
				list[j] = tmp;
			}
		}
	}
	i = size/2;
	middle = (float)list[i];
	if (!(size%2) && size > 0) {
		middle += (float)list[i-1];
		middle /= 2;
	}
	/* get middle */
	return middle;
}

static void _init_sem_pair(struct sem_pair *sp)
{
	if (!sp) {
		return;
	}
	sem_init(&sp->marco, 0, 0);
	sem_init(&sp->polo, 0, 0);
	sp->idmarco = -1;
	sp->idpolo = -1;
	sp->min_us = -1;
	sp->max_us = 0;
}

static uint64_t _now64_us(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000000 + now.tv_nsec / 1000;
}


static int * _init_cpuidx(struct sem_test *st)
{
	int * cpuidx = NULL;
	int t,i1, i2, c;

	if (!st)
		return NULL;

	cpuidx = calloc(st->num_cpus, sizeof(*cpuidx));
	if (!cpuidx)
		return NULL;

	srand(time(NULL));
	for (c = 0; c<st->num_cpus; c++) {
		if (!(st->cpumask & (1<<c)))
			continue;
		cpuidx[c] = c;
	}

	for (c = 0; c<1000; c++) {
		i1 = rand()%(st->num_cpus);
		i2 = rand()%(st->num_cpus);

		if (!(st->cpumask & (1<<i1)))
			continue;
		if (!(st->cpumask & (1<<i2)))
			continue;

		t = cpuidx[i1];
		cpuidx[i1] = cpuidx[i2];
		cpuidx[i2] = t;
	}
	return cpuidx;
}

static void _set_affinity(int cpu)
{
	cpu_set_t mask;
	pthread_t thread = pthread_self();
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	if (pthread_setaffinity_np(thread, sizeof(mask), &mask) == -1) {
		fprintf(stderr, "Could not set affinity for thread %ld, will ignore to avoid deadlock\n", gettid());
	}
}

static void _set_priority(int prio, int policy)
{
	int err;
	if (policy != SCHED_RR && policy != SCHED_FIFO)
		return;
	struct sched_param param;

	param.sched_priority = prio;
	err = sched_setscheduler(0, policy, &param);
	if (err) {
		fprintf(stderr, "Could not set priority for %ld\n", gettid());
		perror(NULL);
	}
}

static char * _pretty_print_time_us(unsigned long long diff)
{
	unsigned long long us;
	unsigned long long secs;
	unsigned long long mins;
	unsigned long long hours;

	char *buff = calloc(512, sizeof(*buff));
	if (!buff)
		return NULL;
	secs = diff / 1000000;
	us = diff - secs*1000000;

	mins = secs / 60;
	secs -= mins*60;

	hours = mins / 60;
	mins -= hours*60;

	sprintf(buff, "%llu hours %llu mins %llu.%llu secs", hours, mins, secs, us);
	return buff;
}


void * marco(void *data)
{
	struct sem_pair *pair = (struct sem_pair *)data;
	uint64_t start,stop,diff;
	unsigned long long tlus = 0;
	if (!pair)
		return NULL;

	if (pair->st->trace_on) {
		tlus = pair->st->trace_limit_us;
	}
	if (pair->idmarco > -1) {
		_set_affinity(pair->idmarco);
	}
	_set_priority(pair->st->pri, pair->st->policy);
	pair->pmarco = gettid();
	pair->start_us = _now64_us();
	while(pair->ctr > 0) {
		/* printf("[SENDER >] signaling reader, waiting for reply\n"); */
		start = _now64_us();
		sem_post(&pair->polo);
		/* printf("[SENDER >] message sent, awaiting reply eagerly\n"); */
		sem_wait(&pair->marco);
		/* printf("[SENDER >] Got msg, do the accounting\n"); */
		stop = _now64_us();
		diff = stop-start;

		if (diff > pair->max_us)
			pair->max_us = diff;
		if (diff < pair->min_us)
			pair->min_us = diff;
		if (diff > tlus) {
			FILE *fd = NULL;
			fd = fopen("/sys/kernel/debug/tracing/trace_marker", "w+");
			if (fd) {
				fprintf(fd, "semtest latency (%llu) exceeded target latency (%llu), P: %d,%d, pid:%d,%d\n",
						(unsigned long long)diff, tlus,pair->idmarco, pair->idpolo, pair->pmarco, pair->ppolo);
				fclose(fd);
			}
		}
		pair->sum_us+=diff;
		pair->ctr--;
		usleep(1000);
	}
	sem_post(&pair->polo);
	pair->stop_us = _now64_us();
	return NULL;
}

void * polo(void *data)
{
	struct sem_pair *pair = (struct sem_pair *)data;

	if (pair->idpolo > -1) {
		_set_affinity(pair->idpolo);
	}
	_set_priority(pair->st->pri, pair->st->policy);

	/* task affinity */
	if (!pair)
		return NULL;
	pair->ppolo = gettid();
	while (pair->ctr > 0) {
		sem_wait(&pair->polo);
		sem_post(&pair->marco);
	}
	return NULL;
}

