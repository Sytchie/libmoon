#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <sys/mman.h>

#include <stdint.h>

#define MEMPOOL_CACHE_SIZE 256

struct rte_mempool* init_mem(uint32_t nb_mbuf, uint32_t socket, uint32_t mbuf_size) {
	static volatile uint32_t mbuf_cnt = 0;
	char pool_name[32];
	sprintf(pool_name, "mbuf_pool%d", __sync_fetch_and_add(&mbuf_cnt, 1));
	// rte_mempool_create is apparently not thread-safe :(
	static rte_spinlock_t lock = RTE_SPINLOCK_INITIALIZER;
	rte_spinlock_lock(&lock);
	struct rte_mempool* pool = rte_pktmbuf_pool_create(pool_name, nb_mbuf, MEMPOOL_CACHE_SIZE,
		0, mbuf_size + RTE_PKTMBUF_HEADROOM,
		socket
	);
	rte_spinlock_unlock(&lock);
	if (!pool) {
		printf("Memory allocation failed: %s (%d)\n", rte_strerror(-rte_errno), rte_errno); 
		return 0;
	}
	return pool;
}

struct rte_mbuf* alloc_mbuf(struct rte_mempool* mp) {
	struct rte_mbuf* res = rte_pktmbuf_alloc(mp);
	return res;
}

void alloc_mbufs(struct rte_mempool* mp, struct rte_mbuf* bufs[], uint32_t len, uint16_t pkt_len) {
	// this is essentially rte_pktmbuf_alloc_bulk()
	// but the loop is optimized to directly set the pkt/data len flags as well
	// since most allocs directly do this (packet generators)
	rte_mempool_get_bulk(mp, (void **)bufs, len);
	uint32_t i = 0;
	switch (len % 4) {
		while (i != len) {
			case 0:
				rte_mbuf_refcnt_set(bufs[i], 1);
				rte_pktmbuf_reset(bufs[i]);
				bufs[i]->pkt_len = pkt_len;
				bufs[i]->data_len = pkt_len;
				i++;
				// fall through
			case 3:
				rte_mbuf_refcnt_set(bufs[i], 1);
				rte_pktmbuf_reset(bufs[i]);
				bufs[i]->pkt_len = pkt_len;
				bufs[i]->data_len = pkt_len;
				i++;
				// fall through
			case 2:
				rte_mbuf_refcnt_set(bufs[i], 1);
				rte_pktmbuf_reset(bufs[i]);
				bufs[i]->pkt_len = pkt_len;
				bufs[i]->data_len = pkt_len;
				i++;
				// fall through
			case 1:
				rte_mbuf_refcnt_set(bufs[i], 1);
				rte_pktmbuf_reset(bufs[i]);
				bufs[i]->pkt_len = pkt_len;
				bufs[i]->data_len = pkt_len;
				i++;
		}
	}
}


uint16_t rte_mbuf_refcnt_read_export(struct rte_mbuf* m) {
	return rte_mbuf_refcnt_read(m);
}

uint16_t rte_mbuf_refcnt_update_export(struct rte_mbuf* m, int16_t value) {
	return rte_mbuf_refcnt_update(m, value);
}


void* alloc_huge(size_t size) {
	void* mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	madvise(mem, size, MADV_HUGEPAGE);
	return mem;
}

int free_huge(void* ptr, size_t size) {
	return munmap(ptr, size);
}

// release/acquire semantics might also be useful, should be implemented in C++
void fence() {
	__sync_synchronize();
}

