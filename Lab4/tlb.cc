#ifndef REALTLB_H
#define REALTLB_H

#include "copyright.h"
#include "ipt.h"
#include "addrspace.h"

// function declarations

//----------------------------------------------------------------------
// UpdateTLB
//      Called when exception is raised and a page isn't in the TLB.
// Figures out what to do (get from IPT, or pageoutpagein) and does it.
//----------------------------------------------------------------------

void UpdateTLB(int possible_badVAddr);

//----------------------------------------------------------------------
// InsertToTLB
//      Put a vpn/phyPage combination into the TLB.
//----------------------------------------------------------------------

void InsertToTLB(int vpn, int phyPage);

//----------------------------------------------------------------------
// PageOutPageIn
//      Calls DoPageOut and DoPageIn and handles IPT and memoryTable
// bookkeeping.
//----------------------------------------------------------------------

int PageOutPageIn(int vpn);

//----------------------------------------------------------------------
// DoPageOut
//      Actually pages out a phyPage to it's swapfile.
//----------------------------------------------------------------------

void DoPageOut(int phyPage);

//----------------------------------------------------------------------
// DoPageIn
//      Actually pages in a phyPage/vpn combo from the swapfile.
//----------------------------------------------------------------------

void DoPageIn(int vpn, int phyPage);

//----------------------------------------------------------------------
// lruAlgorithm
//      Determine where a vpn should go in phymem, and therefore what
// should be paged out.
//----------------------------------------------------------------------

int lruAlgorithm(void);

//----------------------------------------------------------------------
// GetMmap
//      Return an MmapEntry structure corresponding to the vpn.  Returns
// 0 if does not exist.
//----------------------------------------------------------------------

MmapEntry *GetMmap(int vpn);

//----------------------------------------------------------------------
// VpnToPhyPage
//      Gets a phyPage from a vpn, if exists.
//----------------------------------------------------------------------

int VpnToPhyPage(int vpn);

//----------------------------------------------------------------------
// PageOutMmapSpace
//      Pages out stuff being mmaped (or just between beginPage and
// endPage.
//----------------------------------------------------------------------

void PageOutMmapSpace(int beginPage, int endPage);

#endif // TLB_H


------------------------------------------------------------------------------------------------------------------------------


#include "copyright.h"
#include "tlb.h"
#include "syscall.h"
#include "machine.h"
#include "thread.h"
#include "system.h"
#include "utility.h"

//----------------------------------------------------------------------
// UpdateTLB
//      Called when exception is raised and a page isn't in the TLB.
// Figures out what to do (get from IPT, or pageoutpagein) and does it.
//----------------------------------------------------------------------

void UpdateTLB(int possible_badVAddr)
{
  int badVAddr;
  unsigned int vpn;
  int phyPage;

  if(possible_badVAddr) // get the bad address from the correct location
    badVAddr=possible_badVAddr; // fault in kernel
  else
    badVAddr=machine->registers[BadVAddrReg]; // fault in userprog
  
  vpn=(unsigned)badVAddr/PageSize;
  
  if((phyPage=VpnToPhyPage(vpn))!=-1) {
    InsertToTLB(vpn, phyPage);
  } else {
    if(vpn>=currentThread->space->numPages && !GetMmap(vpn))
      machine->RaiseException(AddressErrorException, badVAddr);
    else
      InsertToTLB(vpn, PageOutPageIn(vpn));
  }
}

//----------------------------------------------------------------------
// VpnToPhyPage
//      Gets a phyPage for a vpn, if exists in ipt.
//----------------------------------------------------------------------

int VpnToPhyPage(int vpn)
{
  //your code here to get a physical frame for page vpn
  //you can refer to PageOutPageIn(int vpn) to see how an entry was created in ipt
	 
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//debug message at this time tick for IPT b4 insertion
	for(int k = 0; k < NumPhysPages; k++)
		DEBUG('p', "\n**** IPT [%i]: <pid, vpn, last-Used, valid-flag> = <%i, %i, %i, %i> \n", k, memoryTable[k].pid, memoryTable[k].vPage, memoryTable[k].lastUsed, memoryTable[k].valid);
	
	for(int i = 0; i < NumPhysPages; i++){
		if(memoryTable[i].vPage == vpn && 
			memoryTable[i].pid == currentThread->pid &&
			memoryTable[i].valid){
				DEBUG('p', "Found! vpn %i of pid %d already exists in IPT[%d]\n", vpn, currentThread->pid, i);
				return i;
		}
	}
  return -1;
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}

//----------------------------------------------------------------------
// InsertToTLB
//      Put a vpn/phyPage combination into the TLB. If TLB is full, use FIFO 
//    replacement
//----------------------------------------------------------------------

void InsertToTLB(int vpn, int phyPage)
{
  int i = 0; //entry in the TLB
  
  //your code to find an empty in TLB or to replace the oldest entry if TLB is full
  
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
	//to always point to the oldest entry in tlb
	static int FIFOPointer = 0;  //in c++, static var is not re-initalized 
															 //even when fn is called multiple times	

	bool emptySlot = false;
	int j;

	for(j = 0; j < TLBSize; j++){
		if(!machine->tlb[j].valid){
			emptySlot = true;
			break;
		}		
	}
	
	if(emptySlot){
		i = j;
		if(i == FIFOPointer) //emptySlot and FIFOPointer also points to the slot, need to inc FIFOPointer !
			FIFOPointer = (i + 1) % TLBSize; 
	}
	else{
		i = FIFOPointer;
		FIFOPointer = (i + 1) % TLBSize;
	}
	
	//debug message for tlb at this tick b4 the insertion
	for(int k = 0; k < TLBSize; k++)
		DEBUG('p', "\n****TLB[%i] : <vpn, phyPage, valid-flag> = <%i, %i, %i>\n", k, machine->tlb[k].virtualPage, machine->tlb[k].physicalPage, machine->tlb[k].valid );	
  

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
  // copy dirty data to memoryTable
  if(machine->tlb[i].valid){
    memoryTable[machine->tlb[i].physicalPage].dirty=machine->tlb[i].dirty;
    memoryTable[machine->tlb[i].physicalPage].TLBentry=-1;
  }

  //update the TLB entry
  machine->tlb[i].virtualPage  = vpn;
  machine->tlb[i].physicalPage = phyPage;
  machine->tlb[i].valid        = TRUE;
  machine->tlb[i].readOnly     = FALSE;
  machine->tlb[i].use          = FALSE; //will be true once you ref this vpn the 2nd time
  machine->tlb[i].dirty        = memoryTable[phyPage].dirty;

  //update the corresponding memoryTable
  memoryTable[phyPage].TLBentry=i;
  DEBUG('p', "The corresponding TLBentry for Page %i in TLB is %i ", vpn, i);
  //reset lastUsed to current ticks since it is being used at this moment.
  //for the implementation of LRU algorithm.
  memoryTable[phyPage].lastUsed = stats->totalTicks; 
  
  //increase the number of tlb misses
  stats->numTlbMisses++;

}

//----------------------------------------------------------------------
// PageOutPageIn
//      Calls DoPageOut and DoPageIn and handles memoryTable
// bookkeeping. Use lru algorithm to find the replacement page.
//----------------------------------------------------------------------

int PageOutPageIn(int vpn)
{
  int phyPage; 
  
  //increase the number of page faults
  stats->numPageFaults++;
  //call the LRU algorithm, which returns the freed physical frame
  phyPage=lruAlgorithm();
  
  //Page out the victim page to free the physical frame
  DoPageOut(phyPage);

  //Page in the new page to the freed physical frame
  DoPageIn(vpn, phyPage);
  
  //update memoryTable for this frame
  memoryTable[phyPage].valid=TRUE;
  memoryTable[phyPage].pid=currentThread->pid;
  memoryTable[phyPage].vPage=vpn;
  memoryTable[phyPage].dirty=FALSE;
  memoryTable[phyPage].TLBentry=-1;
  memoryTable[phyPage].lastUsed=0;
  memoryTable[phyPage].swapPtr=currentThread->space->swapPtr;

	
  
  return phyPage;
}

//----------------------------------------------------------------------
// DoPageOut
//      Actually pages out a phyPage to it's swapfile.
//----------------------------------------------------------------------

void DoPageOut(int phyPage)
{
  MmapEntry *mmapPtr;
  int numBytesWritten;
  int mmapBytesToWrite;

  if(memoryTable[phyPage].valid){           // check if pageOut possible
    if(memoryTable[phyPage].TLBentry!=-1){
      memoryTable[phyPage].dirty=
        machine->tlb[memoryTable[phyPage].TLBentry].dirty;
      machine->tlb[memoryTable[phyPage].TLBentry].valid=FALSE;
    }
    if(memoryTable[phyPage].dirty){        // pageOut is necessary
      if((mmapPtr=GetMmap(memoryTable[phyPage].vPage))){ // it's mmaped
        DEBUG('p', "mmap paging out: pid %i, phyPage %i, vpn %i\n",
          memoryTable[phyPage].pid, phyPage, memoryTable[phyPage].vPage);
        if(memoryTable[phyPage].vPage==mmapPtr->endPage)
          mmapBytesToWrite=mmapPtr->lastPageLength;
        else
          mmapBytesToWrite=PageSize;
        numBytesWritten=mmapPtr->openFile->
          WriteAt(machine->mainMemory+phyPage*PageSize, mmapBytesToWrite,
            (memoryTable[phyPage].vPage-mmapPtr->beginPage)*PageSize);
        ASSERT(mmapBytesToWrite==numBytesWritten);
      } else { // it's not mmaped
        DEBUG('p', "paging out: pid %i, phyPage %i, vpn %i\n",
          memoryTable[phyPage].pid, phyPage, memoryTable[phyPage].vPage);
        numBytesWritten=memoryTable[phyPage].swapPtr->
          WriteAt(machine->mainMemory+phyPage*PageSize, PageSize,
            memoryTable[phyPage].vPage*PageSize);
        ASSERT(PageSize==numBytesWritten);
      }
      
      //increase the number of page faults
      stats->numPageOuts++;
      
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      
      //debug message for paging out
      DEBUG('p', "**** pageout = Y\n");
      
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    }
    
    memoryTable[phyPage].valid=FALSE;
  }
}

//----------------------------------------------------------------------
// DoPageIn
//      Actually pages in a phyPage/vpn combo from the swapfile.
//----------------------------------------------------------------------

void DoPageIn(int vpn, int phyPage)
{
  MmapEntry *mmapPtr;
  int numBytesRead;
  int mmapBytesToRead;

  if((mmapPtr=GetMmap(vpn))){ // mmaped file
    DEBUG('p', "mmap paging in: pid %i, phyPage %i, vpn %i\n",
      currentThread->pid, phyPage, vpn);
    if(vpn==mmapPtr->endPage)
      mmapBytesToRead=mmapPtr->lastPageLength;
    else
      mmapBytesToRead=PageSize;
    numBytesRead=
      mmapPtr->openFile->ReadAt(machine->mainMemory+phyPage*PageSize,
                mmapBytesToRead,
                (vpn-mmapPtr->beginPage)*PageSize);
    ASSERT(numBytesRead==mmapBytesToRead);
  } else { // not mmaped
    DEBUG('p', "paging in: pid %i, phyPage %i, vpn %i\n", currentThread->pid,
      phyPage, vpn);
    numBytesRead=currentThread->space->swapPtr->ReadAt(machine->mainMemory+
                   phyPage*PageSize,
                   PageSize,
                   vpn*PageSize);
    ASSERT(PageSize==numBytesRead);
  }
}

//----------------------------------------------------------------------
// lruAlgorithm
//      Determine where a vpn should go in phymem, and therefore what
// should be paged out. This lru algorithm is the one discussed in the 
// lectures.
//----------------------------------------------------------------------

int lruAlgorithm(void)
{
  //your code here to find the physical frame that should be freed 
  //according to the LRU algorithm. 
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
  int phyPage;
	int leastUsedTime = memoryTable[0].lastUsed;
	int leastUsedFrame = 0;


	for(int i = 0; i < NumPhysPages; i++){
		if(!memoryTable[i].valid){ //there's empty frame
			phyPage = i;
			break;
		}
		else{ //keep track of least recently used frame #
			if(leastUsedTime > memoryTable[i].lastUsed){ //finding the frame with the shortest time stamp
				leastUsedTime = memoryTable[i].lastUsed;
				leastUsedFrame = i;
	 		}
      			phyPage = leastUsedFrame;
		}
	}

  return phyPage;
  
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}

//----------------------------------------------------------------------
// GetMmap
//      Return an MmapEntry structure corresponding to the vpn.  Returns
// 0 if does not exist.
//----------------------------------------------------------------------

MmapEntry *GetMmap(int vpn)
{
  MmapEntry *mmapPtr;

  mmapPtr=currentThread->space->mmapEntries;
  while(mmapPtr->next){
    mmapPtr=mmapPtr->next;
    if(vpn>=mmapPtr->beginPage && vpn<=mmapPtr->endPage)
      return mmapPtr;
  }
  return 0;
}

//----------------------------------------------------------------------
// PageOutMmapSpace
//      Pages out stuff being mmaped (or just between beginPage and
// endPage.
//----------------------------------------------------------------------

void PageOutMmapSpace(int beginPage, int endPage)
{
  int vpn;
  int phyPage;

  for(vpn=beginPage; vpn<=endPage; vpn++){
    if((phyPage=VpnToPhyPage(vpn))==-1)
      continue;
    DoPageOut(phyPage);
  }
}

