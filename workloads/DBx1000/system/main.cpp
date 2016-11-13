#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "global.h"
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include "ycsb.h"
#include "tpcc.h"
#include "test.h"
#include "thread.h"
#include "manager.h"
#include "mem_alloc.h"
#include "query.h"
#include "plock.h"
#include "occ.h"
#include "vll.h"

#ifdef GEM5
#include "m5op.h"
#endif


void * f(void *);

// TODO the following global variables are HACK
thread_t * m_thds;

// defined in parser.cpp
void parser(int argc, char * argv[]);

int main(int argc, char* argv[])
{
	setbuf(stdout, NULL);

	// 0. initialize global data structure
	parser(argc, argv);

#if (BTREE_PIM)	
	// open PIMBT device
	g_pimbt_dev = open("/dev/pimbt", O_RDWR);

    if (g_pimbt_dev<0)
		printf("Error opening file \n");

	g_pim_register = (char *)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED,
										g_pimbt_dev, 0);
	if (g_pim_register == MAP_FAILED)
		printf ("Map PIM register failed\n");
#endif

	// per-partition malloc
	mem_allocator.init(g_part_cnt, MEM_SIZE / g_part_cnt); 
	stats.init();
	glob_manager.init();
	if (g_cc_alg == DL_DETECT) 
		dl_detector.init();
	printf("mem_allocator initialized!\n");
	workload * m_wl;
	switch (WORKLOAD) {
		case YCSB :
			m_wl = new ycsb_wl; break;
		case TPCC :
			m_wl = new tpcc_wl; break;
		case TEST :
			m_wl = new TestWorkload; 
			((TestWorkload *)m_wl)->tick();
			break;
		default:
			assert(false);
	}

	int64_t mystarttime = get_server_clock();
	m_wl->init();
	printf("workload initialized!\n");
	// 2. spawn multiple threads
	uint64_t thd_cnt = g_thread_cnt;
	
	pthread_t * p_thds = 
		(pthread_t *) malloc(sizeof(pthread_t) * (thd_cnt - 1));
	m_thds = new thread_t[thd_cnt];
	// query_queue should be the last one to be initialized!!!
	// because it collects txn latency
	if (WORKLOAD != TEST)
		query_queue.init(m_wl);
	pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );
	printf("query_queue initialized!\n");
#if CC_ALG == HSTORE
	part_lock_man.init();
#elif CC_ALG == OCC
	occ_man.init();
#elif CC_ALG == VLL
	vll_man.init();
#endif

	for (uint32_t i = 0; i < thd_cnt; i++) 
		m_thds[i].init(i, m_wl);

#if defined(GEM5)
	m5_checkpoint(0, 0);
#endif

#if (MODEL_PIM_PTABLE)
	grab_pim_table();
#endif

	if (WARMUP > 0){
		printf("WARMUP start!\n");
		int64_t myendtime = get_server_clock();
		printf("SimTime = %ld\n", myendtime - mystarttime);
		for (uint32_t i = 0; i < thd_cnt - 1; i++) {
			uint64_t vid = i;
			pthread_create(&p_thds[i], NULL, f, (void *)vid);
		}
		f((void *)(thd_cnt - 1));
		for (uint32_t i = 0; i < thd_cnt - 1; i++)
			pthread_join(p_thds[i], NULL);
		printf("WARMUP finished!\n");
	}
	warmup_finish = true;
	pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );
#ifndef NOGRAPHITE
	CarbonBarrierInit(&enable_barrier, g_thread_cnt);
#endif
	pthread_barrier_init( &warmup_bar, NULL, g_thread_cnt );

#if defined(GEM5)
	m5_reset_stats(0, 0);
#endif	
	
	// spawn and run txns again.
	int64_t starttime = get_server_clock();
	cpu_set_t cpuset;
	for (uint32_t i = 0; i < thd_cnt - 1; i++) {
		uint64_t vid = i;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		CPU_ZERO(&cpuset);
		CPU_SET(i + 1, &cpuset);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
		pthread_create(&p_thds[i], &attr, f, (void *)vid);
		pthread_setaffinity_np(p_thds[i], sizeof(cpu_set_t), &cpuset);
	}
	// set ourself to CPU 0
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	int s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	f((void *)(thd_cnt - 1));
	for (uint32_t i = 0; i < thd_cnt - 1; i++) 
		pthread_join(p_thds[i], NULL);
	int64_t endtime = get_server_clock();

#if defined(GEM5)
	m5_dump_stats(0, 0);
#endif	
	
	if (WORKLOAD != TEST) {
		printf("PASS! SimTime = %ld, s = %d\n", endtime - starttime, s);
		if (STATS_ENABLE)
			stats.print();
	} else {
		((TestWorkload *)m_wl)->summarize();
	}
	
	fflush(stdout);
	
	for (uint32_t i = 0; i < 100000; i++);

#if defined(GEM5)
	m5_exit(0);
#endif	

	return 0;
}

void * f(void * id) {
	uint64_t tid = (uint64_t)id;
	m_thds[tid].run();
	return NULL;
}
