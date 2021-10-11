// synch.h 
//	Data structures for synchronizing threads.
//
//	Three kinds of synchronization are defined here: semaphores,
//	locks, and condition variables.  The implementation for
//	semaphores is given; for the latter two, only the procedure
//	interface is given -- they are to be implemented as part of 
//	the first assignment.
//
//	Note that all the synchronization objects take a "name" as
//	part of the initialization.  This is solely for debugging purposes.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// synch.h -- synchronization primitives.  

#ifndef SYNCH_H
#define SYNCH_H

#include "copyright.h"
#include "thread.h"
#include "list.h"

// The following class defines a "semaphore" whose value is a non-negative
// integer.  The semaphore has only two operations P() and V():
//
//	P() -- waits until value > 0, then decrement
//
//	V() -- increment, waking up a thread waiting in P() if necessary
// 
// Note that the interface does *not* allow a thread to read the value of 
// the semaphore directly -- even if you did read the value, the
// only thing you would know is what the value used to be.  You don't
// know what the value is now, because by the time you get the value
// into a register, a context switch might have occurred,
// and some other thread might have called P or V, so the true value might
// now be different.

class Semaphore {
  public:
    Semaphore(char* debugName, int initialValue);	// set initial value
    ~Semaphore();   					// de-allocate semaphore
    char* getName() { return name;}			// debugging assist
    
    void P();	 // these are the only operations on a semaphore
    void V();	 // they are both *atomic*
    
  private:
    char* name;        // useful for debugging
    int value;         // semaphore value, always >= 0
    List *queue;       // threads waiting in P() for the value to be > 0
};

// The following class defines a "lock".  A lock can be BUSY or FREE.
// There are only two operations allowed on a lock: 
//
//	Acquire -- wait until the lock is FREE, then set it to BUSY
//
//	Release -- set lock to be FREE, waking up a thread waiting
//		in Acquire if necessary
//
// In addition, by convention, only the thread that acquired the lock
// may release it.  As with semaphores, you can't read the lock value
// (because the value might change immediately after you read it).  

class Lock {
  public:
    Lock(char* debugName);  		// initialize lock to be FREE
    ~Lock();				// deallocate lock
    char* getName() { return name; }	// debugging assist

    void Acquire(); // these are the only operations on a lock
    void Release(); // they are both *atomic*

    bool isHeldByCurrentThread();	// true if the current thread
					// holds this lock.  Useful for
					// checking in Release, and in
					// Condition variable ops below.

  private:
    char* name;		
#ifdef CHANGED		// for debugging
    Semaphore* status;
    class Thread* lock_owner;
    int isLock;
#endif
    // plus some other stuff you'll need to define
};

// The following class defines a "condition variable".  A condition
// variable does not have a value, but threads may be queued, waiting
// on the variable.  These are only operations on a condition variable: 
//
//	Wait() -- release the lock, relinquish the CPU until signaled, 
//		then re-acquire the lock
//
//	Signal() -- wake up a thread, if there are any waiting on 
//		the condition
//
//	Broadcast() -- wake up all threads waiting on the condition
//
// All operations on a condition variable must be made while
// the current thread has acquired a lock.  Indeed, all accesses
// to a given condition variable must be protected by the same lock.
// In other words, mutual exclusion must be enforced among threads calling
// the condition variable operations.
//
// In Nachos, condition variables are assumed to obey *Mesa*-style
// semantics.  When a Signal or Broadcast wakes up another thread,
// it simply puts the thread on the ready list, and it is the responsibility
// of the woken thread to re-acquire the lock (this re-acquire is
// taken care of within Wait()).  By contrast, some define condition
// variables according to *Hoare*-style semantics -- where the signalling
// thread gives up control over the lock and the CPU to the woken thread,
// which runs immediately and gives back control over the lock to the 
// signaller when the woken thread leaves the critical section.
//
// The consequence of using Mesa-style semantics is that some other thread
// can acquire the lock, and change data structures, before the woken
// thread gets a chance to run.

class Condition {
  public:
    Condition(char* debugName);		// initialize condition to 
					// "no one waiting"
    ~Condition();			// deallocate the condition
    char* getName() { return (name); }
    
    void Wait(Lock *conditionLock); 	// these are the 3 operations on 
					// condition variables; releasing the 
					// lock and going to sleep are 
					// *atomic* in Wait()
    void Signal(Lock *conditionLock);   // conditionLock must be held by
    void Broadcast(Lock *conditionLock);// the currentThread for all of 
					// these operations

  private:
    char* name;
    List* Conqueue;                      // the queue for this variable

    // plus some other stuff you'll need to define
};

#endif // SYNCH_H

--
  
// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    DEBUG ('s', "thread %s acquires semaphore %s\n", currentThread->getName(),
	   this->getName());

    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;

    DEBUG ('s', "thread %s releases semaphore %s\n", currentThread->getName(),
	   this->getName());

    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!



// ----------------------------------------------------------------------
#ifdef CHANGED
//-----------------------------------------------------------------------
// Lock :: Lock(char * debugName) , constuctor for Lock
// ----------------------------------------------------------------------
Lock::Lock(char* debugName)
{
  name = debugName;
  status = new Semaphore("locksem", 1);    // one imply free
  lock_owner = NULL;
  isLock = 0;
}

// ----------------------------------------------------------------------
// Lock :: ~Lock(), destructor for Lock
// ----------------------------------------------------------------------

Lock::~Lock()
{
  delete status;         // deallocate the semaphore
  delete lock_owner;
}

// ----------------------------------------------------------------------
// Lock :: Acquire();
// ----------------------------------------------------------------------

void Lock::Acquire()
{

  IntStatus oldLevel = interrupt->SetLevel(IntOff); //disable interrupt
  DEBUG ('s', "thread %s acquire lock %s\n", currentThread->getName(),
	 this->getName());
  status->P();                  // decrement from 1 to 0 to indicate busy 
  
  DEBUG ('s', "thread %s succeeds in acquiring lock %s \n",
	 currentThread->getName(), this->getName());
  lock_owner = currentThread;   // the locked has to be owned by the current thread for
                                // the released to happen
  isLock = 1;
  (void) interrupt->SetLevel(oldLevel);             // reenable interrupt
}
void Lock::Release()
{
  IntStatus oldLevel = interrupt->SetLevel(IntOff); //disable interrupt
  if (isLock && isHeldByCurrentThread()) 
  {
    status->V();
    isLock = 0;
    lock_owner = NULL;
  }
  (void) interrupt->SetLevel(oldLevel);             // reenable interrupt
}
bool Lock:: isHeldByCurrentThread()
{
  return(lock_owner == currentThread);
}
#endif

// --------------------------------------------------------------------------

#ifdef CHANGED
Condition::Condition(char* debugName)
{
  name = debugName;
  Conqueue = new List;
}

Condition::~Condition()
{
  delete Conqueue;
}

void Condition::Wait(Lock* conditionLock)
{
  ASSERT(conditionLock->isHeldByCurrentThread()); 
  IntStatus oldLevel = interrupt->SetLevel(IntOff);  // disable interrupts
  conditionLock->Release();
  Conqueue->Append((void *) currentThread);
  currentThread->Sleep();
  conditionLock->Acquire();
  (void) interrupt->SetLevel(oldLevel);             // reenable interrupts
}

void Condition::Signal(Lock* conditionLock)
{
  Thread * thread;

  ASSERT(conditionLock->isHeldByCurrentThread());
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  thread = (Thread *)Conqueue->Remove();
  if (thread != NULL)
    scheduler->ReadyToRun(thread);
  (void) interrupt->SetLevel(oldLevel);             // reenable interrupt
}

void Condition::Broadcast(Lock* conditionLock)
{
  Thread * thread;
  ASSERT(conditionLock->isHeldByCurrentThread());
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  while(!Conqueue->IsEmpty())
    {
      thread = (Thread *) Conqueue->Remove();
      scheduler->ReadyToRun(thread);
    }
  (void) interrupt->SetLevel(oldLevel);             // reenable interrupt
  
}
#endif
