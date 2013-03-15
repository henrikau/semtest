/* semtest, main routine
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
#include <unistd.h>
#include <getopt.h>
#include "semtest.h"

static unsigned char force_affinity = 1;
static unsigned int num_cpus = 0;
static int policy = SCHED_FIFO;
static int prio = 0;
static unsigned int iters = 10000; /* 10k */
static signed long trace_limit = 0;

void show_help(void)
{
	printf("Usage:\n"
		   "semtest <options>\n\n"
		   "-a       --affinity      bind tasks to cores explicitly\n"
		   "-A       --no_affinity   do not set affinity, let the scheduler move tasks\n"
		   "-i ITER  --iterations    run threads for ITER iterations\n"
		   "-h       --help          show this help\n"
		   "-n CPUS  --num_cpus      test on number of cores\n"
		   "-p PRIO  --priority      run with given (real-time) priority\n"
		   "-r       --policy_rr     use SCHED_RR instead of the default SCHED_FIFO for rt-priorities\n"
		   "-t LIMIT --event_trace   enable event-tracing when running, tag occurences exceeding LIMIT us\n"
		   "\n"
		);
}

void do_options(int argc, char *argv[])
{
	int err = 0;
	for (;;) {
		int optidx = 0;
		static struct option long_opts[] = {
			{ "affinity",		optional_argument, NULL, 'a'},
			{ "no_affinity",	optional_argument, NULL, 'A'},
			{ "iterations", 	optional_argument, NULL, 'i'},
			{ "help",			optional_argument, NULL, 'h'},
			{ "num_cpus",		optional_argument, NULL, 'n'},
			{ "priority",		optional_argument, NULL, 'p'},
			{ "policy_rr",		optional_argument, NULL, 'r'},
			{ "event_trace",    optional_argument, NULL, 't'},
			{ NULL, 0, NULL, 0}
		};
		int c = getopt_long(argc, argv, "aAhi:n:p:rt:", long_opts, &optidx);
		if (c == -1 || err)
			break;
		switch(c) {
		case 'a':
			force_affinity = 1;
			break;
		case 'A':
			force_affinity = 0;
			break;
		case 'h':
			show_help();
			exit(0);
		case 'i':
			iters = atoi(optarg);
			if (!iters || iters < 0)
				err = 1;
			break;
		case 'n':
			num_cpus = atoi(optarg);
			if (!num_cpus || num_cpus < 0)
				err = 1;
			break;
		case 'p':
			prio = atoi(optarg);
			if (policy != SCHED_FIFO || policy != SCHED_RR)
				policy = SCHED_FIFO;
			if (!prio)
				err = 1;
			break;
		case 'r':
			policy = SCHED_RR;
			break;
		case 't':
			trace_limit = atoi(optarg);
			if (trace_limit <= 0) {
				trace_limit = 0;
				err = 1;
			}
			break;
		default:
			/* fixme; error */
			break;
		}
	}
	if (err) {
		fprintf(stderr, "Error, check params\n");
		show_help();
		exit(1);
	}
	if (!prio)
		policy = SCHED_OTHER;
}

int main(int argc, char *argv[])
{
	num_cpus =  sysconf(_SC_NPROCESSORS_ONLN);
	do_options(argc, argv);
	printf("Starting test:\n"
		   "Affinity:\t%s\n"
		   "Iterations:\t%d\n"
		   "num_cpus:\t%d\n"
		   "Priority:\t%d\n"
		   "Policy:\t\t%s\n"
		   "Tracing:\t(%s) %ld\n",
		   (force_affinity ? "On" : "Off"),
		   iters,
		   num_cpus,
		   prio,
		   (prio ? (policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_RR") : "SCHED_OTHER"),
		   (trace_limit == 0 ? "On" : "Off"),
		   trace_limit);

	struct sem_test *st = create_sem_test(num_cpus, force_affinity, policy, prio);

	if (trace_limit) {
		enable_tracing(st, trace_limit);
	}
	if (st) {
		run_test(st, iters);
	}

	print_summary(st);
	return 0;
}
