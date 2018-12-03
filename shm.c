#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;	// Specify shared memory segment
    char *frame;	// Pointer to physical frame that will be shared
    int refcnt;		// Number of processes sharing the page
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

int shm_open(int id, char **pointer) {

  uint i;
  uint exists = 0; 	        // used to see if page already is being shared
  uint firstFreeAddress = 100;
  uint va;	// Free virtual address to map page to
  uint indexPage; 
  char* mem;

  // check if id already exists
  acquire(&(shm_table.lock));

  // Search table to see if the id already exists
  for(i = 0; i < 64; i+=1){
    if (shm_table.shm_pages[i].id == id){
      indexPage = i; 	// We found the page
      exists = 1;
    }
    else{
       if (shm_table.shm_pages[i].frame == 0 && firstFreeAddress == 100){ // This is a free table
         firstFreeAddress = i;
       } 
    }
  }
  release(&(shm_table.lock));

  // Case 1: Id for page exists
  if (exists) {
    acquire(&(shm_table.lock));

    va = PGROUNDUP(myproc()->sz);
    mappages(myproc()->pgdir, (char *)va, PGSIZE, 
	     V2P(shm_table.shm_pages[indexPage].frame), PTE_W | PTE_U);
    ++(shm_table.shm_pages[indexPage].refcnt); // Increment refcnt
    *pointer = (char *)va;  // Return pointer to the virtual adress
    myproc()->sz = va;
    myproc()->sz += PGSIZE;

    release(&(shm_table.lock));
    return 0;
  }
  
  // Case 2: Id for page doesnt exist, first to shm_open
  else {
    mem = kalloc();
    cprintf("%d\n", mem);
    memset(mem, 0, PGSIZE);
    acquire(&(shm_table.lock));

    va = PGROUNDUP(myproc()->sz);
    shm_table.shm_pages[firstFreeAddress].frame = mem;
    cprintf("%d\n", shm_table.shm_pages[firstFreeAddress].frame);
    if(mappages(myproc()->pgdir, (char *)va, PGSIZE, 
	     V2P(shm_table.shm_pages[firstFreeAddress].frame), PTE_W | PTE_U) < 0){
	cprintf("allocuvm out of memory\n");
    }
    shm_table.shm_pages[firstFreeAddress].id = id;

    *pointer = (char *)va; // Return pointer to the virtual address
    shm_table.shm_pages[firstFreeAddress].refcnt = 1; // Increment refcnt
    myproc()->sz = va;
    myproc()->sz += PGSIZE;

    release(&(shm_table.lock));
    return 0;
  }

  //myproc()->sz = va;
  return -1; //added to remove compiler warning -- you should decide what to return
}


int shm_close(int id) {
  uint i;
  // Search through table to close the proper page
  for (i = 0; i < 64; i += 1){
    if (shm_table.shm_pages[i].id == id){ // Close this page
      acquire(&(shm_table.lock));
      shm_table.shm_pages[i].refcnt -= 1;
      if (shm_table.shm_pages[i].refcnt == 0){ // We are last page to close, clear everything
        shm_table.shm_pages[i].id =0;
        shm_table.shm_pages[i].frame =0;
        shm_table.shm_pages[i].refcnt =0;

      }
      release(&(shm_table.lock));
      return 0; 
    }
  }
  return -1;  // if this is reached, we couldnt close the page
}
