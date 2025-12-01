// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"

int getPpnToSwap() {
    switch (kernel->pageReplacementType) {
    case PageReplacementType::LRU: {
        for (unsigned int ppn = 0; ppn < NumPhysPages; ppn++) {
            CoreMapEntry& cmEntry = kernel->coreMap[ppn];
            if (!cmEntry.ownerThread) {
                DEBUG(dbgVM, "ppn " << ppn << " is free" << (cmEntry.lock ? " (locked)" : ""));
                continue;
            }
            TranslationEntry* entry = &cmEntry.ownerThread->space->pageTable[cmEntry.vpn];
            DEBUG(dbgVM,  "ppn " << ppn << " <- " << cmEntry.ownerThread->getName() << 
                " vpn " << cmEntry.vpn << " last access " << cmEntry.lastAccessTick << 
                (cmEntry.lock ? " (locked)" : ""));
        }

        size_t leastTick = -1ull;
        int leastTickPpn = -1;
        for (unsigned int ppn = 0; ppn < NumPhysPages; ppn++) {
            CoreMapEntry& cmEntry = kernel->coreMap[ppn];
            if (cmEntry.lock) continue; // being swapped in, don't choose
            if (!cmEntry.ownerThread) { // free page
                leastTickPpn = ppn;
                break;
            }
            if (cmEntry.lastAccessTick < leastTick) {
                leastTick = cmEntry.lastAccessTick;
                leastTickPpn = ppn;
            }
        }
        return leastTickPpn;
    }
    case PageReplacementType::FIFO:
    default: {
        for (unsigned int ppn = 0; ppn < NumPhysPages; ppn++) {
            CoreMapEntry& cmEntry = kernel->coreMap[ppn];
            if (!cmEntry.ownerThread) {
                DEBUG(dbgVM, "ppn " << ppn << " is free" << (cmEntry.lock ? " (locked)" : ""));
                continue;
            }
            TranslationEntry* entry = &cmEntry.ownerThread->space->pageTable[cmEntry.vpn];
            DEBUG(dbgVM,  "ppn " << ppn << " <- " << cmEntry.ownerThread->getName() << 
                " vpn " << cmEntry.vpn << (cmEntry.lock ? " (locked)" : ""));
        }

        int pageToSwap = kernel->nextSwapPage;
        bool hasPageToSwap = 0;
        for (int i = 0; i < NumPhysPages; i++) {
            if (!kernel->coreMap[pageToSwap].lock) {
                hasPageToSwap = 1;
                break;
            }
            pageToSwap = (pageToSwap + 1) % NumPhysPages;
        }
        if (hasPageToSwap) {
            kernel->nextSwapPage = (pageToSwap + 1) % NumPhysPages;
            return pageToSwap;
        } else {
            return -1;
        }
    }
    }
}

void handlePageFault(TranslationEntry* requestEntry) {
    cerr << "page fault" << endl;
    int victimPpn = getPpnToSwap();
    if (victimPpn == -1) {
        DEBUG(dbgVM, "no page to swap!");
        return;
    }
    int srcSector = requestEntry->physicalPage;
    CoreMapEntry& cmEntry = kernel->coreMap[victimPpn];
    cmEntry.lock = 1; // lock this page during swap
    cout << "find vpn " << requestEntry->virtualPage << " at sector " << 
        srcSector << ", swap into ppn " << victimPpn << endl;
    if (cmEntry.ownerThread) { // need to swap out
        TranslationEntry* victimEntry = &cmEntry.ownerThread->space->pageTable[cmEntry.vpn];
        cout << "ppn " << victimPpn << " is occupied by " << 
            cmEntry.ownerThread->getName() << " vpn " << victimEntry->virtualPage << endl;
        
        ASSERT(!requestEntry->valid && victimEntry->valid); // one in disk, one in memory

        int destSector = kernel->disk->requestSector();
        DEBUG(dbgVM, "write victim ppn " << victimPpn << " to sector " << destSector);
        kernel->disk->WriteSector(
            destSector, 
            &kernel->machine->mainMemory[victimPpn * PageSize]
        );

        if (cmEntry.ownerThread) {
            victimEntry->physicalPage = destSector;
            victimEntry->valid = false;
    
            DEBUG(dbgVM, "after swap out, " << cmEntry.ownerThread->getName() << " vpn " << 
                victimEntry->virtualPage << " -> sector " 
                << victimEntry->physicalPage << " valid " << victimEntry->valid);
        } else {
            DEBUG(dbgVM, "thread exit during swap out");
        }
    }
    
    cmEntry.ownerThread = kernel->currentThread;
    cmEntry.vpn = requestEntry->virtualPage;
    requestEntry->physicalPage = victimPpn;
    requestEntry->valid = true;
    DEBUG(dbgVM, "write sector " << srcSector << " to ppn " << victimPpn);
    kernel->disk->ReadSector(
        srcSector, 
        &kernel->machine->mainMemory[victimPpn * PageSize]
    );
    kernel->disk->releaseSector(srcSector);
    DEBUG(dbgVM, "after swap in, vpn " << requestEntry->virtualPage << " -> ppn " 
        << requestEntry->physicalPage << " valid " << requestEntry->valid);
    cmEntry.lastAccessTick = kernel->stats->totalTicks;
    cmEntry.lock = 0; // unlock this page
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
	int	type = kernel->machine->ReadRegister(2);
	int	val;

    switch (which) {
	case SyscallException:
	    switch(type) {
		case SC_Halt:
		    DEBUG(dbgAddr, "Shutdown, initiated by user program.\n");
   		    kernel->interrupt->Halt();
		    break;
		case SC_PrintInt:
			val=kernel->machine->ReadRegister(4);
			cout << "Print integer:" <<val << endl;
			return;
/*		case SC_Exec:
			DEBUG(dbgAddr, "Exec\n");
			val = kernel->machine->ReadRegister(4);
			kernel->StringCopy(tmpStr, retVal, 1024);
			cout << "Exec: " << val << endl;
			val = kernel->Exec(val);
			kernel->machine->WriteRegister(2, val);
			return;
*/		case SC_Exit:
			DEBUG(dbgAddr, "Program exit\n");
			val=kernel->machine->ReadRegister(4);
			cout << "return value:" << val << endl;
			kernel->currentThread->Finish();
			break;
		default:
		    cerr << "Unexpected system call " << type << "\n";
 		    break;
	    }
	    break;
	case PageFaultException: {
        int vpn = kernel->machine->ReadRegister(BadVAddrReg) / PageSize;
        handlePageFault(&kernel->machine->pageTable[vpn]);
        kernel->stats->numPageFaults++;
        return;
    }
	default: {
	    cerr << "Unexpected user mode exception " << which << "\n";
        int va = kernel->machine->ReadRegister(BadVAddrReg);
        int pc = kernel->machine->ReadRegister(PCReg);
        int at = kernel->machine->ReadRegister(1);
        int instr;
        kernel->machine->ReadMem(pc, 4, &instr);
        DEBUG(dbgVM, "instr = " << hex << instr << dec);
        DEBUG(dbgVM, "atReg = " << at);
        DEBUG(dbgVM, "at " << kernel->currentThread->getName() << 
            " PC " << kernel->machine->ReadRegister(PCReg) <<
            " BadVAddr " << va << " = " << va / PageSize << ":" << va % PageSize);
	    break;
    }
    }
    ASSERTNOTREACHED();
}
