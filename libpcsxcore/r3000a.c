/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

/*
* R3000A CPU functions.
*/

#include "r3000a.h"
#include "cdrom.h"
#include "mdec.h"
#include "gpu.h"
#include "gte.h"

R3000Acpu *psxCpu = NULL;
psxRegisters psxRegs;

int psxInit() {
	SysPrintf(_("Running PCSX Version %s (%s).\n"), PACKAGE_VERSION, __DATE__);

#ifdef PSXREC
	if (Config.Cpu == CPU_INTERPRETER) {
		psxCpu = &psxInt;
	} else psxCpu = &psxRec;
#else
	psxCpu = &psxInt;
#endif

	Log = 0;

	if (psxMemInit() == -1) return -1;

	return psxCpu->Init();
}

void psxReset() {
	psxCpu->Reset();

	psxMemReset();

	memset(&psxRegs, 0, sizeof(psxRegs));

	psxRegs.pc = 0xbfc00000; // Start in bootstrap

	psxRegs.CP0.r[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.r[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

	psxHwReset();
	psxBiosInit();

	if (!Config.HLE)
		psxExecuteBios();

#ifdef EMU_LOG
	EMU_LOG("*BIOS END*\n");
#endif
	Log = 0;
}

void psxShutdown() {
	psxMemShutdown();
	psxBiosShutdown();

	psxCpu->Shutdown();
}

void psxException(u32 code, u32 bd) {
	// Set the Cause
	psxRegs.CP0.n.Cause = code;

	// Set the EPC & PC
	if (bd) {
#ifdef PSXCPU_LOG
		PSXCPU_LOG("bd set!!!\n");
#endif
		SysPrintf("bd set!!!\n");
		psxRegs.CP0.n.Cause |= 0x80000000;
		psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
	} else
		psxRegs.CP0.n.EPC = (psxRegs.pc);

	if (psxRegs.CP0.n.Status & 0x400000)
		psxRegs.pc = 0xbfc00180;
	else
		psxRegs.pc = 0x80000080;

	// Set the Status
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
						  ((psxRegs.CP0.n.Status & 0xf) << 2);

	if (Config.HLE) psxBiosException();
}

static void nothing() {
}

#define ITR_HANDLER(func, psxint)			\
static void _##psxint##_func() {			\
	psxRegs.interrupt &= ~(1 << psxint);	\
	func();									\
}											\
											\
void (*_##psxint##Handler[])() = {			\
	nothing,								\
	_##psxint##_func						\
};											\

ITR_HANDLER(sioInterrupt, PSXINT_SIO);
ITR_HANDLER(cdrInterrupt, PSXINT_CDR);
ITR_HANDLER(cdrReadInterrupt, PSXINT_CDREAD);
ITR_HANDLER(gpuInterrupt, PSXINT_GPUDMA);
ITR_HANDLER(mdec1Interrupt, PSXINT_MDECOUTDMA);
ITR_HANDLER(spuInterrupt, PSXINT_SPUDMA);
ITR_HANDLER(GPU_idle, PSXINT_GPUBUSY);
ITR_HANDLER(mdec0Interrupt, PSXINT_MDECINDMA);
ITR_HANDLER(gpuotcInterrupt, PSXINT_GPUOTCDMA);
ITR_HANDLER(cdrDmaInterrupt, PSXINT_CDRDMA);
ITR_HANDLER(cdrPlayInterrupt, PSXINT_CDRPLAY);
ITR_HANDLER(cdrDecodedBufferInterrupt, PSXINT_CDRDBUF);
ITR_HANDLER(cdrLidSeekInterrupt, PSXINT_CDRLID);

void psxBranchTest() {
	// GameShark Sampler: Give VSync pin some delay before exception eats it
	if (psxHu32(0x1070) & psxHu32(0x1074)) {
		if ((psxRegs.CP0.n.Status & 0x401) == 0x401) {
			
#if defined(__BIGENDIAN__)
			// Crash Bandicoot 2: Don't run exceptions when GTE in pipeline
			u8 opcode = *PSXM(psxRegs.pc);
			
			if( ((opcode) & 0xfe) != 0x4a ) {
#else
			u32 opcode;
			u32 * code = (u32 *)PSXM(psxRegs.pc);
			//u32 *code = Read_ICache(psxRegs.pc, TRUE);;
			// Crash Bandicoot 2: Don't run exceptions when GTE in pipeline
			opcode = SWAP32(*code);

			if( ((opcode >> 24) & 0xfe) != 0x4a ) {
#endif
#ifdef PSXCPU_LOG
				PSXCPU_LOG("Interrupt: %x %x\n", psxHu32(0x1070), psxHu32(0x1074));
#endif
				psxException(0x400, 0);
			}
		}
	}

	if ((psxRegs.cycle - psxNextsCounter) >= psxNextCounter)
		psxRcntUpdate();
#if 1
	if (psxRegs.interrupt) {
		if ((psxRegs.interrupt & (1 << PSXINT_SIO)) && !Config.Sio) { // sio
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_SIO].sCycle) >= psxRegs.intCycle[PSXINT_SIO].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_SIO);
				sioInterrupt();
			}
		}
		if (psxRegs.interrupt & (1 << PSXINT_CDR)) { // cdr
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDR].sCycle) >= psxRegs.intCycle[PSXINT_CDR].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDR);
				cdrInterrupt();
			}
		}
		if (psxRegs.interrupt & (1 << PSXINT_CDREAD)) { // cdr read
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDREAD].sCycle) >= psxRegs.intCycle[PSXINT_CDREAD].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDREAD);
				cdrReadInterrupt();
			}
		}
		if (psxRegs.interrupt & (1 << PSXINT_GPUDMA)) { // gpu dma
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_GPUDMA].sCycle) >= psxRegs.intCycle[PSXINT_GPUDMA].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_GPUDMA);
				gpuInterrupt();
			}
		}
		if (psxRegs.interrupt & (1 << PSXINT_MDECOUTDMA)) { // mdec out dma
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_MDECOUTDMA].sCycle) >= psxRegs.intCycle[PSXINT_MDECOUTDMA].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_MDECOUTDMA);
				mdec1Interrupt();
			}
		}
		if (psxRegs.interrupt & (1 << PSXINT_SPUDMA)) { // spu dma
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_SPUDMA].sCycle) >= psxRegs.intCycle[PSXINT_SPUDMA].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_SPUDMA);
				spuInterrupt();
			}
		}
		if (psxRegs.interrupt & (1 << PSXINT_GPUBUSY)) { // gpu busy
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_GPUBUSY].sCycle) >= psxRegs.intCycle[PSXINT_GPUBUSY].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_GPUBUSY);
				GPU_idle();
			}
		}

		if (psxRegs.interrupt & (1 << PSXINT_MDECINDMA)) { // mdec in
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_MDECINDMA].sCycle) >= psxRegs.intCycle[PSXINT_MDECINDMA].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_MDECINDMA);
				mdec0Interrupt();
			}
		}

		if (psxRegs.interrupt & (1 << PSXINT_GPUOTCDMA)) { // gpu otc
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_GPUOTCDMA].sCycle) >= psxRegs.intCycle[PSXINT_GPUOTCDMA].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_GPUOTCDMA);
				gpuotcInterrupt();
			}
		}

		if (psxRegs.interrupt & (1 << PSXINT_CDRDMA)) { // cdrom
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRDMA].sCycle) >= psxRegs.intCycle[PSXINT_CDRDMA].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDRDMA);
				cdrDmaInterrupt();
			}
		}

		if (psxRegs.interrupt & (1 << PSXINT_CDRPLAY)) { // cdr play timing
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRPLAY].sCycle) >= psxRegs.intCycle[PSXINT_CDRPLAY].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDRPLAY);
				cdrPlayInterrupt();
			}
		}

		if (psxRegs.interrupt & (1 << PSXINT_CDRDBUF)) { // cdr decoded buffer
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRDBUF].sCycle) >= psxRegs.intCycle[PSXINT_CDRDBUF].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDRDBUF);
				cdrDecodedBufferInterrupt();
			}
		}

		if (psxRegs.interrupt & (1 << PSXINT_CDRLID)) { // cdr lid states
			if ((psxRegs.cycle - psxRegs.intCycle[PSXINT_CDRLID].sCycle) >= psxRegs.intCycle[PSXINT_CDRLID].cycle) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDRLID);
				cdrLidSeekInterrupt();
			}
		}
	}
#else
	// Because each fps count ...
#define BF_INT(psxint) \
	((psxRegs.interrupt & (1 << psxint))) && (((psxRegs.intCycle[psxint].cycle - ((psxRegs.cycle - psxRegs.intCycle[psxint].sCycle) + 1))) >> 31)

	// Slow =>
	// ((psxRegs.interrupt & (1 << psxint)) >> psxint) & (((psxRegs.intCycle[psxint].cycle - ((psxRegs.cycle - psxRegs.intCycle[psxint].sCycle) + 1))   & 0xFFFFFFFF) >> 31)
	// Faster but not still not good
	// ((psxRegs.interrupt & (1 << psxint)) >> psxint) && (((psxRegs.intCycle[psxint].cycle - ((psxRegs.cycle - psxRegs.intCycle[psxint].sCycle) + 1))  & 0xFFFFFFFF) >> 31)

#if 1
	if (psxRegs.interrupt) {

		if (BF_INT(PSXINT_CDR)) {
			psxRegs.interrupt &= ~(1 << PSXINT_CDR);
			cdrInterrupt();
		}

		if (BF_INT(PSXINT_SIO)) {
			psxRegs.interrupt &= ~(1 << PSXINT_SIO);
			sioInterrupt();
		}

		if (BF_INT(PSXINT_CDREAD)) {
			psxRegs.interrupt &= ~(1 << PSXINT_CDREAD);
			cdrReadInterrupt();
		}

		if (BF_INT(PSXINT_GPUDMA)) {
			psxRegs.interrupt &= ~(1 << PSXINT_GPUDMA);
			gpuInterrupt();
		}

		if (BF_INT(PSXINT_MDECOUTDMA)) {
			psxRegs.interrupt &= ~(1 << PSXINT_MDECOUTDMA);
			mdec1Interrupt();
		}

		if (BF_INT(PSXINT_SPUDMA)) {
			psxRegs.interrupt &= ~(1 << PSXINT_SPUDMA);
			spuInterrupt();
		}

		if (BF_INT(PSXINT_GPUBUSY)) {
			psxRegs.interrupt &= ~(1 << PSXINT_GPUBUSY);
			GPU_idle();
		}

		if (BF_INT(PSXINT_MDECINDMA)) {
			psxRegs.interrupt &= ~(1 << PSXINT_MDECINDMA);
			mdec0Interrupt();
		}

		if (BF_INT(PSXINT_GPUOTCDMA)) {
			psxRegs.interrupt &= ~(1 << PSXINT_GPUOTCDMA);
			gpuotcInterrupt();
		}

		if (BF_INT(PSXINT_CDRDMA)) {
			psxRegs.interrupt &= ~(1 << PSXINT_CDRDMA);
			cdrDmaInterrupt();
		}

		if (BF_INT(PSXINT_CDRPLAY)) {
			psxRegs.interrupt &= ~(1 << PSXINT_CDRPLAY);
			cdrPlayInterrupt();
		}

		if (BF_INT(PSXINT_CDRDBUF)) {
			psxRegs.interrupt &= ~(1 << PSXINT_CDRDBUF);
			cdrDecodedBufferInterrupt();
		}

		if (BF_INT(PSXINT_CDRLID)) {
			psxRegs.interrupt &= ~(1 << PSXINT_CDRLID);
			cdrLidSeekInterrupt();
		}
	}
#else
	if (psxRegs.interrupt) {
#define BF_DO_INT(psxint) \
	_##psxint##Handler[psxint]();

		BF_DO_INT(PSXINT_SIO);
		BF_DO_INT(PSXINT_CDR);
		BF_DO_INT(PSXINT_CDREAD);
		BF_DO_INT(PSXINT_GPUDMA);
		BF_DO_INT(PSXINT_MDECOUTDMA);
		BF_DO_INT(PSXINT_SPUDMA);
		BF_DO_INT(PSXINT_GPUBUSY);
		BF_DO_INT(PSXINT_MDECINDMA);
		BF_DO_INT(PSXINT_GPUOTCDMA);		
		BF_DO_INT(PSXINT_CDRDMA);
		BF_DO_INT(PSXINT_CDRDBUF);
		BF_DO_INT(PSXINT_CDRLID);		
		BF_DO_INT(PSXINT_CDRPLAY);
	}

#endif

#endif
}

void psxJumpTest() {
	if (!Config.HLE && Config.PsxOut) {
		u32 call = psxRegs.GPR.n.t1 & 0xff;
		switch (psxRegs.pc & 0x1fffff) {
			case 0xa0:
#ifdef PSXBIOS_LOG
				if (call != 0x28 && call != 0xe) {
					PSXBIOS_LOG("Bios call a0: %s (%x) %x,%x,%x,%x\n", biosA0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosA0[call])
					biosA0[call]();
				break;
			case 0xb0:
#ifdef PSXBIOS_LOG
				if (call != 0x17 && call != 0xb) {
					PSXBIOS_LOG("Bios call b0: %s (%x) %x,%x,%x,%x\n", biosB0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosB0[call])
					biosB0[call]();
				break;
			case 0xc0:
#ifdef PSXBIOS_LOG
				PSXBIOS_LOG("Bios call c0: %s (%x) %x,%x,%x,%x\n", biosC0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3);
#endif
				if (biosC0[call])
					biosC0[call]();
				break;
		}
	}
}

void psxExecuteBios() {
	while (psxRegs.pc != 0x80030000)
		psxCpu->ExecuteBlock();
}

