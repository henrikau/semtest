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

void show_help(void)
{
	printf("Usage:\n"
		   "semtest <options>\n\n"
		   "-a       --affinity      bind tasks to cores explicitly\n"
		   "-A       --no_affinity   do not set affinity, let the scheduler move tasks\n"
		   "-i ITER  --iterations    run threads for ITER iterations\n"
		   "-I INTER --interval      time between each semaphore-iteration\n"
		   "-F       --feather       let each task-pair have a cpu-pair exclusively\n"
		   "-g       --graph         dump output eatable by a graphing tool\n"
		   "-G       --group         Group semaphore master and slave on same core\n"
		   "-h       --help          show this help\n"
		   "-n CPUS  --num_cpus      test on number of cores\n"
		   "-N CPUS  --ignore_cpus   ignore the (commaseparated) list of cpus, 0-indexed\n"
		   "-p PRIO  --priority      run with given (real-time) priority\n"
		   "-P       --pid           print pid (or tid) of all marco-polo pair\n"
		   "-r       --policy_rr     use SCHED_RR instead of the default SCHED_FIFO for rt-priorities\n"
		   "-t LIMIT --event_trace   enable event-tracing when running, tag occurences exceeding LIMIT us\n"
		   "-q		 --quiet	     limit output to summary only\n"
		   "\n"
		);
}

void do_options(int argc, char *argv[], struct sem_test *st)
{
	int err = 0;
	int tmp;
	for (;;) {
		int optidx = 0;
		static struct option long_opts[] = {
			{ "affinity",		optional_argument, NULL, 'a'},
			{ "no_affinity",	optional_argument, NULL, 'A'},
			{ "iterations", 	optional_argument, NULL, 'i'},
			{ "interval",		optional_argument, NULL, 'I'},
			{ "feather",        optional_argument, NULL, 'F'},
			{ "graph",          optional_argument, NULL, 'g'},
			{ "group",          optional_argument, NULL, 'G'},
			{ "help",			optional_argument, NULL, 'h'},
			{ "num_cpus",		optional_argument, NULL, 'n'},
			{ "ignore_cpus",    optional_argument, NULL, 'N'},
			{ "priority",		optional_argument, NULL, 'p'},
			{ "pid",            optional_argument, NULL, 'P'},
			{ "policy_rr",		optional_argument, NULL, 'r'},
			{ "event_trace",    optional_argument, NULL, 't'},
			{ "quiet",			optional_argument, NULL, 'q'},
			{ NULL, 0, NULL, 0}
		};
		int c = getopt_long(argc, argv, "aAFgGhi:I:n:N:p:Prt:q", long_opts, &optidx);
		if (c == -1 || err)
			break;
		switch(c) {
		case 'a':
			st_set_affinity(st);
			break;
		case 'A':
			st_clear_affinity(st);
			break;
		case 'F':
			st_enable_feather(st);
			break;
		case 'h':
			show_help();
			exit(0);
		case 'i':
			tmp = atoi(optarg);
			if (!tmp || tmp < 0)
				err = 1;
			st_set_iters(st, tmp);
			break;
		case 'I':
			tmp = atoi(optarg);
			if (tmp && tmp > 0)
				st_set_interval(st, tmp);
			break;
		case 'g':
			set_graph_output(st);
			break;
		case 'G':
			set_grouped_mode(st);
			break;
		case 'n':
			tmp = atoi(optarg);
			if (!tmp || tmp < 0)
				err = 1;
			st_set_max_cpus(st, tmp);
			break;
		case 'N':
			st_clear_cpu(st, atoi(optarg));
			break;
		case 'p':
			tmp = atoi(optarg);
			if (!tmp)
				err = 1;
			else
				st_set_pri(st, tmp);
			break;
		case 'P':
			st_print_pids(st);
			break;
		case 'r':
			st_set_policy(st, SCHED_RR);
			break;
		case 't':
			tmp = atoi(optarg);
			if (tmp > 0) {
				enable_tracing(st, tmp);
			}
			break;
		case 'q':
			st_set_quiet(st);
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
}

int main(int argc, char *argv[])
{
	struct sem_test *st = create_sem_test(sysconf(_SC_NPROCESSORS_ONLN));
	if (!st) {
		perror("Could not create st\n");
		return -1;
	}

	do_options(argc, argv, st);

	/* printf("Starting test:\n" */
	/* 	   "Affinity:\t%s\n" */
	/* 	   "Iterations:\t%d\n" */
	/* 	   "num_cpus:\t%d\n" */
	/* 	   "Priority:\t%d\n" */
	/* 	   "Policy:\t\t%s\n" */
	/* 	   "Tracing:\t(%s) %ld\n", */
	/* 	   (force_affinity ? "On" : "Off"), */
	/* 	   iters, */
	/* 	   num_cpus, */
	/* 	   prio, */
	/* 	   (prio ? (policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_RR") : "SCHED_OTHER"), */
	/* 	   (trace_limit == 0 ? "On" : "Off"), */
	/* 	   trace_limit); */


	run_test(st);

	print_summary(st);
	return 0;
}
