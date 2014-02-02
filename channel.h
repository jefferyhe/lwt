#ifndef CHAN_H
	#define CHAN_H
#endif


typedef struct clist_head
{
	int id;
	//int snd_blocked;
	void* data;
	lwt_t thd;
	struct clist_head *next;

} clist_head, *clist_t;

typedef struct ring_buffer {
	int size;
	int start;
	int end;
//	int count;
	void** data;  
} ring_buffer;


typedef struct cgroup
{
	int id;
    int n_chan;          //number of channels in the group
	struct lwt_channel* chan;
	struct lwt_channel* ready;   //the channel that ready to receive, point to the head of the event queue
    
} cgroup, *lwt_cgrp_t;


typedef struct lwt_channel 
{
	//int id;     /* channel's id*/
	
	/* sender’s data */
	//void *snd_data;
	int snd_cnt; 				/* number of sending threads */
	clist_t snd_thds;			/* list of those threads */
    
    
	/* receiver’s data */
	int rcv_blocked; 			/* if the receiver is blocked */
	lwt_t rcv_thd;	 			/* the receiver */
    void *mark_data;  			/* arbitrary value receiver can store */
    

    /* chan's data */
    struct lwt_channel* next;
    struct lwt_channel* pre;
    int iscgrp;            //check if it added to a group
	lwt_cgrp_t cgrp;		// belongs to which group
    int size;				// size of the buffer
    ring_buffer* data_buffer;

} lwt_channel, *lwt_chan_t;

//int n_chan; //channel counter

clist_t tail_snd; 			//point to the end of sender's list
lwt_chan_t tail_chan;       //point to the end of the channel group
lwt_chan_t tail_cgrp_ready;	//point to the end of the group's ready queue

