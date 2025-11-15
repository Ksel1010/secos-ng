/* GPLv2 (c) Airbus */
#include <debug.h>
#include <pagemem.h>
#include <cr.h>
#include <io.h>
#include <asm.h>
#include <intr.h>

#define PGD1_PHY    ((pde32_t*)0x610000) // physical address of page directory for task 1
#define PTB1_PHY    ((pte32_t*)0x611000) // physical address of page tables for task 1 
#define PGD2_PHY    ((pde32_t*)0x620000) // physical address of page directory for task 2
#define PTB2_PHY    ((pte32_t*)0x621000) // physical address of page tables for task 2

#define KERNEL_PGD  ((pde32_t*)0x600000) // physical address of page tables for the kernel
#define KERNEL_PTB  ((pte32_t*)0x601000) // physical address of page tables for the kernel

#define SHARED_PHY       0x400000 // page memory shared between the two tasks
#define SHARED1_VIRT     0x500000 // virual address for task 1 of the shared page 
#define SHARED2_VIRT     0x800000 // virual address for task 2 of the shared page 

#define STACK_KER1_PHY   0x710000 // kernel stack for task1
#define STACK_KER2_PHY   0x720000 // kernel stack for task2
#define STACK_USER1_PHY  0x730000 // user stack for task 1
#define STACK_USER2_PHY  0x740000 // user stack for task 2

#define c0_idx  1
#define d0_idx  2
#define c3_idx  3
#define d3_idx  4
#define ts_idx  5

#define c0_sel  gdt_krn_seg_sel(c0_idx)
#define d0_sel  gdt_krn_seg_sel(d0_idx)
#define c3_sel  gdt_usr_seg_sel(c3_idx)
#define d3_sel  gdt_usr_seg_sel(d3_idx)
#define ts_sel  gdt_krn_seg_sel(ts_idx)

seg_desc_t GDT[6];
tss_t      TSS;


#define gdt_flat_dsc(_dSc_,_pVl_,_tYp_)                                 \
   ({                                                                   \
      (_dSc_)->raw     = 0;                                             \
      (_dSc_)->limit_1 = 0xffff;                                        \
      (_dSc_)->limit_2 = 0xf;                                           \
      (_dSc_)->type    = _tYp_;                                         \
      (_dSc_)->dpl     = _pVl_;                                         \
      (_dSc_)->d       = 1;                                             \
      (_dSc_)->g       = 1;                                             \
      (_dSc_)->s       = 1;                                             \
      (_dSc_)->p       = 1;                                             \
   })

#define tss_dsc(_dSc_,_tSs_)                                            \
   ({                                                                   \
      raw32_t addr    = {.raw = _tSs_};                                 \
      (_dSc_)->raw    = sizeof(tss_t);                                  \
      (_dSc_)->base_1 = addr.wlow;                                      \
      (_dSc_)->base_2 = addr._whigh.blow;                               \
      (_dSc_)->base_3 = addr._whigh.bhigh;                              \
      (_dSc_)->type   = SEG_DESC_SYS_TSS_AVL_32;                        \
      (_dSc_)->p      = 1;                                              \
   })

#define c0_dsc(_d) gdt_flat_dsc(_d,0,SEG_DESC_CODE_XR)
#define d0_dsc(_d) gdt_flat_dsc(_d,0,SEG_DESC_DATA_RW)
#define c3_dsc(_d) gdt_flat_dsc(_d,3,SEG_DESC_CODE_XR)
#define d3_dsc(_d) gdt_flat_dsc(_d,3,SEG_DESC_DATA_RW)


void init_gdt() {
   gdt_reg_t gdtr;

   GDT[0].raw = 0ULL;

   c0_dsc( &GDT[c0_idx] );
   d0_dsc( &GDT[d0_idx] );
   c3_dsc( &GDT[c3_idx] );
   d3_dsc( &GDT[d3_idx] );

   TSS.s0.esp = get_ebp();
   TSS.s0.ss  = d0_sel;
   tss_dsc(&GDT[ts_idx], (offset_t)&TSS);

   gdtr.desc  = GDT;
   gdtr.limit = sizeof(GDT) - 1;
   set_gdtr(gdtr);
	
   set_cs(c0_sel);
   set_ss(d0_sel);
   set_ds(d0_sel);
   set_es(d0_sel);
   set_fs(d0_sel);
   set_gs(d0_sel);
   set_tr(ts_sel);

}

void init_pgd_with_ptb(pde32_t* pgd, pte32_t* ptb, pte32_t* virt, int level){
	int pgd_index = ((uint32_t)virt >> 22) & 0x3FF;
    pgd[pgd_index].addr = ((unsigned int)ptb >> 12) & 0xFFFFF;
    pgd[pgd_index].p = 1;
    pgd[pgd_index].rw = 1;
    pgd[pgd_index].lvl = level ? 1 : 0;
}

void syscall_isr_80() {
   asm volatile (
      "leave ; pusha        \n"
      "mov %esp, %eax      \n"
      "call syscall_handler \n"
      "popa ; iret"
      );
}

typedef struct {
    uint32_t* esp;
    uint32_t cr3;
} task_ctx_t;

task_ctx_t task1_ctx, task2_ctx;
task_ctx_t* current_task;

void sys_counter(uint32_t *counter){
	debug("sys counter\n");
    asm volatile("int $0x80" :: "a"(counter));
}

void user1(){
	unsigned int *shared_counter = (uint32_t *)SHARED1_VIRT;
	unsigned int i = *shared_counter;
	while(1){
		i++;
		*shared_counter = i;
		debug("user1\n");
		debug("%p = %d\n", shared_counter, i);
	}
}

void user2(){
	debug("user2\n");
	uint32_t *shared_counter = (uint32_t *)SHARED2_VIRT;
    while (1){
        sys_counter(shared_counter);  
	}
}

int init = 2;
uint32_t* espa;
uint32_t* eip;


void syscall_isr_32() {	
	asm volatile("pusha" : : : "memory");
	asm volatile("mov %%esp, %0" : "=r"(espa)); //save actual esp
	espa-=4;// car l'appel du syscal fait push ebp donc esp de la fonction d'avant est celui de syscall-4
    asm volatile("call change_task");
}

void change_task(){
	
	if (init){
		 //push registers
		init --;
	}else{
		current_task->esp = espa;
	}

    // Switch task
    current_task = (current_task == &task1_ctx) ? &task2_ctx : &task1_ctx;
    set_cr3(current_task->cr3);
	
	debug("cr3 = %x\n",get_cr3());
    // update esp in the TSS
    //TSS.s0.esp = (current_task == &task1_ctx) ? (STACK_KER1_PHY + 0x1000)
      //                                        : (STACK_KER2_PHY + 0x1000);
	if(init!=1){
		
	}
    asm volatile("mov %0, %%esp" : : "r"(current_task->esp) : "memory");
    // pass to user
	//debug("pass\n");
	outb(32, 32);
	//debug("pop\n");
	asm volatile("popa; iret" : : : "memory");

}


void __regparm__(1) syscall_handler(int_ctx_t *ctx){
	debug("syscall hndlr\n");
	uint32_t* counter = (uint32_t*) ctx->gpr.eax.raw;
	if((uint32_t)counter>=SHARED2_VIRT && (uint32_t)counter<(SHARED2_VIRT+0x1000)){
		debug("[SysCall] counter %p= %d\n", counter, *counter);
	}else{
		debug("Address given is out of the scope of this function\n");
	}
}

void prepare_initial_stack(uint32_t *stack_base, void (*entry)(void), task_ctx_t *ctx, uint32_t cr3)
{
	// This function would simulate the state of the stack for the first time as if it was prepared by an interruption
	uint32_t *sp = stack_base + 0x400; 
	*(--sp) = d3_sel;                              /* SS */
	*(--sp) = (uint32_t)( (uint8_t*)STACK_USER1_PHY  + 0x1000 ); /* ESP user (top) */
	*(--sp) = 0x202;                              /* EFLAGS (IF=1) */
	*(--sp) = c3_sel;                              /* CS */
	*(--sp) = (uint32_t)entry;                          /* EIP */
	debug("c3 sel = %x\n", c3_sel);

	for (int i =0; i<8; i++){
		*(--sp) = 0;
	}
    ctx->esp = sp;
    ctx->cr3 = cr3;
}

void tp() {
	/*Pagination*/
	//uint32_t cr3 = get_cr3(); // save register CR3 to store pgd

	//init kernel pgd with ptb
	init_pgd_with_ptb(KERNEL_PGD, KERNEL_PTB, KERNEL_PTB, 0);
	// init pgd1 with ptb1
	init_pgd_with_ptb(PGD1_PHY, PTB1_PHY, PTB1_PHY, 1);
	//init pgd2 with ptb2
	init_pgd_with_ptb(PGD2_PHY, PTB2_PHY, PTB2_PHY, 1);

	init_pgd_with_ptb(PGD1_PHY, PTB1_PHY, (pte32_t*)SHARED_PHY, 1);
	
	//set pgd kernel 
	set_cr3(KERNEL_PGD);


	// identity mapping kernal region in every PTB : adresses got from program headers
	for (int addr = 0x300000; addr < 0x306000; addr += 0x1000){
		PTB1_PHY[(addr>>12)&0x3FF].addr = addr>>12;
		PTB1_PHY[(addr>>12)&0x3FF].p = 1;
		PTB1_PHY[(addr>>12)&0x3FF].rw = 1;
		PTB1_PHY[(addr>>12)&0x3FF].lvl = 0;

		PTB2_PHY[(addr>>12)&0x3FF].addr = addr>>12;
		PTB2_PHY[(addr>>12)&0x3FF].p = 1;
		PTB2_PHY[(addr>>12)&0x3FF].rw = 1;
		PTB2_PHY[(addr>>12)&0x3FF].lvl = 0;

		KERNEL_PTB[(addr>>12)&0x3FF].addr = addr>>12;
		KERNEL_PTB[(addr>>12)&0x3FF].p = 1;
		KERNEL_PTB[(addr>>12)&0x3FF].rw = 1;
		KERNEL_PTB[(addr>>12)&0x3FF].lvl = 0;
	}

	//Identity mapping of tasks 1 and 2
	PTB1_PHY[((unsigned int)(&user1)>>12)&0x3FF].addr = (unsigned int)(&user1)>>12;
	PTB1_PHY[((unsigned int)(&user1)>>12)&0x3FF].p = 1;
	PTB1_PHY[((unsigned int)(&user1)>>12)&0x3FF].rw = 1;
	PTB1_PHY[((unsigned int)(&user1)>>12)&0x3FF].lvl = 1;

	PTB2_PHY[((unsigned int)(&user2)>>12)&0x3FF].addr = (unsigned int)(&user2)>>12;
	PTB2_PHY[((unsigned int)(&user2)>>12)&0x3FF].p = 1;
	PTB2_PHY[((unsigned int)(&user2)>>12)&0x3FF].rw = 1;
	PTB2_PHY[((unsigned int)(&user2)>>12)&0x3FF].lvl = 1;

	// Mapping user stacks : identity mapping
	PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].addr = STACK_USER1_PHY>>12;
	PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].p = 1;
	PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].rw = 1;
	PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].lvl = 1;

	PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].addr = STACK_USER2_PHY>>12;
	PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].p = 1;
	PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].rw = 1;
	PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].lvl = 1;

	// Mapping shared page with different virtual addr
	PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].addr = SHARED_PHY>>12;
	PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].p = 1;
	PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].rw = 1;
	PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].lvl = 1;

	PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].addr = SHARED_PHY>>12;
	PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].p = 1;
	PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].rw = 1;
	PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].lvl = 1;
	debug("before gdt\n");
	// Initialize the global descriptor table 
	init_gdt();
	debug("after gdt\n");
	/* Interruption initialization */

	// interruption 80
	idt_reg_t idt;
	get_idtr(idt);
	int_desc_t* desc_80 = &idt.desc[0x80];
	desc_80->offset_1 = ((unsigned int) (syscall_isr_80)) & 0x0000FFFF;
	desc_80->offset_2 = (((unsigned int) (syscall_isr_80))&0xFFFF0000 ) >>16;
	desc_80->dpl = 3;

	//interruption 32
	int_desc_t* desc_32 = &idt.desc[32];
	desc_32->offset_1 = ((unsigned int) (syscall_isr_32)) & 0x0000FFFF;
	desc_32->offset_2 = (((unsigned int) (syscall_isr_32))&0xFFFF0000 ) >>16;
	desc_32->dpl = 0;

	//Initialize the shared memory to 0
	for(int i = 0; i<1024; i++){
		*((char*)(SHARED_PHY+i))=0x0;
	}
	
	
	debug("main\n");

	//init tasks context structs
	task1_ctx.cr3 = (uint32_t)PGD1_PHY;
    task1_ctx.esp = (uint32_t*)STACK_USER1_PHY + 0x1000/4-1; //user stack pointer 

	task2_ctx.cr3 = (uint32_t)PGD2_PHY;
    task2_ctx.esp = (uint32_t*)STACK_USER2_PHY + 0x1000/4-1; // user stack pointer

	prepare_initial_stack((uint32_t*)STACK_USER1_PHY, user1, &task1_ctx, (uint32_t)PGD1_PHY);
	prepare_initial_stack((uint32_t*)STACK_USER2_PHY, user2, &task2_ctx, (uint32_t)PGD2_PHY);
	current_task = &task2_ctx;

	set_idtr(idt);
	set_cr3(KERNEL_PGD);
	force_interrupts_on();
	while(1);
}
