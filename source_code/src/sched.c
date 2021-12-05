
#include "queue.h"
#include "sched.h"
#include <pthread.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

int queue_empty(void) {
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
	ready_queue.head = 0;
	ready_queue.tail = 0;
	run_queue.head = 0;
	run_queue.tail = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/* get a process from [ready_queue]. If ready queue
	 * is empty, push all processes in [run_queue] back to
	 * [ready_queue] and return the highest priority one.
	 * */
	pthread_mutex_lock(&queue_lock);	// acquire lock
	if (empty(&ready_queue)) {			// if ready_queue is empty
		while (!empty(&run_queue)) {	// fetch all procs from run_queue
			enqueue(&ready_queue, dequeue(&run_queue));
		}
	}
	proc = dequeue(&ready_queue);		// get the desired proc
	pthread_mutex_unlock(&queue_lock);	// release lock
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}

void finish_scheduler(void) {
	pthread_mutex_destroy(&queue_lock);
}

