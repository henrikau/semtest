/*
 * semtest.h
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
#ifndef _SEMTEST_H
#define _SEMTEST_H
#include <inttypes.h>
#include <pthread.h>

struct semtest;


struct sem_test * create_sem_test(uint32_t num_cpus,
								  uint8_t set_affinity,
								  int policy,
								  int prio);

void free_sem_test(struct sem_test *);

void run_test(struct sem_test *st, uint32_t iters);

void print_summary(struct sem_test * st);

#endif	/* _SEMTEST_H */
