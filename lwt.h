#ifndef LWT_H
	#define LWT_H
#endif

#include <stddef.h>
#include <assert.h>

//#define DEBUG

#ifdef DEBUG
	#define printd(...) printf(__VA_ARGS__)
#else
	#define printd(...)
#endif

#define STACK_SIZE 4096			//16384 in the future
#define u_long unsigned long
#define LWT_NULL NULL

//#define rdtscll(val) __asm__ __volatile__("rdtsc": "=A"(val))

typedef void *(*lwt_fn_t) (void *) ;

typedef int lwt_flags_t;
lwt_flags_t flags;

typedef enum 
{
	_TCB_ACTIVE,
	_TCB_DEAD,
	_TCB_BLOCKED,
	_TCB_WAITING,			//for channel communication, SENDER blocked
	//_TCB_JOINING,
	//_TCB_JOINED
} _TCB_STATE;

typedef struct lwt_tcb
{
	int id;
	u_long p_ins; 			//store destination instructino pointer
	void *p_stack; 			//store destination stack pointer
	void *p_stack_frame; 	//for free() use
	int state;
	void *ret; 				//store the value passed in by lwt_die()
	struct lwt_tcb *pre;
	struct lwt_tcb *next;	//pointer to next
	int parent_id; //track the parent of current thread, prevent illegal join
	struct lwt_tcb *joining; //track the one who joining this
	struct lwt_tcb *target;

	lwt_flags_t flags;
	lwt_fn_t para_fn;
	void *para_data;

}lwt_tcb, *lwt_t;

int lwt_counter;			//auto-increment counter for thread id

lwt_t current_tcb; 			//global variable for current thread;
lwt_t des_tcb;				//temporarily save the next, change current at start
lwt_t head_tcb;				//track the head of linked list, which is the run queue
lwt_t tail_tcb;

typedef int lwt_info_t;

lwt_info_t LWT_INFO_NTHD_RUNNABLE;
lwt_info_t LWT_INFO_NTHD_ZOMBIES;
lwt_info_t LWT_INFO_NTHD_BLOCKED;
lwt_info_t LWT_INFO_NCHAN;
lwt_info_t LWT_INFO_NSNDING;
lwt_info_t LWT_INFO_NRCVING;

//pass by global variable
lwt_fn_t para_fn;			//function pointer
void *para_data;			//passed data

int lwt_id(lwt_t spe_tcb);
lwt_t lwt_current(void);
int lwt_yield(lwt_t destination);
void *lwt_join(lwt_t thread);
void lwt_die(void *data);
lwt_t lwt_create(lwt_fn_t fn, void *data, lwt_flags_t flags);
int lwt_info(lwt_info_t t);

