#include <stdio.h>   // FILE
#include <stdint.h>  // uint16_t
#include <stdlib.h>
#include <string.h>
#include <signal.h>
/* unix */
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

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
	R_COUNT  // Register counts
};

/* Opcodes */
enum
{
	OP_BR = 0,  /* branch */
	OP_ADD,     /* add */
	OP_LD,      /* load */
	OP_ST,      /* store */
	OP_JSR,     /* jump register */
	OP_AND,     /* and */
	OP_LDR,     /* load register */
	OP_STR,     /* store register */
	OP_RTI,     /* unused */
	OP_NOT,     /* bitwise not */
	OP_LDI,     /* load indirect */
	OP_STI,     /* store indirect */
	OP_JMP,     /* jump */
	OP_RES,     /* reserved (unused) */
	OP_LEA,     /* load effective address */
	OP_TRAP     /* execute trap */
};

/* Condition Flags */
enum{
	FL_POS = 1 << 0,  /* P */
	FL_ZRO = 1 << 1,  /* Z */
	FL_NEG = 1 << 2   /* N */
};

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


/* reg num */
uint16_t reg[R_COUNT];


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

/* According to the result of register r, set conditon code(big NZP). */
void update_flags(uint16_t r) {
	if (reg[r] == 0) {
		reg[R_COND] = FL_ZRO;
	} else if (reg[r] >> 15) {
		reg[R_COND] = FL_POS;
	} else {
		reg[R_COND] = FL_NEG;
	}
}

void read_image_file(FILE* file) {
	/* the origin tells us where in memory to place the image */
	uint16_t origin;
	fread(&origin, sizeof(origin), 1, file);
	origin = swap16(origin);

	/* we know the maximum file size so we only need one fread. */
	uint16_t max_read = MEMORY_MAX - origin;
	uint16_t* p = memory + origin;   // the program origin address.

	/* store them at the location given by p. */
	size_t read = fread(p, sizeof(uint16_t), max_read, file);

	/* swap to little endian */
	while (read-- > 0) {
		*p = swap16(*p);
		++p;
	}
}

int read_image(const char* image_path) {
	/* "rb" mode represents open a binary file. */
	FILE* file = fopen(image_path, "rb");
	/* file does not exist. */
	if (!file) { return 0; }
	/* take path a string. */
	read_image_file(file);
	/* flush the stream pointed by file. */
	fclose(file);
	return 1;
}

uint16_t check_key() {
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	/* monitor STDIN file descriptor and waiting it become ready. */
	return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

/* Memory Access */

void mem_write(uint16_t address, uint16_t val) {
	memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
	if (address == MR_KBSR) {
		if (check_key()) {
			memory[MR_KBSR] = (1 << 15);
			memory[MR_KBDR] = getchar();
		} else {
			memory[MR_KBSR] = 0;
		}
	}
}

/* Platform Specifics (unix) */
struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		/* show usage string */
		printf("lc3 [image-file1] ...\n");
		exit(2);
	}
	for (int j = 1; j < argc; ++j) {
		if (!read_image(argv[j])) {
			printf("failed to load image: %s\n", argv[j]);
			exit(1);
		}
	}

	/* set up */
	signal(SIGINT, handle_interrupt);
	disable_input_buffering();

	/* since exactly one condition flag should be set
	 * at any given time, set the Z flag
	 */
	reg[R_COND] = FL_ZRO;

	/* set the PC to starting position
	 * 0x300 is the defualt
	 */
	enum { PC_START = 0x3000 };
	reg[R_PC] = PC_START;

	int running = 1;
	while (running) {
		/* FETCH */
		uint16_t instr = mem_read(reg[R_PC]++);
		/* LC-3 opcode locate in bit[15:12]. */
		uint16_t op = instr >> 12;

		switch (op) {
			case OP_ADD:
				break;
			case OP_AND:
				break;
			case OP_NOT:
				break;
			case OP_BR:
				break;
			case OP_JMP:
				break;
			case OP_JSR:
				break;
			case OP_LD:
				break;
			case OP_LDI:
				break;
			case OP_LDR:
				break;
			case OP_LEA:
				break;
			case OP_ST:
				break;
			case OP_STI:
				break;
			case OP_STR:
				break;
			case OP_TRAP:
				break;
			case OP_RES:
			case OP_RTI:
			defualt:
				abort();
				break;
		}
	}
	return 0;
}

