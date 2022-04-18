#include <stdio.h>
#include <stdint.h>

/* Memory Mapped Registers */
enum
{
	MR_KBSR = 0xFE00,  /* keyboard status */
	MR_KBDR = 0xFE02   /* keyboard data */
};

/* TRAP Codes */
enum
{
	/* System calls */
	TRAP_GETC = 0x20,  /* get character from keyboard, not echoed into the terminal */
	TRAP_OUT = 0x21,   /* output a character */
	TRAP_PUTS = 0x22,  /* output a word string */
	TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
	TRAP_PUTSP = 0x24, /* output a byte string */
	TRAP_HALT = 0x25   /* halt the program */
};

/* Memory Storage */
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];  /* 65536 locations */

/* Register Storage */
enum
{
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	R_PC,
	R_COND,
	R_COUNT
};

/* Sign Extend */
uint16_t sign_extend(uint16_t x, int bit_count) {
	/* get sign bit of number x */
	if ((x >> (bit_count - 1)) & 1) {
		x |= (0xFFFF << bit_count);
	}
	return x;
}

/* Becuase Lc-3 programs are big-endian, but most modern computers
 * are little-endian. So we need to swap each uint16 that is loaded.
 */
uint16_t swap16(uint16_t x) {
	return (x << 8 | x >> 8);
}

int main() {
	return 0;
}
