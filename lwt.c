#include "lwt.h"

void __lwt_dispatch(lwt_t n, lwt_t c);

void enqueue(lwt_t new) 
{
	new->pre = tail_tcb;
	tail_tcb->next = new;	
	tail_tcb = new;
} 
int lwt_id(lwt_t spe_tcb)
{
	return spe_tcb->id;	
}

lwt_t lwt_current(void)
{
	if (head_tcb == NULL || current_tcb == NULL)
	{
		head_tcb = (lwt_t)malloc(sizeof(lwt_tcb));
		current_tcb = head_tcb;
		current_tcb->state = _TCB_ACTIVE;
		LWT_INFO_NTHD_RUNNABLE++;
		tail_tcb = current_tcb;
	}
	return current_tcb;	
}

void __lwt_schedule(void)
{
	//prevent self dispatch to thread itself without main thread
	lwt_t temp;
	int a = lwt_info(LWT_INFO_NTHD_RUNNABLE);
        //printf("RUNNABLE:%d\n",a);

	//prevent directly yield()
	if (lwt_info(LWT_INFO_NTHD_RUNNABLE) == 0)
	{
		return;
	}

	//get next _TCB_ACTIVE
	if (current_tcb->next != NULL) {		
		temp = current_tcb->next;
	} else { 
		temp = head_tcb;
	}
	
	while (temp->state != _TCB_ACTIVE)
	{
		if (temp->next != NULL) {
			temp = temp->next;
		} else {
			temp = head_tcb;
		}
	}
	if (temp != current_tcb)
	{
		//printf("/////Yielding to %d\n", temp->id);
		des_tcb = temp;
		__lwt_dispatch(temp, current_tcb);
	}
	return;
}

int lwt_yield(lwt_t destination)
{
	if (destination == NULL)
		__lwt_schedule();
	else if(destination->state == _TCB_ACTIVE)
	{
		des_tcb = destination;
		__lwt_dispatch(destination, current_tcb);
	}
	return -1;
}

void __lwt_dispatch(lwt_t n, lwt_t c)	//next, current
{
	__asm__ __volatile__(	
		"pushal\n\t" 						//PUSH ALL other register
		"movl $1f, (%%eax)\n\t" 			//save IP to TCB
		"movl %%esp, (%%ebx)\n\t"			//save SP to TCB
		"movl %%edx, %%esp\n\t"				//recover the SP
		"jmp %%ecx\n\t"						//recover the IP
		"1:"							//LABEL
		"popal"
		:: "a"(&c->p_ins), "b"(&c->p_stack), "c"(n->p_ins), "d"(n->p_stack)
		: "cc", "memory"
	);
	//for switching back to old thread, nested?
	current_tcb = des_tcb;
	return;	
}
void lwt_die(void *data)
{
	//seriously, prevent when joining(A), the finish of B changes the state of main thread
	//if (current_tcb == current_tcb->target)
	if (current_tcb->joining != NULL)
	{
		current_tcb->joining->state = _TCB_ACTIVE;
	}
	//mark its state as DEAD, free the stack when......?
	current_tcb->state = _TCB_DEAD;
	//pass data to TCB
	if (data != NULL)
	{
		current_tcb->ret = (void *)data;
	}

	//if (current_tcb->id == 10008)
	lwt_yield(LWT_NULL);
}

void __lwt_start()//lwt_fn_t fn, void *data)
{
	//printf("---Enter start---\n");
	current_tcb = des_tcb;	
	current_tcb->ret = current_tcb->para_fn(current_tcb->para_data);
	/*
	while (1)
	{
	
	}
	*/
	lwt_die(NULL);//deal with thte normal return;
	//assert(0); //if not dying successfully, quit;
}

void *lwt_join(lwt_t thread)
{
	void *temp; // = thread->ret;
	//illegal join, from non-parent
	if (current_tcb->id != thread->parent_id)
	{
		return NULL;	
	}
	current_tcb->target = thread;
	current_tcb->target->joining = current_tcb;
	//dead before join it
	if (thread->state == _TCB_DEAD)
	{
		LWT_INFO_NTHD_ZOMBIES--;
		LWT_INFO_NTHD_RUNNABLE--;
		//free stack
		free(thread->p_stack_frame);
		thread->pre->next = thread->next;
		//remove TCB from linked list
		if (thread->next == NULL)

			tail_tcb = thread->pre;
		else
			thread->next->pre = thread->pre;
		//free TCB, save ret before it
		temp = thread->ret;
		free(thread);
		return temp;
	}
	//block the parent if the target is not yet finished
	current_tcb->target->joining->state = _TCB_BLOCKED;

	while (current_tcb->target->joining->state == _TCB_BLOCKED)
	{
		LWT_INFO_NTHD_RUNNABLE--;
		LWT_INFO_NTHD_BLOCKED++;
		lwt_yield(LWT_NULL);
	}
	LWT_INFO_NTHD_RUNNABLE++;
	LWT_INFO_NTHD_BLOCKED--;
	//-----
	LWT_INFO_NTHD_RUNNABLE--;
	LWT_INFO_NTHD_ZOMBIES--;
	//free stack
	free(thread->p_stack_frame);
	thread->pre->next = thread->next;
	//remove TCB from linked list
	if (thread->next == NULL)
		tail_tcb = thread->pre;
	else
		thread->next->pre = thread->pre;
	//free TCB
	temp = thread->ret;
	free(thread);
	return temp;
}

lwt_t lwt_create(lwt_fn_t fn, void *data, lwt_flags_t flags)
{
	//if current is NULL, create one for the default thread
	if (head_tcb == NULL)
	{
		head_tcb = (lwt_t)malloc(sizeof(lwt_tcb));
		current_tcb = head_tcb;
		current_tcb->state = _TCB_ACTIVE;
		LWT_INFO_NTHD_RUNNABLE++;
		tail_tcb = current_tcb;
	}
	//malloc the new TCB and stack
	lwt_t new = (lwt_t)malloc(sizeof(lwt_tcb));
	lwt_counter++;
	new->id = lwt_counter;
	new->p_ins = &__lwt_start;
	new->p_stack_frame = malloc(STACK_SIZE); 		//for free stack
	new->p_stack = new->p_stack_frame + STACK_SIZE - 4; //to avoid some unpredicted fault
	new->state = _TCB_ACTIVE;
	new->parent_id = current_tcb->id;
	
	//important! for nested join
	new->joining = head_tcb;
	new->target = head_tcb;

	//pass by variables
	new->para_fn = fn;
	new->para_data = data;

	//added for channel 2: non-joinable
	new->flags = flags;

	LWT_INFO_NTHD_RUNNABLE++;
	LWT_INFO_NTHD_ZOMBIES++;

	enqueue(new);
	des_tcb = new;
	return new;	
}

int lwt_info(lwt_info_t t)
{
	return t;	
}

