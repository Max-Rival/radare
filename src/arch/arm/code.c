/*
 * Copyright (C) 2007, 2008, 2009
 *       esteve <youterm.com>
 *       pancake <youterm.com>
 *
 * radare is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * radare is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with radare; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* code analysis functions */

#include "../../main.h"
#include "../../radare.h"
#include "../../code.h"
#include "arm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned int disarm_branch_offset ( unsigned int pc, unsigned int insoff )
{
	unsigned int add = insoff << 2;
	/* zero extend if higher is 1 (0x02000000) */
	if ( (add & 0x02000000) == 0x02000000 )
		add = add | 0xFC000000 ;
	return add + pc + 8;
}

static inline int anal_is_B  ( int inst )
{
	if ((inst&ARM_BRANCH_I_MASK) == ARM_BRANCH_I)
		return 1;
	 return 0;
}

static inline int anal_is_BL  ( int inst )
{
	if (anal_is_B(inst) && (inst&ARM_BRANCH_LINK) == ARM_BRANCH_LINK)
		return 1;
	return 0;
}

static inline int anal_is_return ( int inst )
{
	if ((inst&(ARM_DTM_I_MASK|ARM_DTM_LOAD|(1<<15))) == (ARM_DTM_I|ARM_DTM_LOAD|(1<<15)))
		return 1;
	return 0;
}

static inline int anal_is_unkjmp ( int inst )
{
	//if ( (inst & ( ARM_DTX_I_MASK | ARM_DTX_LOAD  | ( ARM_DTX_RD_MASK ) ) ) == ( ARM_DTX_LOAD | ARM_DTX_I | ( ARM_PC << 12 ) ) )
	if (( (( ARM_DTX_RD_MASK ) ) ) == ( ARM_DTX_LOAD | ARM_DTX_I | ( ARM_PC << 12 ) ))
		return 1;
	return 0;
}

static inline int anal_is_load (int inst)
{
	if ((inst & ARM_DTX_LOAD) == (ARM_DTX_LOAD))
		return 1;
	return 0;
}

static inline int anal_is_condAL ( int inst )
{
	if ((inst&ARM_COND_MASK)==ARM_COND_AL)
		return 1;
	return 0;
}

static int anal_is_exitpoint ( int inst )
{
	return ( anal_is_B ( inst ) || anal_is_return ( inst ) || anal_is_unkjmp ( inst ) );
}

extern int arm_mode;

int arch_arm_aop(ut64 addr, const u8 *codeA, struct aop_t *aop)
{
	unsigned int i = 0;
	unsigned int* code=(unsigned int *)codeA;
	unsigned int branch_dst_addr;
	ut8 *b = (ut8*)&code[i];

	if (aop == NULL)
		return (arm_mode==16)?2:4;

	memset(aop, '\0', sizeof(struct aop_t));
	aop->type = AOP_TYPE_UNK;
#if 0
	fprintf(stderr, "CODE %02x %02x %02x %02x\n",
		codeA[0], codeA[1], codeA[2], codeA[3]);
#endif

//eprintf("0x%08x\n", code[i] & ARM_DTX_LOAD);
	// 0x0001B4D8,           1eff2fe1        bx    lr
	if (b[3]==0xe2 && b[2]==0x8d && b[1]==0xd0) {
		// ADD SP, SP, ...
		aop->type = AOP_TYPE_ADD;
		aop->stackop = AOP_STACK_INCSTACK;
		aop->value = -b[0];
	}
	if (b[3]==0xe2 && b[2]==0x4d && b[1]==0xd0) {
		// SUB SP, SP, ..
		aop->type = AOP_TYPE_SUB;
		aop->stackop = AOP_STACK_INCSTACK;
		aop->value = b[0];
	} else
	if (b[3]==0xe2 && b[2]==0x4c && b[1]==0xb0) {
		// SUB SP, FP, ..
		aop->type = AOP_TYPE_SUB;
		aop->stackop = AOP_STACK_INCSTACK;
		aop->value = -b[0];
	} else
	if (b[3]==0xe2 && b[2]==0x4b && b[1]==0xd0) {
		// SUB SP, IP, ..
		aop->type = AOP_TYPE_SUB;
		aop->stackop = AOP_STACK_INCSTACK;
		aop->value = -b[0];
	} else
	if( (code[i] == 0x1eff2fe1) ||(code[i] == 0xe12fff1e)) { // bx lr
		aop->type = AOP_TYPE_RET;
		aop->eob = 1;
	} else
	if ((code[i] & ARM_DTX_LOAD)) { //anal_is_load(code[i])) {
		int ret;
		ut32 ptr = 0;
		aop->type = AOP_TYPE_MOV;
		if (b[2]==0x1b) {
			/* XXX pretty incomplete */
			aop->stackop = AOP_STACK_LOCAL_GET;
			aop->ref = b[0];
			//var_add_access(addr, -b[0], 1, 0); // TODO: set/get (the last 0)
		} else {
			ut32 oaddr = addr+8+b[0];
			ret = radare_read_at(oaddr, (ut8*)&ptr, 4);
			if (ret == 4) {
				b = (ut8*)&ptr;
				aop->ref = b[0] + (b[1]<<8) + (b[2]<<16) + (b[3]<<24);
				data_xrefs_add(oaddr, aop->ref, 1);
				//TODO change data type to pointer
			} else {
				aop->ref = 0;
			}
		}
	} else
	if (b[3]==0xe5) {
		/* STORE */
		aop->type = AOP_TYPE_STORE;
		aop->stackop = AOP_STACK_LOCAL_SET;
		aop->ref = b[0];
	}

	if (anal_is_exitpoint(code[i])) {
		branch_dst_addr = disarm_branch_offset(addr, code[i]&0x00FFFFFF );
aop->ref = 0;
		if (anal_is_BL(code[i])) {
			if ( anal_is_B ( code[i] ) ) {
				aop->type = AOP_TYPE_CALL;
				aop->jump = branch_dst_addr;
				aop->fail = addr + 4 ;
				aop->eob  = 1;
			} else {
				aop->type = AOP_TYPE_RET;
				aop->eob = 1;
			}
		} else if ( anal_is_B ( code[i] ) ) {
			if ( anal_is_condAL (code[i] )  ) {
				aop->type = AOP_TYPE_JMP;
				aop->jump = branch_dst_addr;
				aop->eob = 1;
			} else {
				aop->type = AOP_TYPE_CJMP;
				aop->jump = branch_dst_addr;
				aop->fail = addr + 4;
				aop->eob  = 1;
			}
		} else {
			//unknown jump o return
			aop->type = AOP_TYPE_UJMP;
			aop->eob = 1;
		}
	}

	return (arm_mode==16)?2:4;
}


// NOTE: bytes should be at least 16 bytes!
#if 0
int arch_arm_aop(unsigned long addr, const unsigned char *codeA, struct aop_t *aop)
{
	unsigned int i=0;
	unsigned int* code=codeA;

	unsigned int branch_dst_addr;
	memset(aop, '\0', sizeof(struct aop_t));

	// unknown jump
	if ( (code[i] & ( ARM_DTX_I_MASK | ARM_DTX_LOAD
	| ( ARM_DTX_RD_MASK ) ) ) == ( ARM_DTX_LOAD | ARM_DTX_I
	| ( ARM_PC << 12 ) ) ) {
		if ( (code[i] & ARM_COND_MASK ) == ARM_COND_AL ) {
			//aop->type = AOP_TYPE_UJMP;
			aop->eob = 1;
			return 4;
		}
	}

	// ret
	if ( (code[i]& ( ARM_DTM_I_MASK | ARM_DTM_LOAD 
	|  ( 1 << 15 ) ) ) == ( ARM_DTM_I | ARM_DTM_LOAD
	|  ( 1 << 15 ) )  ) {
		if ( (code[i] & ARM_COND_MASK ) == ARM_COND_AL ) {
			aop->type = AOP_TYPE_RET;
			aop->eob  = 1;
			return 4;
		}
	}

	if ( (code[i]& ARM_BRANCH_I  ) == ARM_BRANCH_I  ) {
		// CALL
		if ( (code[i]&ARM_BRANCH_LINK ) == ARM_BRANCH_LINK ) {	// BL {
			branch_dst_addr =  disarm_branch_offset ( addr*4, code[i]&0x00FFFFFF ) ;
			if ( branch_dst_addr !=  (addr*4 ) ) {
				aop->type = AOP_TYPE_CALL;
				aop->jump = branch_dst_addr;
				aop->fail = addr + 4;
				aop->eob  = 1;
			}
		} else {
			// B
			if ( (code[i]&ARM_COND_MASK ) == ARM_COND_AL ) {
				branch_dst_addr =  disarm_branch_offset ( addr*4, code[i]&0x00FFFFFF ) ;
				if ( branch_dst_addr !=  (addr*4 ) ) {
					aop->type   = AOP_TYPE_JMP;
					aop->jump = branch_dst_addr;
					aop->eob = 1;
				}
			} else {
				branch_dst_addr =  disarm_branch_offset ( addr*4, code[i]&0x00FFFFFF ) ;
				if ( branch_dst_addr !=  (addr*4 ) ) {
					aop->type = AOP_TYPE_CJMP;
					aop->jump = branch_dst_addr;
					aop->fail = addr + 4;
					aop->eob = 1;
				}
			}
		}
	}

	return 4;
}

#endif
