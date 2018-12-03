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

  uint caseNumber = 2;
  uint i;
  uint indexPage; 
  uint firstFreeAddress = 0;
  uint va = PGROUNDUP(myproc()->sz);
  char* mem;

  // check if id already exists
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {  
    if (shm_table.shm_pages[i].id == id) {
      caseNumber = 1; 
      indexPage = i;
    }
    if ((shm_table.shm_pages[i].id == 0) && (firstFreeAddress == 0)){
	firstFreeAddress = i;
      }
  }
/*
  // Search table to see if the id already exists
  for(i = 0; i < 64; i+=1){
    if (shm_table.shm_pages[i].id == id){
      indexPage = i;
    }
  }
*/
  release(&(shm_table.lock));
  cprintf("meme\n");

  // id we are opening already exists. map the page 
  if (caseNumber == 1) {
    acquire(&(shm_table.lock));
    mappages(myproc()->pgdir, (char *)va, PGSIZE, 
	     V2P(shm_table.shm_pages[indexPage].frame), PTE_W | PTE_U);
    ++(shm_table.shm_pages[indexPage].refcnt);
    release(&(shm_table.lock));
  }
  
  // id we want to open does not exist
  else if (caseNumber == 2) {
    mem = kalloc();
    memset(mem, 0, PGSIZE);
    acquire(&(shm_table.lock));
    shm_table.shm_pages[firstFreeAddress].frame = mem;
    mappages(myproc()->pgdir, (char *)va, PGSIZE, 
	     V2P(shm_table.shm_pages[firstFreeAddress].frame), PTE_W | PTE_U);
    ++(shm_table.shm_pages[firstFreeAddress].refcnt);
    shm_table.shm_pages[firstFreeAddress].id = id;
    release(&(shm_table.lock));
  }

  *pointer = (char *)va;
  myproc()->sz = va;

return 0; //added to remove compiler warning -- you should decide what to return
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
