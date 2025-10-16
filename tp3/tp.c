/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>
#include <string.h>

void userland() {
   //asm volatile ("mov %eax, %cr0");
   debug("USERLAAAND\n");
}


void tp() {
    gdt_reg_t gdtr_ptr;

    get_gdtr(gdtr_ptr);
    debug("gdtr = 0x%lx\n", gdtr_ptr.addr);
    uint16_t ds = get_ds();
    debug("ds = 0x%x\n", ds);

    uint16_t ss = get_ss();
    debug("ss = 0x%x\n", ss);

    uint16_t _cs = get_seg_sel(cs);
    debug("cs = 0x%x\n", _cs);

    uint16_t es = get_es();
    debug("es = 0x%x\n", es);

    uint16_t fs = get_fs();
    debug("fs = 0x%x\n", fs);

    uint16_t gs = get_gs();
    debug("gs = 0x%x\n", gs);

    debug("------------------------------init----------------------------------\n");

    seg_desc_t* my_gdt = (seg_desc_t*)  0x100000;
    memset(my_gdt, 0, sizeof(seg_desc_t) * 6);

    gdt_reg_t new_gdt_ptr ={
        .limit = 0x2f,
        .desc = my_gdt
    };

    my_gdt[1].limit_1 = 0xffff;
    my_gdt[1].base_1 = 0x0000;
    my_gdt[1].base_2 = 0x00;
    my_gdt[1].base_3 = 0x00;
    my_gdt[1].type = 0xa; //1010 segment de code : 4eme bit à 1
    my_gdt[1].avl = 0x00; // available 
    my_gdt[1].d = 1; //taille des operandes par defaut : 1 = 32bits ety 0 = 16bits
    my_gdt[1].g = 1; // granularité
    my_gdt[1].limit_2 = 0xf;
    my_gdt[1].p = 1; // utilisé (present)
    my_gdt[1].s = 0x1; //type descriptor 1 code/data et 0 system
    my_gdt[1].dpl = 0x0; // code ring 0
    my_gdt[1].l = 0x0; // long mode 1 à 64bits et 0 32bits

    my_gdt[2].limit_1 = 0xffff;
    my_gdt[2].base_1 = 0x0000;
    my_gdt[2].base_2 = 0x00;
    my_gdt[2].base_3 = 0x00;
    my_gdt[2].type = 0x2; //0010 segment de data : 4eme bit à 0
    my_gdt[2].avl = 0x00;
    my_gdt[2].d = 0x1;
    my_gdt[2].g = 0x1;
    my_gdt[2].limit_2 = 0xf;
    my_gdt[2].p = 0x1;
    my_gdt[2].s = 0x1;
    my_gdt[2].dpl = 0x0;
    my_gdt[2].l = 0x0;


    my_gdt[3].limit_1 = 0x1f;
    my_gdt[3].base_1 = 0x0000;
    my_gdt[3].base_2 = 0x60;
    my_gdt[3].base_3 = 0x00;
    my_gdt[3].type = 0x2; //0010 segment de data : 4eme bit à 0
    my_gdt[3].avl = 0x00;
    my_gdt[3].d = 0x1;
    my_gdt[3].g = 0x0;
    my_gdt[3].limit_2 = 0x0;
    my_gdt[3].p = 0x1;
    my_gdt[3].s = 0x1;
    my_gdt[3].dpl = 0x0;
    my_gdt[3].l = 0x0;

    

    my_gdt[4].limit_1 = 0xffff;
    my_gdt[4].base_1 = 0x0000;
    my_gdt[4].base_2 = 0x00;
    my_gdt[4].base_3 = 0x00;
    my_gdt[4].type = 0xa; //1010 segment de code : 4eme bit à 1
    my_gdt[4].avl = 0x00; // available 
    my_gdt[4].d = 1; //taille des operandes par defaut : 1 = 32bits ety 0 = 16bits
    my_gdt[4].g = 1; // granularité
    my_gdt[4].limit_2 = 0xf;
    my_gdt[4].p = 1; // utilisé (present)
    my_gdt[4].s = 0x1; //type descriptor 1 code/data et 0 system
    my_gdt[4].dpl = 0x3; // code ring 3
    my_gdt[4].l = 0x0; // long mode 1 à 64bits et 0 32bits

    my_gdt[5].limit_1 = 0xffff;
    my_gdt[5].base_1 = 0x0000;
    my_gdt[5].base_2 = 0x00;
    my_gdt[5].base_3 = 0x00;
    my_gdt[5].type = 0x2; //0010 segment de data : 4eme bit à 0
    my_gdt[5].avl = 0x00;
    my_gdt[5].d = 0x1;
    my_gdt[5].g = 0x1;
    my_gdt[5].limit_2 = 0xf;
    my_gdt[5].p = 0x1;
    my_gdt[5].s = 0x1;
    my_gdt[5].dpl = 0x3;
    my_gdt[5].l = 0x0;

    set_gdtr(new_gdt_ptr);
    set_ds(16);
    set_cs(8);
    set_es(24);
    set_ss(16); // ss ne peut pas etre un seg de code 
    set_fs(16);
    set_gs(16);

    //set_cs(8*4);
    set_ds(8*5);

    unsigned int cs_3 = 8;
    unsigned int eip = (unsigned int) userland;

    asm volatile(
      "push %[ss]\n\t"
      "push %%esp \n\t"
      "pushf\n\t"
      "push %[cs_3]\n\t"
      "push %[eip]\n\t"
      ::
      [ss]"r"(ss), [cs_3]"r"(cs_3), [eip]"r"(eip)
      :"memory"
    );

    asm volatile(
      "iret\n\t"
    );        
}