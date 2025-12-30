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

void user1() __attribute__((section(".user1"))); //precise physical addresses for user 1 and 2 and sys_counter as it is called by user2
void user2() __attribute__((section(".user2")));
void sys_counter(uint32_t *counter) __attribute__((section(".user2")));

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
int current=0;

static void launch_task(pde32_t *pgd, void (*user_fn)(void),
                        unsigned int user_stack_top, unsigned int kstack_top) {
    set_cr3(pgd);

    /* config TSS */
    TSS.s0.esp = kstack_top;
    TSS.s0.ss  = d0_sel; 

    // push user SS, ESP, EFLAGS, CS, EIP puis iret 
	//current=1; // tell that we execute the function user1
    asm volatile (
        "cli\n\t"
        "push %0\n\t"     
        "push %1\n\t"     
        "pushf\n\t"
		"orl $0x200, (%%esp)\n\t" 
        "push %2\n\t"
        "push %3\n\t"
        "movl $1, current\n\t"
		"iret\n\t"
        :
        : "i"(d3_sel), "r"(user_stack_top), "i"(c3_sel), "r"(user_fn)
        : "memory"
    );
}

void init_pgd_with_ptb(pde32_t* pgd, pte32_t* ptb, pte32_t* virt, int user){
	//initialize the pgd with the ptb 
	int pgd_index = ((uint32_t)virt >> 22) & 0x3FF;
    pgd[pgd_index].addr = ((unsigned int)ptb >> 12) & 0xFFFFF;
    pgd[pgd_index].p = 1;
    pgd[pgd_index].rw = 1;
    pgd[pgd_index].lvl = user ? 1 : 0;
}

__attribute__((naked)) 
void syscall_isr_80() {
   asm volatile (
      "pusha        \n"
      "mov %esp, %eax      \n"
      "call syscall_handler \n"
      "popa ; iret"
      );
}


typedef struct {
    uint32_t esp;
    uint32_t kstack_top;
} task_t;

task_t task1, task2; // to store the esp user for each function along with the kernal stack


void sys_counter(uint32_t *counter){
	if((uint32_t)counter>=SHARED2_VIRT && (uint32_t)counter<(SHARED2_VIRT+0x1000)){
    	asm volatile("int $0x80" :: "a"(counter));
	}else{
		//debug("Address given is out of the scope of this function\n");
	}
}
void user1(){
	unsigned int *shared_counter = (uint32_t *)SHARED1_VIRT;
	unsigned int i = *shared_counter;
	while(1){
		i++;
		*shared_counter = i;
	}
}

void user2(){
	uint32_t *shared_counter = (uint32_t *)SHARED2_VIRT;
    while (1){
        sys_counter(shared_counter);  
	}
}


uint32_t next_esp, current_esp;
void change_task(void) {
	debug("cr3 = 0x%x\n", get_cr3());
    if (current == 1) {
        task1.esp = current_esp;
        current = 2;
        next_esp = task2.esp;
        TSS.s0.esp = task2.kstack_top;
        set_cr3(PGD2_PHY);
    } else { 
		if (current==2){
			task2.esp = current_esp;
        	current = 1;
        	next_esp = task1.esp;
        	TSS.s0.esp = task1.kstack_top;
        	set_cr3(PGD1_PHY);
		}
		else{
			next_esp = current_esp;
		}
    }
}
__attribute__((naked))
void syscall_isr_32() {
	asm volatile(
		"pusha\n\t"
		"mov %esp, current_esp\n\t"
		"call change_task\n\t"
		"mov next_esp, %esp\n\t"
		"popa \n\t"
		"movb $0x20, %al\n\t"
        "outb %al, $0x20\n\t"
        "sti\n\t"
		"iret");
}


void __regparm__(1) syscall_handler(int_ctx_t *ctx){
	debug("[SysCall] counter %p = 0x%x\n", (int*)ctx->gpr.eax.raw, *((int*)SHARED2_VIRT));
}

void prepare_initial_stack(uint32_t *stack_base, void (*entry)(void), task_t *ctx) {
	//simuler lz pile suite a un appel a la fonction 
    uint32_t* sp = stack_base; 
    *(--sp) = d3_sel;                    
    *(--sp) = (uint32_t)(stack_base ); 
    *(--sp) = 0x202;                      
    *(--sp) = c3_sel;                     
    *(--sp) = (uint32_t)entry;            

    // Initialiser les registres généraux 
    for (int i = 0; i < 8; i++) *(--sp) = 0;

    ctx->esp = (uint32_t)sp;
}



void tp() {
    // Pagination

	//init pgd
    init_pgd_with_ptb(KERNEL_PGD, KERNEL_PTB, (pte32_t*)0x0, 0);
    init_pgd_with_ptb(PGD1_PHY, PTB1_PHY, (pte32_t*)0x0, 1);
    init_pgd_with_ptb(PGD1_PHY, PTB1_PHY, (pte32_t*)0x400000, 1); 
    init_pgd_with_ptb(PGD1_PHY, PTB1_PHY, (pte32_t*)0x800000, 1); 
    init_pgd_with_ptb(PGD2_PHY, PTB2_PHY, (pte32_t*)0x0, 1);
    init_pgd_with_ptb(PGD2_PHY, PTB2_PHY, (pte32_t*)0x400000, 1);
    init_pgd_with_ptb(PGD2_PHY, PTB2_PHY, (pte32_t*)0x800000, 1);

    // Pagination 
    for (int addr = 0x600000; addr <= 0x621000; addr += 0x1000) {
        int idx = (addr>>12)&0x3FF;
        KERNEL_PTB[idx].addr = addr>>12; KERNEL_PTB[idx].p = 1; KERNEL_PTB[idx].rw = 1; KERNEL_PTB[idx].lvl = 0;
        PTB1_PHY[idx].addr = addr>>12; PTB1_PHY[idx].p = 1; PTB1_PHY[idx].rw = 1; PTB1_PHY[idx].lvl = 0;
        PTB2_PHY[idx].addr = addr>>12; PTB2_PHY[idx].p = 1; PTB2_PHY[idx].rw = 1; PTB2_PHY[idx].lvl = 0;
    }

    // map kernel and low memory
    for (int addr = 0x0; addr < 0x100000; addr += 0x1000){
        int idx = (addr>>12)&0x3FF;
        PTB1_PHY[idx].addr = addr>>12; PTB1_PHY[idx].p = 1; PTB1_PHY[idx].rw = 1; PTB1_PHY[idx].lvl = 0;
        PTB2_PHY[idx].addr = addr>>12; PTB2_PHY[idx].p = 1; PTB2_PHY[idx].rw = 1; PTB2_PHY[idx].lvl = 0;
        KERNEL_PTB[idx].addr = addr>>12; KERNEL_PTB[idx].p = 1; KERNEL_PTB[idx].rw = 1; KERNEL_PTB[idx].lvl = 0;
    }
    for (int addr = 0x300000; addr < 0x30A000; addr += 0x1000){
        int idx = (addr>>12)&0x3FF;
        PTB1_PHY[idx].addr = addr>>12; PTB1_PHY[idx].p = 1; PTB1_PHY[idx].rw = 1; PTB1_PHY[idx].lvl = 0;
        PTB2_PHY[idx].addr = addr>>12; PTB2_PHY[idx].p = 1; PTB2_PHY[idx].rw = 1; PTB2_PHY[idx].lvl = 0;
        KERNEL_PTB[idx].addr = addr>>12; KERNEL_PTB[idx].p = 1; KERNEL_PTB[idx].rw = 1; KERNEL_PTB[idx].lvl = 0;
    }

    /* map the code for each function*/
	// user1
    PTB1_PHY[(0x810000>>12)&0x3FF].addr = 0x810000>>12; PTB1_PHY[(0x810000>>12)&0x3FF].p = 1;
    PTB1_PHY[(0x810000>>12)&0x3FF].rw = 1; PTB1_PHY[(0x810000>>12)&0x3FF].lvl = 1;
    
	//user2
    PTB2_PHY[(0x820000>>12)&0x3FF].addr = 0x820000>>12; PTB2_PHY[(0x820000>>12)&0x3FF].p = 1;
    PTB2_PHY[(0x820000>>12)&0x3FF].rw = 1; PTB2_PHY[(0x820000>>12)&0x3FF].lvl = 1;

    // map the stacks
   
    // kernel stacks  mapped everywhere
    PTB1_PHY[(STACK_KER1_PHY>>12)&0x3FF].addr = STACK_KER1_PHY>>12;
    PTB1_PHY[(STACK_KER1_PHY>>12)&0x3FF].p = 1;
    PTB1_PHY[(STACK_KER1_PHY>>12)&0x3FF].rw = 1;
    PTB1_PHY[(STACK_KER1_PHY>>12)&0x3FF].lvl = 0;
    
    PTB2_PHY[(STACK_KER1_PHY>>12)&0x3FF].addr = STACK_KER1_PHY>>12;
    PTB2_PHY[(STACK_KER1_PHY>>12)&0x3FF].p = 1;
    PTB2_PHY[(STACK_KER1_PHY>>12)&0x3FF].rw = 1;
    PTB2_PHY[(STACK_KER1_PHY>>12)&0x3FF].lvl = 0;
    
    KERNEL_PTB[(STACK_KER1_PHY>>12)&0x3FF].addr = STACK_KER1_PHY>>12;
    KERNEL_PTB[(STACK_KER1_PHY>>12)&0x3FF].p = 1;
    KERNEL_PTB[(STACK_KER1_PHY>>12)&0x3FF].rw = 1;
    KERNEL_PTB[(STACK_KER1_PHY>>12)&0x3FF].lvl = 0;
    
    PTB1_PHY[(STACK_KER2_PHY>>12)&0x3FF].addr = STACK_KER2_PHY>>12;
    PTB1_PHY[(STACK_KER2_PHY>>12)&0x3FF].p = 1;
    PTB1_PHY[(STACK_KER2_PHY>>12)&0x3FF].rw = 1;
    PTB1_PHY[(STACK_KER2_PHY>>12)&0x3FF].lvl = 0;
    
    PTB2_PHY[(STACK_KER2_PHY>>12)&0x3FF].addr = STACK_KER2_PHY>>12;
    PTB2_PHY[(STACK_KER2_PHY>>12)&0x3FF].p = 1;
    PTB2_PHY[(STACK_KER2_PHY>>12)&0x3FF].rw = 1;
    PTB2_PHY[(STACK_KER2_PHY>>12)&0x3FF].lvl = 0;
    
    KERNEL_PTB[(STACK_KER2_PHY>>12)&0x3FF].addr = STACK_KER2_PHY>>12;
    KERNEL_PTB[(STACK_KER2_PHY>>12)&0x3FF].p = 1;
    KERNEL_PTB[(STACK_KER2_PHY>>12)&0x3FF].rw = 1;
    KERNEL_PTB[(STACK_KER2_PHY>>12)&0x3FF].lvl = 0;
    
    // stacks for each user function only mapped to their ptb
    PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].addr = STACK_USER1_PHY>>12;
    PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].p = 1;
    PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].rw = 1;
    PTB1_PHY[(STACK_USER1_PHY>>12)&0x3FF].lvl = 1;
    
    PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].addr = STACK_USER2_PHY>>12;
    PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].p = 1;
    PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].rw = 1;
    PTB2_PHY[(STACK_USER2_PHY>>12)&0x3FF].lvl = 1;

    // map the shared memory
    PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].addr = SHARED_PHY>>12; PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].p = 1;
    PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].rw = 1; PTB1_PHY[(SHARED1_VIRT>>12)&0x3FF].lvl = 1;
    
    PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].addr = SHARED_PHY>>12; PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].p = 1;
    PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].rw = 1; PTB2_PHY[(SHARED2_VIRT>>12)&0x3FF].lvl = 1;

    // init shared memory to 0
    for(int i = 0; i < 4096; i++) *((char*)(SHARED_PHY + i)) = 0;

    debug("Initialisation GDT\n");
    init_gdt();

    idt_reg_t idt;
    get_idtr(idt);
    
    int_desc_t* desc_80 = &idt.desc[0x80];
    desc_80->offset_1 = ((unsigned int)(syscall_isr_80)) & 0xFFFF;
    desc_80->offset_2 = ((unsigned int)(syscall_isr_80)) >> 16;
    desc_80->selector = c0_sel; desc_80->type = 0xE; desc_80->dpl = 3; desc_80->p = 1;

    int_desc_t* desc_32 = &idt.desc[32];
    desc_32->offset_1 = ((unsigned int)(syscall_isr_32)) & 0xFFFF;
    desc_32->offset_2 = ((unsigned int)(syscall_isr_32)) >> 16;
    desc_32->selector = c0_sel; desc_32->type = 0xE; desc_32->dpl = 0; desc_32->p = 1;
	debug("Initialisation IDT\n");
    set_idtr(idt);

    task1.kstack_top = STACK_KER1_PHY + PAGE_SIZE;
    task2.kstack_top = STACK_KER2_PHY + PAGE_SIZE;
	//prepare the stack user2 to simulate an already called function as we cant call bth user1 and2 in C
    prepare_initial_stack((uint32_t*)(STACK_USER2_PHY + PAGE_SIZE), (void(*)(void))0x820000, &task2);

    // enable pagination
    set_cr3(KERNEL_PGD);
    debug("PAGINATION...\n");
    set_cr0(get_cr0() | (1UL << 31));
    debug("Pagination activated\n");

    debug("Launch user1()\n");
    launch_task((pde32_t*)PGD1_PHY, (void(*)(void))0x810000, 
                STACK_USER1_PHY + PAGE_SIZE, STACK_KER1_PHY + PAGE_SIZE);

    while(1);
}