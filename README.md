# lwt 

#### Introduction

* __lwt__ stands for light-weight-threading, which is an user-level non-preemptive multi-threading library for GNU environment.

* This is a threading library that provides some API similar to POSIX Threads (PThread).

* User-level means the switching between threads does NOT trap into kernel mode of operating system.

* Non-preemptive means there is no priority concept between different threads. The switching between them need user to call yield() function in user-define function.

===

#### Data structures:

1. **_TCB_STATE_** denotes the state of a thread control block (TCB), may be __TCB_ACTIVE, __TCB_DEAD, or __TCB_BLOCKED if it's joining other thread.

2. __lwt_t__ The pointer to a TCB that contains some important information of specific thread:
	* thread id
	* Stack Pointer of thread
	* Instruction Poinetr (Program Counter) of thread
	* the return value of this thread
	* previous node of run queue
	
3. ____lwt_fn_t__ typedef _void *(lwt_fn_t) (void *)_ to make it as a function pointer.

#### APIs for user:

1. __lwt_t lwt_create(lwt_fn_t)__ as with pthread_create, this function calls the passed in _fn()_ with the argument passed in as _data_. It will return the lwt_t mentioned above.

2. __void *lwt_join(lwt_t)__ is equivalent of pthread_join. It blocks current thread for waiting the refereced lwt to
terminate, and returns the _void *_ that the thread itself returned from its _lwt_fn_t_, ro that passed to _lwt_die()_.

3. __void lwt_die(void *)__ will attempt to kill the current thread. This is the only way to terminate a thread, even if a thread returns natually, this function will be called in __invisible system APIs__.

	It will also clear up the space of TCB and thread stack (memory) that malloc before.

4. __int lwt_yield(lwt_t)__ will yield the current executing lwt, and possibly switch to another lwt (as determined by the scheduling policy, which is the run queue). 

	The function's argument is either a reference to a lwt in the system, or a special "no lwt" vale (the equivalent to NULL) called LWT_NULL.
	
	It the argument of this function is NOT LWT_NULL, then instead of allowing the scheduler to decide the next thread ot execute, the library instead attemps to switch directly to the specified lwt.
	
5. __lwt_t lwt_current(void)__ returns the currently active lwt (that is calling this function).

6. __int lwt_id(lwt_t)__ returns the unique identifier for the thread. I temporarily ignore integer overflow of such a counter.


#### Invisible system APIs:

1. **void __lwt_schedule(void)** which will possibly switch to another thread based on its scheduling policy. **lwt_yield()**, when taking no argument, is a small wrapper around this function.

2. **void __dispatch(lwt_t next, lwt_t current)** which implements the inline assembly for the context switch from the current thread to the next. The scheduling function uses this to dispatch to the thread chosen to execute next.

3. **void __lwt_start(lwt_fn_t fn, void *data)** is the destination instruction pointer of a freshly created thread. It will call the user-defined _fn_ and pass in the _data_, then is must lwt_die() accordingly (passing to lwt_die the return value from the _fn_).



