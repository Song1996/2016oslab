OUTPUT_FORMAT(
	"elf32-i386",
	"elf32_i386",
	"elf32-i386"
)
OUTPUT_ARCH(i386)
ENTRY(main_loop)

SECTIONS
{
	. = 0x8048000;
	PROVIDE(start = .);
	.text : AT(0x100000){
		*(.text .text.*)
	}
	PROVIDE(etext = .);
	.rodata : {
		*(.rodata .rodata.*)
	}
	.=ALIGN(0x1000);
	.data :{
		*(.data)
	}
	.bss :{
		*(.bss)
	}
	PROVIDE(end = .);
}
