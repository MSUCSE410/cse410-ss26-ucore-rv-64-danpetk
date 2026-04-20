#include "queue.h"
#include "defs.h"

void init_queue(struct queue *q)
{
	q->front = q->tail = 0;
	q->empty = 1;
}

void push_queue(struct queue *q, int value)
{
	if (!q->empty && q->front == q->tail) {
		panic("queue shouldn't be overflow");
	}
	q->empty = 0;
	q->data[q->tail] = value;
	q->tail = (q->tail + 1) % NPROC;
}

int pop_queue(struct queue *q, struct proc *pool){

	// if (q->empty)
	// 	return -1;
	// int value = q->data[q->front];
	// q->front = (q->front + 1) % NPROC;
	// if (q->front == q->tail)
	// 	q->empty = 1;
	// return value;

	/******** */


	if (q->empty) 
		return -1;

	int i;
	int min_pool_index = -1;
	int min_data_index = -1;
	long long min = -1;

	for (i = q->front; i != q->tail; i = (i + 1) % NPROC) {
		
		int pool_index = q->data[i];
		struct proc* curr_proc = &pool[pool_index];

		
		if (min == -1 || curr_proc->stride < min) {
			min = curr_proc->stride;
			min_pool_index = pool_index;
			min_data_index = i;
		}
	}
	
	// printf("PID: %d, PRIO: %d, STRIDE: %d\n", pool[min_pool_index].pid, pool[min_pool_index].prio, pool[min_pool_index].stride);
	pool[min_pool_index].stride += BIG_STRIDE / pool[min_pool_index].prio;

	// printf("%d\n", pool[min_pool_index].stride);
	// printf("%d, %d\n", q->front, q->tail);


	// Move current fron to where we replace to keep memory contiguous. We dont do fifo anymore so it doesnt matter
	q->data[min_data_index] = q->data[q->front];

	q->front = (q->front + 1) % NPROC;
	if (q->front == q->tail)
		q->empty = 1;

	return min_pool_index;
}
