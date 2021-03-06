// interrupt.h 
//	Data structures to emulate low-level interrupt hardware.
//
//	The hardware provides a routine (SetLevel) to enable or disable
//	interrupts.
//
//	In order to emulate the hardware, we need to keep track of all
//	interrupts the hardware devices would cause, and when they
//	are supposed to occur.  
//
//	This module also keeps track of simulated time.  Time advances
//	only when the following occur: 
//		interrupts are re-enabled
//		a user instruction is executed
//		there is nothing in the ready queue
//
//	As a result, unlike real hardware, interrupts (and thus time-slice 
//	context switches) cannot occur anywhere in the code where interrupts
//	are enabled, but rather only at those places in the code where 
//	simulated time advances (so that it becomes time to invoke an
//	interrupt in the hardware simulation).
//
//	NOTE: this means that incorrectly synchronized code may work
//	fine on this hardware simulation (even with randomized time slices),
//	but it wouldn't work on real hardware.  (Just because we can't
//	always detect when your program would fail in real life, does not 
//	mean it's ok to write incorrectly synchronized code!)
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "copyright.h"
#include "list.h"

// Interrupts can be disabled (IntOff) or enabled (IntOn)
enum IntStatus { IntOff, IntOn };

// Nachos can be running kernel code (SystemMode), user code (UserMode),
// or there can be no runnable thread, because the ready list 
// is empty (IdleMode).
enum MachineStatus {IdleMode, SystemMode, UserMode};

// IntType records which hardware device generated an interrupt.
// In Nachos, we support a hardware timer device, a disk, a console
// display and keyboard, and a network.
enum IntType { TimerInt, DiskInt, ConsoleWriteInt, ConsoleReadInt, 
				NetworkSendInt, NetworkRecvInt};


// The following class defines an interrupt that is scheduled
// to occur in the future.  The internal data structures are
// left public to make it simpler to manipulate.

class PendingInterrupt {
  public:
    PendingInterrupt(VoidFunctionPtr func, _int param, int time, IntType kind);
				// initialize an interrupt that will
				// occur in the future

    VoidFunctionPtr handler;    // The function (in the hardware device
				// emulator) to call when the interrupt occurs
    _int arg;           // The argument to the function.
    int when;			// When the interrupt is supposed to fire
    IntType type;		// for debugging
};

// The following class defines the data structures for the simulation
// of hardware interrupts.  We record whether interrupts are enabled
// or disabled, and any hardware interrupts that are scheduled to occur
// in the future.

class Interrupt {
  public:
    Interrupt();			// initialize the interrupt simulation
    ~Interrupt();			// de-allocate data structures
    
    IntStatus SetLevel(IntStatus level);// Disable or enable interrupts 
					// and return previous setting.

    void Enable();			// Enable interrupts.
    IntStatus getLevel() {return level;}// Return whether interrupts
					// are enabled or disabled
    
    void Idle(); 			// The ready queue is empty, roll 
					// simulated time forward until the 
					// next interrupt

    void Halt(); 			// quit and print out stats
    
    void YieldOnReturn();		// cause a context switch on return 
					// from an interrupt handler

    MachineStatus getStatus() { return status; } // idle, kernel, user
    void setStatus(MachineStatus st) { status = st; }

    void DumpState();			// Print interrupt state
    

    // NOTE: the following are internal to the hardware simulation code.
    // DO NOT call these directly.  I should make them "private",
    // but they need to be public since they are called by the
    // hardware device simulators.

    void Schedule(VoidFunctionPtr handler,// Schedule an interrupt to occur
	_int arg, int when, IntType type);// at time ``when''.  This is called
    					// by the hardware device simulators.
    
    void OneTick();       		// Advance simulated time

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //EXPT 2 added
    void DequeuePendingInterrupt(); //removes a pending interrupt from head of pending list

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


  private:
    IntStatus level;		// are interrupts enabled or disabled?
    List *pending;		// the list of interrupts scheduled
				// to occur in the future
    bool inHandler;		// TRUE if we are running an interrupt handler
    bool yieldOnReturn; 	// TRUE if we are to context switch
				// on return from the interrupt handler
    MachineStatus status;	// idle, kernel mode, user mode

    // these functions are internal to the interrupt simulation code

    bool CheckIfDue(bool advanceClock); // Check if an interrupt is supposed
					// to occur now

    void ChangeLevel(IntStatus old, 	// SetLevel, without advancing the
	IntStatus now);  		// simulated time
};

#endif // INTERRRUPT_H



------------------------------------------------------------------------------------------------------------------------------


// interrupt.cc 
//	Routines to simulate hardware interrupts.
//
//	The hardware provides a routine (SetLevel) to enable or disable
//	interrupts.
//
//	In order to emulate the hardware, we need to keep track of all
//	interrupts the hardware devices would cause, and when they
//	are supposed to occur.  
//
//	This module also keeps track of simulated time.  Time advances
//	only when the following occur: 
//		interrupts are re-enabled
//		a user instruction is executed
//		there is nothing in the ready queue
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "interrupt.h"
#include "system.h"


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/* Experiment 2 
   Add code to remove pending interrupt from head of pending list. */
   
void
Interrupt :: DequeuePendingInterrupt()
{
        if (!pending->IsEmpty())
	delete pending->Remove();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



// String definitions for debugging messages

static char *intLevelNames[] = { "off", "on"};
static char *intTypeNames[] = { "timer", "disk", "console write", 
			"console read", "network send", "network recv"};

//----------------------------------------------------------------------
// PendingInterrupt::PendingInterrupt
// 	Initialize a hardware device interrupt that is to be scheduled 
//	to occur in the near future.
//
//	"func" is the procedure to call when the interrupt occurs
//	"param" is the argument to pass to the procedure
//	"time" is when (in simulated time) the interrupt is to occur
//	"kind" is the hardware device that generated the interrupt
//----------------------------------------------------------------------

PendingInterrupt::PendingInterrupt(VoidFunctionPtr func, _int param, int time, 
				IntType kind)
{
    handler = func;
    arg = param;
    when = time;
    type = kind;
}

//----------------------------------------------------------------------
// Interrupt::Interrupt
// 	Initialize the simulation of hardware device interrupts.
//	
//	Interrupts start disabled, with no interrupts pending, etc.
//----------------------------------------------------------------------

Interrupt::Interrupt()
{
    level = IntOff;
    pending = new List();
    inHandler = FALSE;
    yieldOnReturn = FALSE;
    status = SystemMode;
}

//----------------------------------------------------------------------
// Interrupt::~Interrupt
// 	De-allocate the data structures needed by the interrupt simulation.
//----------------------------------------------------------------------

Interrupt::~Interrupt()
{
    while (!pending->IsEmpty())
	delete pending->Remove();
    delete pending;
}

//----------------------------------------------------------------------
// Interrupt::ChangeLevel
// 	Change interrupts to be enabled or disabled, without advancing 
//	the simulated time (normally, enabling interrupts advances the time).

//----------------------------------------------------------------------
// Interrupt::ChangeLevel
// 	Change interrupts to be enabled or disabled, without advancing 
//	the simulated time (normally, enabling interrupts advances the time).
//
//	Used internally.
//
//	"old" -- the old interrupt status
//	"now" -- the new interrupt status
//----------------------------------------------------------------------
void
Interrupt::ChangeLevel(IntStatus old, IntStatus now)
{
    level = now;
    DEBUG('i',"\tinterrupts: %s -> %s\n",intLevelNames[old],intLevelNames[now]);
}

//----------------------------------------------------------------------
// Interrupt::SetLevel
// 	Change interrupts to be enabled or disabled, and if interrupts
//	are being enabled, advance simulated time by calling OneTick().
//
// Returns:
//	The old interrupt status.
// Parameters:
//	"now" -- the new interrupt status
//----------------------------------------------------------------------

IntStatus
Interrupt::SetLevel(IntStatus now)
{
    IntStatus old = level;
    
    ASSERT((now == IntOff) || (inHandler == FALSE));// interrupt handlers are 
						// prohibited from enabling 
						// interrupts

    ChangeLevel(old, now);			// change to new state
    if ((now == IntOn) && (old == IntOff))
	OneTick();				// advance simulated time
    return old;
}

//----------------------------------------------------------------------
// Interrupt::Enable
// 	Turn interrupts on.  Who cares what they used to be? 
//	Used in ThreadRoot, to turn interrupts on when */
	interrupt->DequeuePendingInterrupt();
	timer->myfunction();	first starting up
//	a thread.
//----------------------------------------------------------------------
void
Interrupt::Enable()
{ 
    (void) SetLevel(IntOn); 
}

//----------------------------------------------------------------------
// Interrupt::OneTick
// 	Advance simulated time and check if there are any pending 
//	interrupts to be called. 
//
//	Two things can cause OneTick to be called:
//		interrupts are re-enabled
//		a user instruction is executed
//----------------------------------------------------------------------
void
Interrupt::OneTick()
{
    MachineStatus old = status;

// advance simulated time
    if (status == SystemMode) {
        stats->totalTicks += SystemTick;
	stats->systemTicks += SystemTick;
    } else {					// USER_PROGRAM
	stats->totalTicks += UserTick;
	stats->userTicks += UserTick;
    }
    DEBUG('i', "\n== Tick %d ==\n", stats->totalTicks);

// check any pending interrupts are now ready to fire
    ChangeLevel(IntOn, IntOff);		// first, turn off interrupts
					// (interrupt handlers run with
					// interrupts disabled)
    while (CheckIfDue(FALSE))		// check for pending interrupts
	;
    ChangeLevel(IntOff, IntOn);		// re-enable interrupts
    if (yieldOnReturn) {		// if the timer device handler asked 
					// for a context switch, ok to do it now
	yieldOnReturn = FALSE;
 	status = SystemMode;		// yield is a kernel routine
	currentThread->Yield();
	status = old;
    }
}

//----------------------------------------------------------------------
// Interrupt::YieldOnReturn
// 	Called from within an interrupt handler, to cause a context switch
//	(for example, on a time slice) in the interrupted thread,
//	when the handler returns.
//
//	We can't do the context switch here, because that would switch
//	out the interrupt handler, and we want to switch out the 
//	interrupted thread.
//----------------------------------------------------------------------

void
Interrupt::YieldOnReturn()
{ 
    ASSERT(inHandler == TRUE);  
    yieldOnReturn = TRUE; 
}

//----------------------------------------------------------------------
// Interrupt::Idle
// 	Routine called when there is nothing in the ready queue.
//
//	Since something has to be running in order to put a thread
//	on the ready queue, the only thing to do is to advance 
//	simulated time until the next scheduled hardware interrupt.
//
//	If there are no pending interrupts, stop.  There's nothing
//	more for us to do.
//----------------------------------------------------------------------
void
Interrupt::Idle()
{
    DEBUG('i', "Machine idling; checking for interrupts.\n");
    status = IdleMode;
    if (CheckIfDue(TRUE)) {		// check for any pending interrupts
    	while (CheckIfDue(FALSE))	// check for any other pending 
	    ;				// interrupts
        yieldOnReturn = FALSE;		// since there's nothing in the
					// ready queue, the yield is automatic
        status = SystemMode;
	return;				// return in case there's now
					// a runnable thread
    }

    // if there are no pending interrupts, and nothing is on the ready
    // queue, it is time to stop.   If the console or the network is 
    // operating, there are *always* pending interrupts, so this code
    // is not reached.  Instead, the halt must be invoked by the user program.

    DEBUG('i', "Machine idle.  No interrupts to do.\n");
    printf("No threads ready or runnable, and no pending interrupts.\n");
    printf("Assuming the program completed.\n");
    Halt();
}

//----------------------------------------------------------------------
// Interrupt::Halt
// 	Shut down Nachos cleanly, printing out performance statistics.
//----------------------------------------------------------------------
void
Interrupt::Halt()
{
    printf("Machine halting!\n\n");
    stats->Print();
    Cleanup();     // Never returns.
}

//----------------------------------------------------------------------
// Interrupt::Schedule
// 	Arrange for the CPU to be interrupted when simulated time
//	reaches "now + when".
//
//	Implementation: just put it on a sorted list.
//
//	NOTE: the Nachos kernel should not call this routine directly.
//	Instead, it is only called by the hardware device simulators.
//
//	"handler" is the procedure to call when the interrupt occurs
//	"arg" is the argument to pass to the procedure
//	"fromNow" is how far in the future (in simulated time) the 
//		 interrupt is to occur
//	"type" is the hardware device that generated the interrupt
//----------------------------------------------------------------------
void
Interrupt::Schedule(VoidFunctionPtr handler, _int arg, int fromNow, IntType type)
{
    int when = stats->totalTicks + fromNow;
    PendingInterrupt *toOccur = new PendingInterrupt(handler, arg, when, type);

    DEBUG('i', "Scheduling interrupt handler the %s at time = %d\n", 
					intTypeNames[type], when);
    ASSERT(fromNow > 0);

    pending->SortedInsert(toOccur, when);
}

//----------------------------------------------------------------------
// Interrupt::CheckIfDue
// 	Check if an interrupt is scheduled to occur, and if so, fire it off.
//
// Returns:
//	TRUE, if we fired off any interrupt handlers
// Params:
//	"advanceClock" -- if TRUE, there is nothing in the ready queue,
//		so we should simply advance the clock to when the next 
//		pending interrupt would occur (if any).  If the pending
//		interrupt is just the time-slice daemon, however, then 
//		we're done!
//----------------------------------------------------------------------
bool
Interrupt::CheckIfDue(bool advanceClock)
{
    MachineStatus old = status;
    int when;

    ASSERT(level == IntOff);		// interrupts need to be disabled,
					// to invoke an interrupt handler
    if (DebugIsEnabled('i'))
	DumpState();
    PendingInterrupt *toOccur = 
		(PendingInterrupt *)pending->SortedRemove(&when);

    if (toOccur == NULL)		// no pending interrupts
	return FALSE;			

    if (advanceClock && when > stats->totalTicks) {	// advance the clock
	stats->idleTicks += (when - stats->totalTicks);
	stats->totalTicks = when;
    } else if (when > stats->totalTicks) {	// not time yet, put it back
	pending->SortedInsert(toOccur, when);
	return FALSE;
    }

// Check if there is nothing more to do, and if so, quit
    if ((status == IdleMode) && (toOccur->type == TimerInt) 
				&& pending->IsEmpty()) {
	 pending->SortedInsert(toOccur, when);
	 return FALSE;
    }

    DEBUG('i', "Invoking interrupt handler for the %s at time %d\n", 
			intTypeNames[toOccur->type], toOccur->when);
#ifdef USER_PROGRAM
    if (machine != NULL)
    	machine->DelayedLoad(0, 0);
#endif
    inHandler = TRUE;
    status = SystemMode;			// whatever we were doing,
						// we are now going to be
						// running in the kernel
    (*(toOccur->handler))(toOccur->arg);	// call the interrupt handler
    status = old;				// restore the machine status
    inHandler = FALSE;
    delete toOccur;
    return TRUE;
}

//----------------------------------------------------------------------
// PrintPending
// 	Print information about an interrupt that is scheduled to occur.
//	When, where, why, etc.
//----------------------------------------------------------------------

static void
PrintPending(_int arg)
{
    PendingInterrupt *pend = (PendingInterrupt *)arg;

    printf("Interrupt handler %s, scheduled at %d\n", 
	intTypeNames[pend->type], pend->when);
}

//----------------------------------------------------------------------
// DumpState
// 	Print the complete interrupt state - the status, and all interrupts
//	that are scheduled to occur in the future.
//----------------------------------------------------------------------

void
Interrupt::DumpState()
{
    printf("Time: %d, interrupts %s\n", stats->totalTicks, 
					intLevelNames[level]);
    printf("Pending interrupts:\n");
    fflush(stdout);
    pending->Mapcar(PrintPending);
    printf("End of pending interrupts\n");
    fflush(stdout);
}
