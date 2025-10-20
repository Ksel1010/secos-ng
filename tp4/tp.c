/* GPLv2 (c) Airbus */
#include <debug.h>
#include <cr.h>
#include <pagemem.h>
#include <types.h>

void tp() {
	uint32_t cr3 = get_cr3();
	debug("addr CR3 = 0x%x\n", cr3); //addr CR3 = 0x0

	//Q2
	pde32_t* pgd = (pde32_t*)0x600000;
	//set_cr3(pgd);
	debug("addr CR3 = 0x%x\n", get_cr3());
	//Q3
	//set_cr0(get_cr0() | (0x1<<31));
	//debug("cr0.PG = %d\n", 0x1 & get_cr0());

	//Q4
	pte32_t* ptb = (pte32_t*)0x601000;

	
	pgd[0].addr = (unsigned int)ptb>>12; // adresse sur les bits 31:12
	pgd[0].p = 0x1; // page present
	pgd[0].rw = 0x1;
	debug("premiere entree de pgd : 0x%x \n", (unsigned int) pgd[0].addr);
	set_cr3(pgd);

	//Q5
	//  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
	//  LOAD           0x000094 0x00300000 0x00300000 0x0000c 0x0000c RWE 0x4
	ptb[0x00300000>>12].addr = 0x00300000>>12;
	ptb[0x00300000>>12].p = 1;
	ptb[0x00300000>>12].rw = 1;
	//  LOAD           0x000000 0x00300010 0x00300010 0x00000 0x02000 RW  0x10
	ptb[0x00301000>>12].addr = 0x00301000>>12;
	ptb[0x00301000>>12].p = 1;
	ptb[0x00301000>>12].rw = 1;
	
	//  LOAD           0x0000b0 0x00302010 0x00302010 0x02a00 0x03630 RWE 0x20
	ptb[0x00302010>>12].addr = 0x00302010>>12;
	ptb[0x00302010>>12].p = 1;
	ptb[0x00302010>>12].rw = 1;
	
	ptb[0x00303010>>12].addr = 0x00303010>>12;
	ptb[0x00303010>>12].p = 1;
	ptb[0x00303010>>12].rw = 1;

	ptb[0x00304010>>12].addr = 0x00304010>>12;
	ptb[0x00304010>>12].p = 1;
	ptb[0x00304010>>12].rw = 1;

	ptb[0x00305010>>12].addr = 0x00305010>>12;
	ptb[0x00305010>>12].p = 1;
	ptb[0x00305010>>12].rw = 1;

	ptb[0x00306010>>12].addr = 0x00306010>>12;
	ptb[0x00306010>>12].p = 1;
	ptb[0x00306010>>12].rw = 1;

		//Q6
	//debug("afficher le contenu du premier PTE: @ = 0x%x\n", (unsigned int)pgd[0].addr); //error page fault
	pte32_t* ptb_2 = (pte32_t*)0x00400000;
	pgd[1].addr = (unsigned int)ptb_2>>12;
	pgd[1].p = 1;
	pgd[1].rw = 1;
	
	ptb_2[(0x600000>>12)&0x3FF].addr = 0x600000>>12;
	ptb_2[(0x600000>>12)&0x3FF].p = 1;
	ptb_2[(0x600000>>12)&0x3FF].rw = 1;
	
	ptb_2[(0x601000>>12)&0x3FF].addr = 0x601000>>12;
	ptb_2[(0x601000>>12)&0x3FF].p = 1;
	ptb_2[(0x601000>>12)&0x3FF].rw = 1;
	//todo : voir pourquoi l'@ 0x601000 est elle accessible meme quand je ne fais pas son mapping mais je fais celui de 0x600000 
	debug("afficher le contenu du premier PTE: @ = 0x%x\n", (unsigned int)pgd[0].addr); //error page fault
	//Fin Q6	

	//Q7
	pgd[768].addr = (unsigned int)ptb_2>>12;
	pgd[768].p = 1;
	pgd[768].rw = 1;

	ptb_2[0].addr = (unsigned int) pgd>>12;
	ptb_2[0].p = 1;
	ptb_2[0].rw = 1;
	
	/*
	set_cr0(get_cr0() | (0x1<<31));
	debug("cr0.PG = %d\n", 0x1 & get_cr0());

	debug("contenu de pgd addr = 0x%x\n", pgd[0].addr);
	pde32_t* var = (pde32_t*) 0xc0000000;
	debug("contenu de l'@ 0xc0000000 = 0x%x\n", var->addr);
	*/
	//Fin Q7

	//Q8
	//0x700000 => 1 (pgd)| 768 (ptb)| 0 (offset) 
	ptb_2[768].addr = 0x2000 >> 12;
	ptb_2[768].p = 1;
	ptb_2[768].rw = 1;

	//0x7ff000 => 1 (pgd)| 1023 (ptb)| 0 (offset) 
	ptb_2[1023].addr = 0x2000>>12;
	ptb_2[1023].p = 1;
	ptb_2[1023].rw = 1;

	set_cr0(get_cr0() | (0x1<<31));
	debug("cr0.PG = %d\n", 0x1 & get_cr0());

	char* ch1 = (char*) 0x700000;
	char* ch2 = (char*) 0x7ff000;
	debug("chaine a l'@ 0x700000 = %s\n", ch1);
	debug("chaine a l'@ 0x7ff000 = %s\n", ch2);
	//Fin Q8

	//Q9
	//pgd[0].p = 0;
	//pgd[0].addr = 0x0;
	//pgd[0].rw = 0;
	
}
