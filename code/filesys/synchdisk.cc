// synchdisk.cc 
//	Routines to synchronously access the disk.  The physical disk 
//	is an asynchronous device (disk requests return immediately, and
//	an interrupt happens later on).  This is a layer on top of
//	the disk providing a synchronous interface (requests wait until
//	the request completes).
//
//	Use a semaphore to synchronize the interrupt handlers with the
//	pending requests.  And, because the physical disk can only
//	handle one operation at a time, use a lock to enforce mutual
//	exclusion.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchdisk.h"


//----------------------------------------------------------------------
// SynchDisk::SynchDisk
// 	Initialize the synchronous interface to the physical disk, in turn
//	initializing the physical disk.
//
//	"name" -- UNIX file name to be used as storage for the disk data
//	   (usually, "DISK")
//----------------------------------------------------------------------

SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    disk = new Disk(name, this);
    for (int i = 0; i < NumSectors; i++) {
        used[i] = false;
    }
}

//----------------------------------------------------------------------
// SynchDisk::~SynchDisk
// 	De-allocate data structures needed for the synchronous disk
//	abstraction.
//----------------------------------------------------------------------

SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
}

//----------------------------------------------------------------------
// SynchDisk::ReadSector
// 	Read the contents of a disk sector into a buffer.  Return only
//	after the data has been read.
//
//	"sectorNumber" -- the disk sector to read
//	"data" -- the buffer to hold the contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::WriteSector
// 	Write the contents of a buffer into a disk sector.  Return only
//	after the data has been written.
//
//	"sectorNumber" -- the disk sector to be written
//	"data" -- the new contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
}

void SynchDisk::Swap(TranslationEntry* entry1, TranslationEntry* entry2) {
    lock->Acquire();
    ASSERT(!entry1->valid && entry2->valid); // one in disk, one in memory
    int sectorToSwap = entry1->physicalPage;
    int pageToSwap = entry2->physicalPage;
    swap(entry1->physicalPage, entry2->physicalPage);
    swap(entry1->valid, entry2->valid);
    kernel->coreMapEntry[pageToSwap] = entry1;

    char data[PageSize];
    DEBUG(dbgVM, "read sector " << sectorToSwap << " to data");
    disk->ReadRequest(sectorToSwap, data);
    semaphore->P();
    DEBUG(dbgVM, "write ppn " << pageToSwap << " to sector " << sectorToSwap);
    disk->WriteRequest(
        sectorToSwap,
        &kernel->machine->mainMemory[pageToSwap * PageSize]
    );
    DEBUG(dbgVM, "write data to ppn " << pageToSwap);
    for (int i = 0; i < PageSize; i++) {
        kernel->machine->mainMemory[pageToSwap * PageSize + i] = data[i];
    }
    semaphore->P();
    lock->Release();
}

int SynchDisk::requestSector() {
    for (int i = 0; i < NumSectors; i++) {
        if (used[i]) continue;
        used[i] = true;
        return i;
    }
}

void SynchDisk::releaseSector(int sectorNumber) {
    used[sectorNumber] = false;
}

//----------------------------------------------------------------------
// SynchDisk::CallBack
// 	Disk interrupt handler.  Wake up any thread waiting for the disk
//	request to finish.
//----------------------------------------------------------------------

void
SynchDisk::CallBack()
{ 
    semaphore->V();
}
