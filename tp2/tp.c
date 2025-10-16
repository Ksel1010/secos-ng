/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>

void bp_handler() {
   // Q7
   //uint32_t val;
   //asm volatile ("mov 4(%%ebp), %0":"=r"(val));
   //debug("catch bp exception val = %x\n", val);
   asm volatile ("pusha");
   debug("catch bp exception\n");
   asm volatile ("popa");
   asm volatile ("leave; iret");
}

void bp_trigger() {
	asm volatile(
		"int3"
	);
	debug("trigger\n");
}

void tp() {
	// TODO print idtr
	idt_reg_t idt;
	get_idtr(idt);
	debug("idt addr = 0x%ld\nlimit idt = %d\n", idt.addr, idt.limit);
	int_desc_t* desc = &idt.desc[3];
	desc->offset_1 = ((unsigned int) (bp_handler)) & 0x0000FFFF;
	desc->offset_2 = (((unsigned int) (bp_handler))&0xFFFF0000 ) >>16;
	set_idtr(idt);
	// TODO call bp_trigger
	bp_trigger();
	debug("end\n");
}
