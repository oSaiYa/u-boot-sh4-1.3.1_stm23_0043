/*
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.

 * (C) Copyright 2000-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* #define DEBUG */

#include <common.h>

#if defined(CFG_ENV_IS_IN_NAND) /* Environment is in Nand Flash */

#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <nand.h>

#if defined(CONFIG_CMD_ENV) && defined(CONFIG_CMD_NAND)
#define CMD_SAVEENV
#elif defined(CFG_ENV_OFFSET_REDUND)
#error Cannot use CFG_ENV_OFFSET_REDUND without CONFIG_CMD_ENV & CONFIG_CMD_NAND
#endif

#if defined(CFG_ENV_SIZE_REDUND) && (CFG_ENV_SIZE_REDUND != CFG_ENV_SIZE)
#error CFG_ENV_SIZE_REDUND should be the same as CFG_ENV_SIZE
#endif

#ifdef CONFIG_INFERNO
#error CONFIG_INFERNO not supported yet
#endif

int nand_legacy_rw (struct nand_chip* nand, int cmd,
	    size_t start, size_t len,
	    size_t * retlen, u_char * buf);

/* info for NAND chips, defined in drivers/nand/nand.c */
extern nand_info_t nand_info[];

/* references to names in env_common.c */
extern uchar default_environment[];
extern int default_environment_size;

char * env_name_spec = "NAND";


#ifdef ENV_IS_EMBEDDED
extern uchar environment[];
env_t *env_ptr = (env_t *)(&environment[0]);
#else /* ! ENV_IS_EMBEDDED */
env_t *env_ptr = 0;
#endif /* ENV_IS_EMBEDDED */


/* local functions */
#if !defined(ENV_IS_EMBEDDED)
static void use_default(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

uchar env_get_char_spec (int index)
{
	return ( *((uchar *)(gd->env_addr + index)) );
}


/* this is called before nand_init()
 * so we can't read Nand to validate env data.
 * Mark it OK for now. env_relocate() in env_common.c
 * will call our relocate function which will does
 * the real validation.
 *
 * When using a NAND boot image (like sequoia_nand), the environment
 * can be embedded or attached to the U-Boot image in NAND flash. This way
 * the SPL loads not only the U-Boot image from NAND but also the
 * environment.
 */
int env_init(void)
{
#if defined(ENV_IS_EMBEDDED)
	ulong total;
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1, *tmp_env2;

	total = CFG_ENV_SIZE;

	tmp_env1 = env_ptr;
	tmp_env2 = (env_t *)((ulong)env_ptr + CFG_ENV_SIZE);

	crc1_ok = (crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);
	crc2_ok = (crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);

	if (!crc1_ok && !crc2_ok)
		gd->env_valid = 0;
	else if(crc1_ok && !crc2_ok)
		gd->env_valid = 1;
	else if(!crc1_ok && crc2_ok)
		gd->env_valid = 2;
	else {
		/* both ok - check serial */
		if(tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if(tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if(tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if(tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;
	}

	if (gd->env_valid == 1)
		env_ptr = tmp_env1;
	else if (gd->env_valid == 2)
		env_ptr = tmp_env2;
#else /* ENV_IS_EMBEDDED */
	gd->env_addr  = (ulong)&default_environment[0];
	gd->env_valid = 1;
#endif /* ENV_IS_EMBEDDED */

	return (0);
}

#ifdef CMD_SAVEENV
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
#ifdef CFG_ENV_OFFSET_REDUND
int saveenv(void)
{
	ulong total;
	int ret = 0;

	env_ptr->flags++;
	total = CFG_ENV_SIZE;

	if(gd->env_valid == 1) {
		puts ("Erasing redundant Nand...");
		if (nand_erase(&nand_info[0],
			       CFG_ENV_OFFSET_REDUND, CFG_ENV_SIZE))
			return 1;
		puts ("Writing to redundant Nand... ");
		ret = nand_write(&nand_info[0], CFG_ENV_OFFSET_REDUND, &total,
				 (u_char*) env_ptr);
	} else {
		puts ("Erasing Nand...");
		if (nand_erase(&nand_info[0],
			       CFG_ENV_OFFSET, CFG_ENV_SIZE))
			return 1;

		puts ("Writing to Nand... ");
		ret = nand_write(&nand_info[0], CFG_ENV_OFFSET, &total,
				 (u_char*) env_ptr);
	}
	if (ret || total != CFG_ENV_SIZE)
		return 1;

	puts ("done\n");
	gd->env_valid = (gd->env_valid == 2 ? 1 : 2);
	return ret;
}
#else /* ! CFG_ENV_OFFSET_REDUND */
int saveenv(void)
{
	//YWDRIVER_MODI 2010/3/5 d48zm modify
	#if 1
	int ret = 0;
	int blockstart = -1;
	ulong erasesize_blockalign = nand_info[0].erasesize;
	u_char* data_ptr = (u_char*)env_ptr;
	ssize_t offset = CFG_ENV_OFFSET;
	ssize_t envlen = CFG_ENV_SIZE;
	ulong writelen = erasesize_blockalign;
	ulong checklen = erasesize_blockalign;
	ssize_t boundbegin = CFG_ENV_OFFSET + YW_CFG_NAND_ENV_BOUND - erasesize_blockalign;

	while ((envlen > 0) && (offset <= boundbegin)){

		if (envlen < erasesize_blockalign){
			checklen = envlen;	
			writelen = envlen;
		}

		/*
		 * new eraseblock, check for bad block(s). Stay in the
		 * loop to be sure if the offset changes because of
		 * a bad block, that the next block that will be
		 * written to is also checked. Thus avoiding errors if
		 * the block(s) after the skipped block(s) is also bad
		 * (number of blocks depending on the blockalign
		 */
		while (blockstart != (offset & (~erasesize_blockalign+1))) {
			blockstart = offset & (~erasesize_blockalign+1);

			int ret = nand_block_isbad(&nand_info[0], offset);

			if (ret < 0) {
				printf("Bad block check failed\n");
				return 1;
			}
			if (ret == 1) {
				offset = blockstart
					+ erasesize_blockalign;
				printf("\rBad block at 0x%lx "
					   "in erase block from "
					   "0x%x will be skipped\n",
					   (long) offset,
					   blockstart);
			}
		}
		printf ("Erasing Nand block at 0x%lx...", offset);
		if (nand_erase(&nand_info[0], offset, erasesize_blockalign))
			return 1;

		puts ("Writing to Nand block... ");
		ret = nand_write(&nand_info[0], offset, &writelen, data_ptr);
		if (ret || writelen != checklen)
			return 1;
		
		envlen -= writelen;
		data_ptr += writelen;
		offset += writelen;
	}
	#else
	ulong total;
	int ret = 0;

	puts ("Erasing Nand...");
	if (nand_erase(&nand_info[0], CFG_ENV_OFFSET, CFG_ENV_SIZE))
		return 1;

	puts ("Writing to Nand... ");
	total = CFG_ENV_SIZE;
	ret = nand_write(&nand_info[0], CFG_ENV_OFFSET, &total, (u_char*)env_ptr);
	if (ret || total != CFG_ENV_SIZE)
		return 1;
	#endif
	//YWDRIVER_MODI end

	puts ("done\n");
	return ret;
}
#endif /* CFG_ENV_OFFSET_REDUND */
#endif /* CMD_SAVEENV */

#ifdef CFG_ENV_OFFSET_REDUND
void env_relocate_spec (void)
{
#if !defined(ENV_IS_EMBEDDED)
	ulong total;
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1, *tmp_env2;

	total = CFG_ENV_SIZE;

	tmp_env1 = (env_t *) malloc(CFG_ENV_SIZE);
	tmp_env2 = (env_t *) malloc(CFG_ENV_SIZE);

	nand_read(&nand_info[0], CFG_ENV_OFFSET, &total,
		  (u_char*) tmp_env1);
	nand_read(&nand_info[0], CFG_ENV_OFFSET_REDUND, &total,
		  (u_char*) tmp_env2);

	crc1_ok = (crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);
	crc2_ok = (crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);

	if(!crc1_ok && !crc2_ok)
		return use_default();
	else if(crc1_ok && !crc2_ok)
		gd->env_valid = 1;
	else if(!crc1_ok && crc2_ok)
		gd->env_valid = 2;
	else {
		/* both ok - check serial */
		if(tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if(tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if(tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if(tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;

	}

	free(env_ptr);
	if(gd->env_valid == 1) {
		env_ptr = tmp_env1;
		free(tmp_env2);
	} else {
		env_ptr = tmp_env2;
		free(tmp_env1);
	}

#endif /* ! ENV_IS_EMBEDDED */
}
#else /* ! CFG_ENV_OFFSET_REDUND */
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
void env_relocate_spec (void)
{
#if !defined(ENV_IS_EMBEDDED)
	//YWDRIVER_MODI 2010/3/5 d48zm modify
	#if 1
	int ret;
	int blockstart = -1;
	ulong erasesize_blockalign = nand_info[0].erasesize;
	u_char* data_ptr = (u_char*)env_ptr;
	ssize_t offset = CFG_ENV_OFFSET;
	ssize_t envlen = CFG_ENV_SIZE;
	ulong readlen = erasesize_blockalign;
	ulong checklen = erasesize_blockalign;
	ssize_t boundbegin = CFG_ENV_OFFSET + YW_CFG_NAND_ENV_BOUND - erasesize_blockalign;

	while ((envlen > 0) && (offset <= boundbegin)){

		if (envlen < erasesize_blockalign){
			checklen = envlen;	
			readlen = envlen;
		}

		/*
		 * new eraseblock, check for bad block(s). Stay in the
		 * loop to be sure if the offset changes because of
		 * a bad block, that the next block that will be
		 * written to is also checked. Thus avoiding errors if
		 * the block(s) after the skipped block(s) is also bad
		 * (number of blocks depending on the blockalign
		 */
		while (blockstart != (offset & (~erasesize_blockalign+1))) {
			blockstart = offset & (~erasesize_blockalign+1);

			ret = nand_block_isbad(&nand_info[0], offset);

			if (ret < 0) {
				printf("Bad block check failed\n");
				return 1;
			}
			if (ret == 1) {
				offset = blockstart
					+ erasesize_blockalign;
				printf("\rBad block at 0x%lx "
					   "in erase block from "
					   "0x%x will be skipped\n",
					   (long) offset,
					   blockstart);
			}
		}

		ret = nand_read(&nand_info[0], offset, &readlen, data_ptr);
  		if (ret || readlen != checklen)
			return use_default();
		
		envlen -= readlen;
		data_ptr += readlen;
		offset += readlen;
	}
	
	if (crc32(0, env_ptr->data, ENV_SIZE) != env_ptr->crc)
		return use_default();
	#else
	ulong total;
	int ret;

	total = CFG_ENV_SIZE;
	ret = nand_read(&nand_info[0], CFG_ENV_OFFSET, &total, (u_char*)env_ptr);
  	if (ret || total != CFG_ENV_SIZE)
		return use_default();

	if (crc32(0, env_ptr->data, ENV_SIZE) != env_ptr->crc)
		return use_default();
	#endif
	//YWDRIVER_MODI 2010/3/5 d48zm modify end
#endif /* ! ENV_IS_EMBEDDED */
}
#endif /* CFG_ENV_OFFSET_REDUND */

#if !defined(ENV_IS_EMBEDDED)
static void use_default()
{
	puts ("*** Warning - bad CRC or NAND, using default environment\n\n");

	if (default_environment_size > CFG_ENV_SIZE){
		puts ("*** Error - default environment is too large\n\n");
		return;
	}

	memset (env_ptr, 0, sizeof(env_t));
	memcpy (env_ptr->data,
			default_environment,
			default_environment_size);
	env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE);
	gd->env_valid = 1;

}
#endif

#endif /* CFG_ENV_IS_IN_NAND */
