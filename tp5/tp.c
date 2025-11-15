/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>

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

   gdtr.desc  = GDT;
   gdtr.limit = sizeof(GDT) - 1;
   set_gdtr(gdtr);

   set_cs(c0_sel);

   set_ss(d0_sel);
   set_ds(d0_sel);
   set_es(d0_sel);
   set_fs(d0_sel);
   set_gs(d0_sel);
}

void syscall_isr() {
   asm volatile (
      "leave ; pusha        \n"
      "mov %esp, %eax      \n"
      "call syscall_handler \n"
      "popa ; iret"
      );
}

void __regparm__(1) syscall_handler(int_ctx_t *ctx) {
   //debug("SYSCALL eax = %p\n", (void *) ctx->gpr.eax.raw);
if ( (
        ( (unsigned int)ctx->gpr.esi.raw >= ( ((unsigned int)(GDT[d3_idx].base_1
                                                     | ((unsigned int)GDT[d3_idx].base_2 << 16)
                                                     | ((unsigned int)GDT[d3_idx].base_3 << 24)))
                                                  << 12) )
      && ( (unsigned int)ctx->gpr.esi.raw <= ( ((((unsigned int)GDT[d3_idx].limit_1
                                              | ((unsigned int)GDT[d3_idx].limit_2 << 16)) << 12) | 0xFFF) ) )
     )
  ||
     ( ( (unsigned int)ctx->gpr.esi.raw >= ( ((unsigned int)(GDT[c3_idx].base_1
                                                     | ((unsigned int)GDT[c3_idx].base_2 << 16)
                                                     | ((unsigned int)GDT[c3_idx].base_3 << 24)))
                                                  << 12) )
      && ( (unsigned int)ctx->gpr.esi.raw <= ( ((((unsigned int)GDT[c3_idx].limit_1
                                              | ((unsigned int)GDT[c3_idx].limit_2 << 16)) << 12) | 0xFFF) ) )
     )
   ){
      debug("SYSCALL: message depuis userland -> %s\n", (const char *) ctx->gpr.esi.raw);
   }
   else{
      debug("@ out of scope : seg fault\n");
   }
   //debug("base = %ld and limit = %lx \n ", (unsigned long int) (GDT[d3_idx].base_1 | ( GDT[d3_idx].base_2<<16) | (GDT[d3_idx].base_3<<24)), (unsigned long int) (GDT[d3_idx].limit_1 | (GDT[d3_idx].limit_2<<16)));
}

void userland() {
   //uint32_t arg =  0x2023;
   const char *str = "chaine du ring 3 !";
   debug("\n\nuserland\n\n");
   debug("@ str = %p\n", str);
   debug("granularité = %d\n",GDT[c3_idx].g );
   //asm volatile ("int $48"::"S"(str));
   asm volatile ("int $48"::"S"(0xffffffff));
   while(1);
}

void tp() {
   // Q1
   init_gdt();
   set_ds(d3_sel);
   set_es(d3_sel);
   set_fs(d3_sel);
   set_gs(d3_sel);
   // Note: TSS is needed for the "kernel stack"
   // when returning from ring 3 to ring 0
   // during a next interrupt occurence
   TSS.s0.esp = get_ebp();
   TSS.s0.ss  = d0_sel;
   tss_dsc(&GDT[ts_idx], (offset_t)&TSS);
   set_tr(ts_sel);
   // end Q1

   idt_reg_t idt;
	get_idtr(idt);
	debug("idt addr = 0x%ld\nlimit idt = %d\n", idt.addr, idt.limit);
	int_desc_t* desc = &idt.desc[48];
	desc->offset_1 = ((unsigned int) (syscall_isr)) & 0x0000FFFF;
	desc->offset_2 = (((unsigned int) (syscall_isr))&0xFFFF0000 ) >>16;
	desc->dpl = 3;
   set_idtr(idt);
   // Q2
   asm volatile (
   "push %0    \n" // ss
   "push %%ebp \n" // esp
   "pushf      \n" // eflags
   "push %1    \n" // cs
   "push %2    \n" // eip
   // end Q2
   // Q3
   "iret"
   ::
    "i"(d3_sel),
    "i"(c3_sel),
    "r"(&userland)
   );
   // end Q3
}
