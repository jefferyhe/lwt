#include "lwt.h"
#include "channel.h"

void enqueue_snd(lwt_chan_t c, clist_t new)
{
	//printf("Entering enqueue!\n");
	//c->snd_cnt++;
	if(c->snd_thds == NULL) {
		c->snd_thds = new;
		tail_snd = new;
		return;
	} else {
		tail_snd->next = new;
		tail_snd = new;
		return;
	}
}

void dequeue_snd(lwt_chan_t c)
{
	if (c->snd_thds == NULL) {
		return;
	}
	if (c->snd_thds->next == NULL) {
		free(c->snd_thds);
		c->snd_thds = NULL;
		tail_snd = NULL;
		return;
	} else {
		clist_t temp = c->snd_thds;
		c->snd_thds = c->snd_thds->next;
		free(temp);
		return;
	}
}

/************************************************************/
/*                   ring_buffer							*/
/************************************************************/
//pass in the data_buffer, and the size
void rb_init(ring_buffer* rb, int size) {
	rb->size = size+1;
	rb->start = 0;
	rb->end =0;
	rb->data = (void **)calloc(rb->size, sizeof(void *));
}

int rbIsFull(ring_buffer* rb) {
    return (rb->end + 1) % rb->size == rb->start; }
 
int rbIsEmpty(ring_buffer* rb) {
    return rb->end == rb->start; }

void rb_add(ring_buffer* rb, void* data) {
    rb->data[rb->end] = data;
    rb->end = (rb->end + 1) % rb->size;
    return;
}

void* rb_get(ring_buffer* rb) {
    void* temp = rb->data[rb->start];
    rb->start = (rb->start + 1) % rb->size;
    return temp;
}

/************************************************************/
/************************************************************/

lwt_chan_t lwt_chan(int sz)
{
	LWT_INFO_NCHAN++;
	lwt_chan_t new = (lwt_chan_t)malloc(sizeof(lwt_channel));
	rb_init(new->data_buffer,sz);
    
	new->snd_cnt = 0;
	//new->id = n_chan++;
	new->snd_thds = NULL;
	new->rcv_blocked = 0;
	new->rcv_thd = NULL;
	new->next = NULL;
	new->pre = NULL;
	new->iscgrp = 0;
	new->cgrp = NULL;
	return new;
}

/*Deallocate the channel only if no threads still
 have references to the channel. Threads have
 references to the channel if they are either the
 receiver or one of the multiple senders. */

void lwt_chan_deref(lwt_chan_t c)
{
	if (c->snd_thds == NULL && c->rcv_thd->state != _TCB_WAITING) {
		free(c); 	//will also set "rcv_thd" as NULL
		return;
	}
	printd("Cannot derefence the channel.\n");
	return;
}

//add a chan to a cgrp's ready queue, ready to receive
void cgrp_ready_add(lwt_cgrp_t cgrp, lwt_chan_t c)
{
  
    if(cgrp->ready == NULL) {
        cgrp->chan = c;
		tail_cgrp_ready = c;
		return;
	} else {
		tail_cgrp_ready->next = c;
		c->pre = tail_cgrp_ready;
		tail_cgrp_ready = c;
		return;
	}
    
}


void cgrp_ready_rem(lwt_cgrp_t cgrp, lwt_chan_t c)
{
	if (c == cgrp->ready)
	{
		if (c->next == NULL) {
			cgrp->ready = NULL; 
			return;
		} 
		else {
			c->next->pre = NULL;
			cgrp->ready = c->next;
			return;
		}
	} else {
		c->pre->next = c->next;
		return;
	}
}     


int lwt_snd(lwt_chan_t c, void *data)
{
	//this channnel has been lwt_chan_deref() before || data is NULL, illegal
	if (c->rcv_thd == NULL || data == NULL) return -1;
	
	// if belong to a group, add myself to the cgrp's ready queue
	if (c->iscgrp) cgrp_ready_add(c->cgrp,c);
	
	//if data_buffer is NOT FULL, put into ring buffer;
	if (!rbIsFull(c->data_buffer)) {
		rb_add(c->data_buffer, data);
		return 0;
	}
	//if data_buffer is FULL
	else {
		//enqueue()
		clist_t new_sender = (clist_t)malloc(sizeof(clist_head));
		new_sender->thd = current_tcb;
		new_sender->data = data;
		new_sender->next = NULL;
		c->snd_cnt++;
		enqueue_snd(c, new_sender);

		//block while it is FULL
		while(rbIsFull(c->data_buffer)) {
			lwt_yield(LWT_NULL);
		}
	}

	//unblocked, because there is space in data_buffer (consumed by receiver)
	//dequeue the first sender in queue, put is data into buffer
	//c->data_buffer->data[start + count] = c->snd_thds->data;
	rb_add(c->data_buffer, data);
	dequeue_snd(c);
	return 0;
}

                         
void *lwt_rcv(lwt_chan_t c)
{
	if (c->rcv_thd == NULL) {
		c->rcv_thd = current_tcb;
	}
	
	//if buffer is NOT EMPTY
	if (!rbIsEmpty(c->data_buffer)) {
		c->rcv_blocked = 0;
		//read from data_buffer
		return rb_get(c->data_buffer);
	}

	//if empty, block rcv, remove c from ready queue
	if (c->cgrp != NULL) {
		if(c->cgrp->ready != NULL) {
			cgrp_ready_rem(c->cgrp,c);
		}
	} 
	c->rcv_blocked = 1;
	while (rbIsEmpty(c->data_buffer)) {
		lwt_yield(LWT_NULL);
	}

	//somebody sent, so it can receive
	return rb_get(c->data_buffer);
}

/* This is equivalent to lwt snd except that a channel
 is sent over the channel (sending is sent over c).
 This is how senders are added to a channel: The thread
 that receives channel c is added to the senders of the
 channel. This is used for reference counting to determine
 when to deallocate the channel. */

int lwt_snd_chan(lwt_chan_t c, lwt_chan_t sending)
{
	//this channnel has been lwt_chan_deref() before || data is NULL, illegal
	if (c->rcv_thd == NULL || sending == NULL) {
		return -1;
	}
	
	// if belong to a group, add myself to the cgrp's ready queue
	if (c->iscgrp) cgrp_ready_add(c->cgrp,c);
	//if data_buffer is NOT FULL, put into ring buffer;
	if (!rbIsFull(c->data_buffer)) {
		rb_add(c->data_buffer, sending);
		return 0;
	}
	//if data_buffer is FULL
	else {
		//enqueue()
		clist_t new_sender = (clist_t)malloc(sizeof(clist_head));
		new_sender->thd = current_tcb;
		new_sender->data = (void*)sending;
		new_sender->next = NULL;
		c->snd_cnt++;
		enqueue_snd(c, new_sender);

		//block while it is FULL
		while(rbIsFull(c->data_buffer)) {
			lwt_yield(LWT_NULL);
		}
	}

	//unblocked, because there is space in data_buffer (consumed by receiver)
	//dequeue the first sender in queue, put is data into buffer
	//c->data_buffer->data[start + count] = c->snd_thds->data;
	sending->rcv_thd = current_tcb; //add current_tcb to the new channel's receiver
	rb_add(c->data_buffer, c->snd_thds->data);
	dequeue_snd(c);
	return 0;
    
}

/* Same as for lwt rcv except a channel is sent over
 the channel. See lwt snd chan for an explanation. */

lwt_chan_t lwt_rcv_chan(lwt_chan_t c)
{
	if (c->rcv_thd == NULL) {
		c->rcv_thd = current_tcb;
	}	

	//if buffer is NOT EMPTY
	if (!rbIsEmpty(c->data_buffer)) {
		c->rcv_blocked = 0;
		//read from data_buffer
		return rb_get(c->data_buffer);
	}
	//if empty, block rcv, remove c from ready queue
	if (c->cgrp != NULL) {
		if(c->cgrp->ready != NULL) {
			cgrp_ready_rem(c->cgrp,c);
		}
	} 
	c->rcv_blocked = 1;
	while (rbIsEmpty(c->data_buffer)) {
		lwt_yield(NULL);
	}

	//somebody sent, so it can receive
	return rb_get(c->data_buffer);
}


lwt_cgrp_t lwt_cgrp(void)
{
    lwt_cgrp_t new = NULL;
    new->n_chan = 0;
    new->chan = NULL;
    new->ready = NULL;
    return new;
}

int lwt_cgrp_free(lwt_cgrp_t cgrp)
{
    if (cgrp->ready = NULL)
    {
    	free(cgrp);
    	return 0;
    }
    return 1;
}

int lwt_cgrp_add(lwt_cgrp_t cgrp, lwt_chan_t c)
{

	if (c->iscgrp == 1) return -1; //A channel can be added into only one group 

    cgrp->n_chan++;
    c->iscgrp = 1;
    c->cgrp = cgrp;
 
}

int lwt_cgrp_rem(lwt_cgrp_t cgrp, lwt_chan_t c)
{
	if (cgrp->ready != NULL) return 1; //cgrp has a pending event
	
	cgrp->n_chan--;
    c->iscgrp = 0;
    c->cgrp = NULL;
    return 0;

}

lwt_chan_t lwt_cgrp_wait(lwt_cgrp_t cgrp)
{
    while (cgrp->ready == NULL) {
    	lwt_yield(NULL);
    }
    return cgrp->ready;
}

void lwt_chan_mark_set(lwt_chan_t c, void * data)
{
    c->mark_data = data;
    return;
}

void *lwt_chan_mark_get(lwt_chan_t c)
{
    return c->mark_data;
}

