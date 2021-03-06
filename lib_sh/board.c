/*
 * (C) Copyright 2004 STMicroelectronics.
 *
 * Andy Sturges <andy.sturges@st.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA $Revision: 1.22 $
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <version.h>
#include <devices.h>
#include <version.h>
#include <net.h>
#include <environment.h>
#if defined(CONFIG_CMD_NAND)
#include <nand.h>
#endif
#if defined(CONFIG_SH_STB7100)
#include <asm/stb7100reg.h>
#elif defined(CONFIG_SH_STX7105)
#include <asm/stx7105reg.h>
#elif defined(CONFIG_SH_STX7111)
#include <asm/stx7111reg.h>
#elif defined(CONFIG_SH_STX7141)
#include <asm/stx7141reg.h>
#elif defined(CONFIG_SH_STX7200)
#include <asm/stx7200reg.h>
#else
#error Missing Device Definitions!
#endif
#include <asm/st40reg.h>

extern ulong _uboot_end_data;
extern ulong _uboot_end;

ulong monitor_flash_len;

#ifndef CONFIG_IDENT_STRING
#define CONFIG_IDENT_STRING ""
#endif

#ifndef CONFIG_BOARD_VERSION_STRING
#define CONFIG_BOARD_VERSION_STRING ""
#endif

#ifdef CONFIG_YW_BURNER
#define CONFIG_YW_BUNER_STRING " burner"
#else
#define CONFIG_YW_BUNER_STRING ""
#endif

const char version_string[] =
	U_BOOT_VERSION" (" __DATE__ " - " __TIME__ ") - " CONFIG_IDENT_STRING
	" - " CONFIG_BOARD_VERSION_STRING CONFIG_YW_BUNER_STRING;

/*
 * Begin and End of memory area for malloc(), and current "brk"
 */

#define	TOTAL_MALLOC_LEN	CFG_MALLOC_LEN

static ulong mem_malloc_start;
static ulong mem_malloc_end;
static ulong mem_malloc_brk;

extern int soc_init (void); 	/* Detect/set SOC settings  */
extern int board_init (void);   /* Set up board             */
extern int timer_init (void);
extern int checkboard (void);   /* Give info about board    */
//YWDRIVER_MODI  add begin
//Description:add for vfd
#ifdef YW_CONFIG_VFD
extern void YWVFD_Init(void);
extern int YWVFD_LBD_PowerOff(void);
#endif
void IDENT_Init(void);
//YWDRIVER_MODI add end
static void mem_malloc_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;

	ulong dest_addr = TEXT_BASE + gd->reloc_off;

	mem_malloc_end = dest_addr;
	mem_malloc_start = dest_addr - TOTAL_MALLOC_LEN;
	mem_malloc_brk = mem_malloc_start;

	memset ((void *) mem_malloc_start,
		0, mem_malloc_end - mem_malloc_start);
}

void *sbrk (ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	if ((new < mem_malloc_start) || (new > mem_malloc_end)) {
		return (NULL);
	}
	mem_malloc_brk = new;
	return ((void *) old);
}

static int init_func_ram (void)
{
	DECLARE_GLOBAL_DATA_PTR;

#ifdef	CONFIG_BOARD_TYPES
	int board_type = gd->board_type;
#endif

	gd->ram_size = CFG_SDRAM_SIZE;
	puts ("DRAM:  ");
	print_size (gd->ram_size, "\n");

	return (0);
}

static int display_banner (void)
{

	printf ("\n\n%s\n\n", version_string);
	return (0);
}

#ifndef CFG_NO_FLASH
static void display_flash_config (ulong size)
{
	puts ("NOR:   ");
	print_size (size, "\n");
}
#endif /* CFG_NO_FLASH */


static int init_baudrate (void)
{
	DECLARE_GLOBAL_DATA_PTR;

	char tmp[64];		/* long enough for environment variables */
	int i = getenv_r ("baudrate", tmp, sizeof (tmp));

	gd->baudrate = (i > 0)
		? (int) simple_strtoul (tmp, NULL, 10)
		: CONFIG_BAUDRATE;

	return (0);
}

void flashWriteEnable(void);

/*
 * All attempts to come up with a "common" initialization sequence
 * that works for all boards and architectures failed: some of the
 * requirements are just _too_ different. To get rid of the resulting
 * mess of board dependend #ifdef'ed code we now make the whole
 * initialization sequence configurable to the user.
 *
 * The requirements for any new initalization function is simple: it
 * receives a pointer to the "global data" structure as it's only
 * argument, and returns an integer return code, where 0 means
 * "continue" and != 0 means "fatal error, hang the system".
 */
typedef int (init_fnc_t) (void);

init_fnc_t *init_sequence[] = {
	soc_init,
	timer_init,
	board_init,
	env_init,		/* initialize environment */
	init_baudrate,		/* initialze baudrate settings */
	serial_init,		/* serial communications setup */
	console_init_f,		/* Initial console             */
	checkboard,
	display_banner,		/* say that we are here */
	init_func_ram,
	NULL,
};


void start_sh4boot (void)
{
	DECLARE_GLOBAL_DATA_PTR;

	bd_t *bd;
	ulong addr;
	init_fnc_t **init_fnc_ptr;
#ifndef CFG_NO_FLASH
	ulong size;
#endif /* CFG_NO_FLASH */

	char *s, *e;
	int i;

	addr = TEXT_BASE;
	/* Reserve memory for malloc() arena. */
	addr -= TOTAL_MALLOC_LEN;
	/* (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */
	addr -= sizeof (gd_t);
	gd = (gd_t *) addr;
	memset ((void *) gd, 0, sizeof (gd_t));
	addr -= sizeof (bd_t);
	bd = (bd_t *) addr;
	gd->bd = bd;

	/* Reserve memory for boot params.
	 */

	addr -= CFG_BOOTPARAMS_LEN;
	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr) () != 0) {
			hang ();
		}
	}

	gd->reloc_off = 0;
	gd->flags |= GD_FLG_RELOC;	/* tell others: relocation done */

	monitor_flash_len = (ulong) & _uboot_end_data - TEXT_BASE;

	/* configure available FLASH banks */
	flashWriteEnable();
#ifndef CFG_NO_FLASH
	size = flash_init ();
	display_flash_config (size);
#endif /* CFG_NO_FLASH */

	bd = gd->bd;
	bd->bi_boot_params = addr;
	bd->bi_memstart = CFG_SDRAM_BASE;	/* start of  DRAM memory */
	bd->bi_memsize = gd->ram_size;	/* size  of  DRAM memory in bytes */
	bd->bi_baudrate = gd->baudrate;	/* Console Baudrate */
	bd->bi_flashstart = CFG_FLASH_BASE;
#ifndef CFG_NO_FLASH
	bd->bi_flashsize = size;
#endif /* CFG_NO_FLASH */
#if CFG_MONITOR_BASE == CFG_FLASH_BASE
	bd->bi_flashoffset = monitor_flash_len;	/* reserved area for U-Boot */
#else
	bd->bi_flashoffset = 0;
#endif

	/* initialize malloc() area */
	mem_malloc_init ();

#if defined(CONFIG_CMD_NAND)
	puts ("NAND:  ");
	nand_init();		/* go init the NAND */
#endif

	/* Allocate environment function pointers etc. */
	env_relocate ();

	/* board MAC address */
	s = getenv ("ethaddr");
	for (i = 0; i < 6; ++i) {
		bd->bi_enetaddr[i] = (s ? simple_strtoul (s, &e, 16) : 0)
			&& 0xff;
		if (s)
			s = (*e) ? e + 1 : e;
	}

	/* IP Address */
	bd->bi_ip_addr = getenv_IPaddr ("ipaddr");

#if defined(CONFIG_PCI)
	/*
	 * Do pci configuration
	 */
	pci_init ();
#endif

/** leave this here (after malloc(), environment and PCI are working) **/
	/* Initialize devices */
	devices_init ();

	jumptable_init ();

	/* Initialize the console (after the relocation and devices init) */
	console_init_r ();

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

	/* Initialize from environment */
	if ((s = getenv ("loadaddr")) != NULL) {
		load_addr = simple_strtoul (s, NULL, 16);
	}
#if defined(CONFIG_CMD_NET)
	if ((s = getenv ("bootfile")) != NULL) {
		copy_filename (BootFile, s, sizeof (BootFile));
	}
#endif /* CONFIG_CMD_NET */

#if defined(CONFIG_MISC_INIT_R)
	/* miscellaneous platform dependent initialisations */
	misc_init_r ();
#endif

#if defined(CONFIG_CMD_NET)
#if defined(CONFIG_NET_MULTI)
	puts ("Net:   ");
#endif
	eth_initialize(gd->bd);
#endif
//YWDRIVER_MODI D02SH 2009/06/22 add begin

#ifdef YW_CONFIG_VFD
    YWVFD_Init();
	YWVFD_LBD_PowerOff();
#endif
//#ifdef	CONFIG_IDENT
	IDENT_Init();
//#endif

//YWDRIVER_MODI D02SH 2009/06/22 add end
	/* main_loop() can return to retry autoboot, if so just run it again. */
	for (;;) {
		main_loop ();
	}

	/* NOTREACHED - no way out of command loop except booting */
}


void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}


static void sh_reset (void) __attribute__ ((noreturn));
static void sh_reset (void)
{
#if 1
	/*
	 * We will use the on-chip watchdog timer to force a
	 * power-on-reset of the device.
	 * A power-on-reset is required to guarantee all SH4-200 cores
	 * will reset back into 29-bit mode, if they were in SE mode.
	 * However, on SH4-300 series parts, issuing a TRAP instruction
	 * with SR.BL=1 is sufficient. However, we will use a "one size fits
	 * all" solution here, and use the watchdog for all SH parts.
	 */

		/* WTCNT          = FF	counter to overflow next tick */
	*ST40_CPG_WTCNT = 0x5AFF;

		/* WTCSR2.CKS[3]  = 0	use legacy clock dividers */
	*ST40_CPG_WTCSR2 = 0xAA00;

		/* WTCSR.TME      = 1	enable up-count counter */
		/* WTCSR.WT       = 1	watchdog timer mode */
		/* WTCSR.RSTS     = 0	enable power-on reset */
		/* WTCSR.CKS[2:0] = 2	clock division ratio 1:128 */
		/* NOTE: we need CKS to be big enough to allow
		 * U-boot to disable the watchdog, AFTER the reset,
		 * otherwise, we enter an infinite-loop of resetting! */
	*ST40_CPG_WTCSR = 0xA5C2;

	/* wait for H/W reset to kick in ... */
	for (;;);
#else
	ulong sr;
	asm ("stc sr, %0":"=r" (sr));
	sr |= (1 << 28);	/* set block bit */
	asm ("ldc %0, sr": :"r" (sr));
	asm volatile ("trapa #0");
#endif
}


extern int do_reset (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	sh_reset();
	/*NOTREACHED*/ return (0);
}
