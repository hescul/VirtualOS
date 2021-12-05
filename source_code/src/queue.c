#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->head == q->tail);
}

int full(struct queue_t* q) {
	return (q->tail + 1) % (MAX_QUEUE_SIZE + 1) == q->head;
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* put a new process to queue q in priority order */
	if (!full(q)) {			// only enqueue on non-full
		int i = q->head;	// search through the queue
		while (i != q->tail && q->proc[i]->priority >= proc->priority)
			i = (i + 1) % (MAX_QUEUE_SIZE + 1);	// find suitable slot
		if (i == q->tail)	// easy when add to the back of the queue
			q->proc[i] = proc;
		else {				// otherwise, we sift down the elements
			while (i != q->tail) {
				// swap in each iteration
				struct pcb_t* tmp = q->proc[i];
				q->proc[i] = proc;
				proc = tmp;
				i = (i + 1) % (MAX_QUEUE_SIZE + 1);
				// place the last element before exiting the loop
				if (i == q->tail)
					q->proc[i] = proc;
			}
		}
		q->tail = (q->tail + 1) % (MAX_QUEUE_SIZE + 1);	// update tail
	}
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* return a pcb whose priority is the highest */
	if (empty(q))	// only dequeue on non-empty
		return NULL;
	struct pcb_t* proc = q->proc[q->head];			// get the target process
	q->head = (q->head + 1) % (MAX_QUEUE_SIZE + 1);	// update head
	return proc;
}

