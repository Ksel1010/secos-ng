/* GPLv2 (c) Airbus */
OUTPUT_FORMAT("elf32-i386","elf32-i386","elf32-i386");
OUTPUT_ARCH("i386")

ENTRY(entry)

PHDRS
{
   phboot  PT_LOAD FLAGS (7);
   phstack PT_LOAD FLAGS (6);
   phsetup PT_LOAD FLAGS (7);
}

SECTIONS
{
   . = 0x300000;
   /* faut que la mem commence par .mbh afin d'eviter que le magic numbeer se retrouve ici et que ca ne corresponde pas a celui de mbh (meme nombre par hasard )*/
   .mbh      : { KEEP(*(.mbh)) . = ALIGN(4);     } : phboot /* fait une taille de 12 (3 element de 32bits dans le tableau de en start.c) */
                                                            /* align sur 16 (entry.s) */
                                                            /* 0x300000 + 0x10 */
   /* afin de proteger la stack faudra mettre __kernel__start avant la pile */
   .stack    : { KEEP(*(.stack))                 } : phstack
                                                            /* 0x302010 */
   __kernel_start__ = .;

   .idt_jmp  : { KEEP(*(.idt_jmp))               } : phsetup
                                                               /* fait 0x1000 => en realite entry commence a 0x303010 */
   .text     : { *(.text)                        } : phsetup
   .rodata   : { *(.rodata)                      } : phsetup
   .data     : { *(.data)                        } : phsetup
   .bss      : { *(.bss COMMON)                  } : phsetup
   /DISCARD/ : { *(.note* .indent .comment)      } : phsetup

   __kernel_end__ = .;
      . = 0x810000;  
    .user1 : { *(.user1) }

    . = 0x820000;   
    .user2 : { *(.user2) }
    
}
