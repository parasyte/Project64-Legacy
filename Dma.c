/*
 * Project 64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 zilmar (zilmar@emulation64.com) and 
 * Jabo (jabo@emulation64.com).
 *
 * pj64 homepage: www.pj64.net
 *
 * Permission to use, copy, modify and distribute Project64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Project64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Project64 or software derived from Project64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */
#include <Windows.h>
#include <stdio.h>
#include "main.h"
#include "debugger.h"
#include "CPU.h"

int DMAUsed;

void FirstDMA (void) {
	switch (GetCicChipID(ROM)) {
	case CIC_NUS_6101: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_6102: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_6103: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_6105: *(DWORD *)&N64MEM[0x3F0] = RdramSize; break;
	case CIC_NUS_6106: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_5167: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_8303: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_8401: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_DDUS: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_XENO: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	default: 
		*(DWORD *)&N64MEM[0x318] = RdramSize;
		if (ShowDebugMessages)
			DisplayError("Unhandled CicChip(%d) in first DMA",GetCicChipID(ROM));
	}
}

void PI_DMA_READ (void) {
//	PI_STATUS_REG |= PI_STATUS_DMA_BUSY;
	PI_DRAM_ADDR_REG &= 0x1FFFFFFF;

	PI_RD_LEN_REG = (PI_RD_LEN_REG & 1) ? PI_RD_LEN_REG : PI_RD_LEN_REG + 1;	// Fix for Ai Shogi 3

	PI_CART_ADDR_REG &= ~1;	// Taz Express fix
	PI_DRAM_ADDR_REG &= ~7;	// Tax Express fix

	if ( PI_DRAM_ADDR_REG + PI_RD_LEN_REG + 1 > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("PI_DMA_READ not in Memory");
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		return;
	}

	// Trying to fix saves for Dezaemon 3D (J)
	// There is likely a better way to do this
	if ((PI_CART_ADDR_REG >= 0x08000000 && PI_CART_ADDR_REG <= 0x08010000) ||
		(PI_CART_ADDR_REG >= 0x08040000 && PI_CART_ADDR_REG <= 0x08050000) ||
		(PI_CART_ADDR_REG >= 0x08080000 && PI_CART_ADDR_REG <= 0x08090000))
	{
		switch (SaveUsing) {
		case Auto:
			SaveUsing = Sram;
		case Sram:
			DmaToSram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_RD_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		case FlashRam:
			DmaToFlashram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_WR_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		}
		return;
	}

	if (SaveUsing == FlashRam) {
		if (ShowDebugMessages)
			DisplayError("**** FLashRam DMA Read address %X *****",PI_CART_ADDR_REG);
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		return;
	}
	if (ShowDebugMessages)
		DisplayError("PI_DMA_READ where are you dmaing to ?");
	PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
	MI_INTR_REG |= MI_INTR_PI;
	CheckInterrupts();
	return;
}

void PI_DMA_WRITE (void) {
	DWORD i;	
	PI_DRAM_ADDR_REG &= 0x1FFFFFFF;

	if (PI_WR_LEN_REG > 0x300)
		PI_WR_LEN_REG = ((PI_WR_LEN_REG & 1)) ? PI_WR_LEN_REG : PI_WR_LEN_REG + 1;	// Fix for Ai Shogi 3
	
	int wr_len = PI_WR_LEN_REG;
	int wr_len_cart = PI_WR_LEN_REG;

	if (!wr_len) wr_len = 7;
	if (!wr_len_cart) wr_len_cart = 1;

	PI_WR_LEN_REG -= PI_DRAM_ADDR_REG & 6;

	PI_CART_ADDR_REG &= ~1;	// Taz Express fix
	PI_DRAM_ADDR_REG &= ~1;	// Taz Express fix

	PI_STATUS_REG |= PI_STATUS_DMA_BUSY;
	if ( PI_DRAM_ADDR_REG + PI_WR_LEN_REG + 1 > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("PI_DMA_WRITE not in Memory");
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		return;
	}

	// Trying to fix saves for Dezaemon 3D (J)
	// There is likely a better way to do this
	if ((PI_CART_ADDR_REG >= 0x08000000 && PI_CART_ADDR_REG <= 0x08010000) ||
		(PI_CART_ADDR_REG >= 0x08040000 && PI_CART_ADDR_REG <= 0x08050000) ||
		(PI_CART_ADDR_REG >= 0x08080000 && PI_CART_ADDR_REG <= 0x08090000))
	{
		switch (SaveUsing) {
		case Auto:
			SaveUsing = Sram;
		case Sram:
			DmaFromSram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_WR_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		case FlashRam:
			DmaFromFlashram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_WR_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		}
		return;
	}
	
	if ( PI_CART_ADDR_REG >= 0x06000000 && PI_CART_ADDR_REG < 0x08000000) {
#ifdef ROM_IN_MAPSPACE
		if (WrittenToRom) { 
			DWORD OldProtect;
			VirtualProtect(ROM,RomFileSize,PAGE_READONLY, &OldProtect);
		}
#endif
		PI_CART_ADDR_REG -= 0x06000000;
		if (PI_CART_ADDR_REG + PI_WR_LEN_REG + 1 < RomFileSize) {
			for (i = 0; i < PI_WR_LEN_REG + 1; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
		} else if (RomFileSize > PI_CART_ADDR_REG) {
			DWORD Len;
			Len = RomFileSize - PI_CART_ADDR_REG;
			for (i = 0; i < Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
			for (i = Len; i < PI_WR_LEN_REG + 1 - Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  0;
			}
		}
		PI_CART_ADDR_REG += 0x06000000;

		PI_DRAM_ADDR_REG += wr_len + 1;
		PI_CART_ADDR_REG += wr_len_cart + 1;
		PI_WR_LEN_REG = 0x7f;
		PI_RD_LEN_REG = 0x7f;

		if (!DMAUsed) { 
			DMAUsed = TRUE;
			FirstDMA(); 
		}
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9) + 50);
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9));
		CheckTimer();
		return;
	}

	if ( PI_CART_ADDR_REG >= 0x10000000 && PI_CART_ADDR_REG <= 0x1FBFFFFF) {
#ifdef ROM_IN_MAPSPACE
		if (WrittenToRom) { 
			DWORD OldProtect;
			VirtualProtect(ROM,RomFileSize,PAGE_READONLY, &OldProtect);
		}
#endif
		PI_CART_ADDR_REG -= 0x10000000;
		if (PI_CART_ADDR_REG + PI_WR_LEN_REG + 1 < RomFileSize) {
			for (i = 0; i < PI_WR_LEN_REG + 1; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
		} else if (RomFileSize > PI_CART_ADDR_REG) {
			DWORD Len;
			Len = RomFileSize - PI_CART_ADDR_REG;
			for (i = 0; i < Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
			for (i = Len; i < PI_WR_LEN_REG + 1 - Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  0;
			}
		}
		PI_CART_ADDR_REG += 0x10000000;

		PI_DRAM_ADDR_REG += wr_len + 1;
		PI_CART_ADDR_REG += wr_len_cart + 1;
		PI_WR_LEN_REG = 0x7f;
		PI_RD_LEN_REG = 0x7f;

		if (!DMAUsed) { 
			DMAUsed = TRUE;
			FirstDMA(); 
		}
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9) + 50);
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9));
		CheckTimer();
		return;
	}
	
	if (HaveDebugger && ShowUnhandledMemory) { DisplayError("PI_DMA_WRITE not in ROM"); }
	PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
	MI_INTR_REG |= MI_INTR_PI;
	CheckInterrupts();

}

void SI_DMA_READ (void) {
	BYTE * PifRamPos = &PIF_Ram[0];

	SI_DRAM_ADDR_REG &= 0x7FFFFFFF;

	if ((int)SI_DRAM_ADDR_REG > (int)RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SI DMA\nSI_DRAM_ADDR_REG not in RDRam space");
		return;
	}
	
	PifRamRead();
	SI_DRAM_ADDR_REG &= 0xFFFFFFF8;
	if ((int)SI_DRAM_ADDR_REG < 0) {
		int count, RdramPos;

		RdramPos = (int)SI_DRAM_ADDR_REG;
		for (count = 0; count < 0x40; count++, RdramPos++) {
			if (RdramPos < 0) { continue; }
			N64MEM[RdramPos ^3] = PIF_Ram[count];
		}
	} else {
		_asm {
			mov edi, dword ptr [RegSI]
			mov edi, dword ptr [edi]
			add edi, N64MEM
			mov ecx, PifRamPos
			mov edx, 0		
	memcpyloop:
			mov eax, dword ptr [ecx + edx]
			bswap eax
			mov  dword ptr [edi + edx],eax
			mov eax, dword ptr [ecx + edx + 4]
			bswap eax
			mov  dword ptr [edi + edx + 4],eax
			mov eax, dword ptr [ecx + edx + 8]
			bswap eax
			mov  dword ptr [edi + edx + 8],eax
			mov eax, dword ptr [ecx + edx + 12]
			bswap eax
			mov  dword ptr [edi + edx + 12],eax
			add edx, 16
			cmp edx, 64
			jb memcpyloop
		}
	}
	
	if (HaveDebugger && LogOptions.LogPRDMAMemStores) {
			int count;
			char HexData[100], AsciiData[100], Addon[20];
			LogMessage("\tData DMAed to RDRAM:");			
			LogMessage("\t--------------------");
			for (count = 0; count < 16; count ++ ) {
				if ((count % 4) == 0) { 
					sprintf(HexData,"\0"); 
					sprintf(AsciiData,"\0"); 
				}
 				sprintf(Addon,"%02X %02X %02X %02X", 
					PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
					PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
				strcat(HexData,Addon);
				if (((count + 1) % 4) != 0) {
					sprintf(Addon,"-");
					strcat(HexData,Addon);
				} 
			
				sprintf(Addon,"%c%c%c%c", 
					PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
					PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
				strcat(AsciiData,Addon);
			
				if (((count + 1) % 4) == 0) {
					LogMessage("\t%s %s",HexData, AsciiData);
				} 
			}
			LogMessage("");
	}

	if (DelaySI) {
		ChangeTimer(SiTimer,0x900);
	} else {
		MI_INTR_REG |= MI_INTR_SI;
		SI_STATUS_REG |= SI_STATUS_INTERRUPT;
		CheckInterrupts();
	}
}

void SI_DMA_WRITE (void) {
	BYTE * PifRamPos = &PIF_Ram[0];

	SI_DRAM_ADDR_REG &= 0x7FFFFFFF;

	if ((int)SI_DRAM_ADDR_REG > (int)RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SI DMA\nSI_DRAM_ADDR_REG not in RDRam space");
		return;
	}
	
	SI_DRAM_ADDR_REG &= 0xFFFFFFF8;
	if ((int)SI_DRAM_ADDR_REG < 0) {
		int count, RdramPos;

		RdramPos = (int)SI_DRAM_ADDR_REG;
		for (count = 0; count < 0x40; count++, RdramPos++) {
			if (RdramPos < 0) { PIF_Ram[count] = 0; continue; }
			PIF_Ram[count] = N64MEM[RdramPos ^3];
		}
	} else {
		_asm {
			mov ecx, dword ptr [RegSI]
			mov ecx, dword ptr [ecx]
			add ecx, N64MEM
			mov edi, PifRamPos
			mov edx, 0		
	memcpyloop:
			mov eax, dword ptr [ecx + edx]
			bswap eax
			mov  dword ptr [edi + edx],eax
			mov eax, dword ptr [ecx + edx + 4]
			bswap eax
			mov  dword ptr [edi + edx + 4],eax
			mov eax, dword ptr [ecx + edx + 8]
			bswap eax
			mov  dword ptr [edi + edx + 8],eax
			mov eax, dword ptr [ecx + edx + 12]
			bswap eax
			mov  dword ptr [edi + edx + 12],eax
			add edx, 16
			cmp edx, 64
			jb memcpyloop
		}
	}
	
	if (HaveDebugger && LogOptions.LogPRDMAMemLoads) {
		int count;
		char HexData[100], AsciiData[100], Addon[20];
		LogMessage("");
		LogMessage("\tData DMAed to the Pif Ram:");			
		LogMessage("\t--------------------------");
		for (count = 0; count < 16; count ++ ) {
			if ((count % 4) == 0) { 
				sprintf(HexData,"\0"); 
				sprintf(AsciiData,"\0"); 
			}
			sprintf(Addon,"%02X %02X %02X %02X", 
				PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
				PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
			strcat(HexData,Addon);
			if (((count + 1) % 4) != 0) {
				sprintf(Addon,"-");
				strcat(HexData,Addon);
			} 
			
			sprintf(Addon,"%c%c%c%c", 
				PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
				PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
			strcat(AsciiData,Addon);
			
			if (((count + 1) % 4) == 0) {
				LogMessage("\t%s %s",HexData, AsciiData);
			} 
		}
		LogMessage("");
	}

	PifRamWrite();
	
	if (DelaySI) {
		ChangeTimer(SiTimer,0x900);
	} else {
		MI_INTR_REG |= MI_INTR_SI;
		SI_STATUS_REG |= SI_STATUS_INTERRUPT;
		CheckInterrupts();
	}
}

char *get_geometry_mode_flags(DWORD flags) {
	if (flags == 0xffffffff) {
		// This definition doesn't actually exist, but it looks better...
		return "G_ALL";
	}

	static char flag_str[200] = { 0 };

	flag_str[0] = 0;

	if (flags & 0x000001) {
		strcpy(flag_str, "G_ZBUFFER");
	}
	if (flags & 0x000004) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_SHADE");
	}
	if (flags & 0x000020) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_SHADING_SMOOTH");
	}

	if ((flags & 0x003000) == 0x003000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_CULL_BOTH");
	} else if (flags & 0x002000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_CULL_BACK");
	} else if (flags & 0x001000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_CULL_FRONT");
	}

	if (flags & 0x010000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_FOG");
	}
	if (flags & 0x020000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_LIGHTING");
	}
	if (flags & 0x040000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_TEXTURE_GEN");
	}
	if (flags & 0x080000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_TEXTURE_GEN_LINEAR");
	}
	if (flags & 0x800000) {
		if (flag_str[0]) {
			strcat(flag_str, " | ");
		}
		strcat(flag_str, "G_CLIPPING");
	}

	if (flag_str[0] == 0) {
		strcpy(flag_str, "0");
	}

	return flag_str;
}

char *get_render_mode_flags(DWORD flags) {
	return "TODO";
}

void process_display_list(FILE *f, DWORD addr, int depth) {
	char *spacing = "                    ";
	char *indent = spacing + 20 - depth * 2;
	char *padding = spacing + depth * 2;
	static DWORD segment_map[16] = { 0 };

#define TRANSLATE(addr) (segment_map[(addr >> 24) & 0x0f] + (addr & 0x00fffff8))

	while (TRUE) {
		DWORD word1 = *(DWORD *)(N64MEM + addr);
		DWORD word2 = *(DWORD *)(N64MEM + addr + 4);

		fprintf(f, "%s%08x %08x%s", indent, word1, word2, padding);
		switch (word1 >> 24) {
		case 0x01:
			fputs(" ; gsSpMatrix(); // TODO", f);
			break;
		case 0x03:
			fputs(" ; G_MOVEMEM; // TODO: gsSPViewport, gsSPForceMatrix, gsSPLight, gsSPLookAtX, gsSPLookAtY", f);
			break;
		case 0x04: {
			// F3DEX_GBI || F3DLP_GBI
			int n = (word1 >> 10) & 0x3f;
			int v0 = (word1 >> 17) & 0x7f;
			int length = (word1 & 0x03ff) + 1;
			// !F3DEX_GBI_2 && !F3DEX_GBI && !F3DLP_GBI
			//int n = ((word1 >> 20) & 0x0f) + 1;
			//int v0 = (word1 >> 16) & 0x0f;
			//int length = word1 & 0xffff;
			fprintf(f, " ; gsSpVertex(0x%08x, %d, %d); // length = %d", TRANSLATE(word2) | 0x80000000, n, v0, length);
			break;
		}
		case 0x06:
			if ((word1 >> 16) & 1) {
				fprintf(f, " ; gsSPBranchList(0x%08x)\n", TRANSLATE(word2) | 0x80000000);
				process_display_list(f, TRANSLATE(word2), depth);
				return;
			} else {
				fprintf(f, " ; gsSPDisplayList(0x%08x)\n", TRANSLATE(word2) | 0x80000000);
				process_display_list(f, TRANSLATE(word2), depth + 1);
			}
			break;
		case 0xaf:
			fputs(" ; G_LOAD_UCODE; // TODO", f);
			break;
		case 0xb0:
			fputs(" ; G_BRANCHZ; // TODO", f);
			break;
		case 0xb1: {
			int v00 = (word1 >> 17) & 0x7f;
			int v01 = (word1 >> 9) & 0x7f;
			int v02 = (word1 >> 1) & 0x7f;
			int v10 = (word2 >> 17) & 0x7f;
			int v11 = (word2 >> 9) & 0x7f;
			int v12 = (word2 >> 1) & 0x7f;
			fprintf(f, " ; gsSP2Triangles(%d, %d, %d, _, %d, %d, %d, _);", v00, v01, v02, v10, v11, v12);
			break;
		}
		case 0xb2:
			fputs(" ; G_MODIFYVTX; // TODO", f);
			break;
		case 0xb3:
			fputs(" ; G_RDPHALF2; // TODO", f);
			break;
		case 0xb4:
			fputs(" ; G_RDPHALF1; // TODO", f);
			break;
		case 0xb5:
			fputs(" ; G_LINE3D; // TODO", f);
			break;
		case 0xb6: {
			char *flags = get_geometry_mode_flags(word2);
			fprintf(f, " ; gsSPClearGeometryMode(%s);", flags);
			break;
		}
		case 0xb7: {
			char *flags = get_geometry_mode_flags(word2);
			fprintf(f, " ; gsSPSetGeometryMode(%s);", flags);
			break;
		}
		case 0xb8:
			fputs(" ; gsSPEndDisplayList();", f);
			return;
		case 0xb9: {
			int length = word1 & 0xff;
			switch ((word1 >> 8) & 0xff) {
			case 0:
				fprintf(f, " ; gsDPSetAlphaCompare(0x%08x); // length = %d", word2, length);
				break;
			case 2:
				fprintf(f, " ; gsDPSetDepthSource(0x%08x); // length = %d", word2, length);
				break;
			case 3: {
				char *flags = get_render_mode_flags(word2);
				fprintf(f, " ; gsDPSetRenderMode(%s);", flags);
				break;
			}
			default:
				fputs(" ; // UNKNOWN", f);
				break;
			}
			break;
		}
		case 0xba:
			fputs(" ; G_SETOTHERMODE_H; // TODO", f);
			break;
		case 0xbb:
			fputs(" ; G_TEXTURE; // TODO", f);
			break;
		case 0xbc:
			switch (word1 & 0xff) {
			case 0:
				fputs(" ; gsSPInsertMatrix(...); // TODO", f);
				break;
			case 2:
				fputs(" ; gsSPNumLights(...); // TODO", f);
				break;
			case 4:
				fputs(" ; gsSPClipRatio(...); // TODO", f);
				break;
			case 6: {
				int seg = (word1 >> 10) & 0x3fff;
				fprintf(f, " ; gsSPSegment(%d, 0x%08x);", seg, word2 & 0x00fffff8);
				segment_map[seg] = word2 & 0x00fffff8;
				break;
			}
			case 8:
				fputs(" ; gsSPFogPosition(...); // TODO", f);
				break;
			case 10:
				fputs(" ; gsSPLightColor(...); // TODO", f);
				break;
			case 12:
				fputs(" ; gsSPModifyVertex(...); // TODO", f);
				break;
			case 14:
				fputs(" ; gsSPPerspNormalize(...); // TODO", f);
				break;
			default:
				fputs(" ; // G_MOVEWORD UNKNOWN index", f);
				break;
			}
			break;
		case 0xbd:
			fputs(" ; G_POPMTX; // TODO", f);
			break;
		case 0xbe:
			fputs(" ; G_CULLDL; // TODO", f);
			break;
		case 0xbf: {
			int v0 = (word2 >> 17) & 0x7f;
			int v1 = (word2 >> 9) & 0x7f;
			int v2 = (word2 >> 1) & 0x7f;
			fprintf(f, " ; gsSP1Triangle(%d, %d, %d, _);", v0, v1, v2);
			break;
		}
		case 0xc0:
			fputs(" ; G_NOOP;", f);
			break;
		case 0xe4:
			fputs(" ; G_TEXRECT; // TODO", f);
			break;
		case 0xe5:
			fputs(" ; G_TEXRECTFLIP; // TODO", f);
			break;
		case 0xe6:
			fputs(" ; G_RDPLOADSYNC; // TODO", f);
			break;
		case 0xe7:
			fputs(" ; G_RDPPIPESYNC; // TODO", f);
			break;
		case 0xe8:
			fputs(" ; G_RDPTILESYNC; // TODO", f);
			break;
		case 0xe9:
			fputs(" ; G_RDPFULLSYNC; // TODO", f);
			break;
		case 0xea:
			fputs(" ; G_SETKEYGB; // TODO", f);
			break;
		case 0xeb:
			fputs(" ; G_SETKEYR; // TODO", f);
			break;
		case 0xec:
			fputs(" ; G_SETCONVERT; // TODO", f);
			break;
		case 0xed:
			fputs(" ; G_SETSCISSOR; // TODO", f);
			break;
		case 0xee:
			fputs(" ; G_SETPRIMDEPTH; // TODO", f);
			break;
		case 0xef:
			fputs(" ; G_RDPSETOTHERMODE; // TODO", f);
			break;
		case 0xf0:
			fputs(" ; G_LOADTLUT; // TODO", f);
			break;
		case 0xf2:
			fputs(" ; G_SETTILESIZE; // TODO", f);
			break;
		case 0xf3:
			fputs(" ; G_LOADBLOCK; // TODO", f);
			break;
		case 0xf4:
			fputs(" ; G_LOADTILE; // TODO", f);
			break;
		case 0xf5:
			fputs(" ; G_SETTILE; // TODO", f);
			break;
		case 0xf6:
			fputs(" ; G_FILLRECT; // TODO", f);
			break;
		case 0xf7:
			fputs(" ; G_SETFILLCOLOR; // TODO", f);
			break;
		case 0xf8: {
			BYTE r = (word2 >> 24) & 0xff;
			BYTE g = (word2 >> 16) & 0xff;
			BYTE b = (word2 >> 8) & 0xff;
			BYTE a = (word2 >> 0) & 0xff;
			fprintf(f, " ; gsDPSetFogColor(%d, %d, %d, %d);", r, g, b, a);
			break;
		}
		case 0xf9:
			fputs(" ; G_SETBLENDCOLOR; // TODO", f);
			break;
		case 0xfa:
			fputs(" ; G_SETPRIMCOLOR; // TODO", f);
			break;
		case 0xfb:
			fputs(" ; G_SETENVCOLOR; // TODO", f);
			break;
		case 0xfc:
			fputs(" ; G_SETCOMBINE; // TODO", f);
			break;
		case 0xfd:
			fputs(" ; G_SETTIMG; // TODO", f);
			break;
		case 0xfe:
			fputs(" ; G_SETZIMG; // TODO", f);
			break;
		case 0xff:
			fputs(" ; G_SETCIMG; // TODO", f);
			break;
		default:
			break;
		}
		fputc('\n', f);

		addr += 8;
	}
}

void SP_DMA_READ (void) { 
	SP_DRAM_ADDR_REG &= 0x1FFFFFFF;

	if (SP_DRAM_ADDR_REG > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SP DMA\nSP_DRAM_ADDR_REG not in RDRam space");
		SP_DMA_BUSY_REG = 0;
		SP_STATUS_REG  &= ~SP_STATUS_DMA_BUSY;
		return;
	}
	
	if (SP_RD_LEN_REG + 1  + (SP_MEM_ADDR_REG & 0xFFF) > 0x1000) {
		if (ShowDebugMessages)
			DisplayError("SP DMA\ncould not fit copy in memory segement");
		SP_DMA_BUSY_REG = 0;
		SP_STATUS_REG  &= ~SP_STATUS_DMA_BUSY;
		return;		
	}

	if ((SP_MEM_ADDR_REG & 3) != 0 || (SP_DRAM_ADDR_REG & 3) != 0 || ((SP_RD_LEN_REG + 1) & 3) != 0) {
		DisplayErrorFatal("Nonstandard DMA Transfer.\nStopping Emulation.");
		ExitThread(0);
	}
	/*
	if ((SP_MEM_ADDR_REG & 3) != 0) { _asm int 3 }
	if ((SP_DRAM_ADDR_REG & 3) != 0) { _asm int 3 }
	if (((SP_RD_LEN_REG + 1) & 3) != 0) { _asm int 3 }
	*/
	memcpy( DMEM + (SP_MEM_ADDR_REG & 0x1FFF), N64MEM + SP_DRAM_ADDR_REG,
		SP_RD_LEN_REG + 1 );
		
	SP_DMA_BUSY_REG = 0;
	SP_STATUS_REG &= ~SP_STATUS_DMA_BUSY;

	if ((SP_MEM_ADDR_REG & 0x1fff) == 0x0fc0) {
		// osSpTaskLoad
		if (CPU_Action.DlDebug) {
			// Only log Graphics display lists (ignore audio lists and others)
			DWORD type = *(DWORD *)(N64MEM + SP_DRAM_ADDR_REG);
			if (type != 1) {
				return;
			}

			FILE *f = fopen("dl-debug.txt", "a");
			if (f == NULL) {
				return;
			}

			DWORD addr = *(DWORD*)(N64MEM + SP_DRAM_ADDR_REG + 0x30) & 0x007fffff;
			process_display_list(f, addr, 0);

			fclose(f);

			CPU_Action.DlDebugDone = TRUE;
		}
	}
}

void SP_DMA_WRITE (void) { 
	SP_DRAM_ADDR_REG &= 0x7FFFFFFF;

	if (SP_DRAM_ADDR_REG > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SP DMA WRITE\nSP_DRAM_ADDR_REG not in RDRam space");
		return;
	}
	
	if (SP_WR_LEN_REG + 1 + (SP_MEM_ADDR_REG & 0xFFF) > 0x1000) {
		if (ShowDebugMessages)
			DisplayError("SP DMA WRITE\ncould not fit copy in memory segement");
		return;		
	}

	if ((SP_MEM_ADDR_REG & 3) != 0) { _asm int 3 }
	if ((SP_DRAM_ADDR_REG & 3) != 0) { _asm int 3 }
	if (((SP_WR_LEN_REG + 1) & 3) != 0) { _asm int 3 }

	memcpy( N64MEM + SP_DRAM_ADDR_REG, DMEM + (SP_MEM_ADDR_REG & 0x1FFF),
		SP_WR_LEN_REG + 1);
		
	SP_DMA_BUSY_REG = 0;
	SP_STATUS_REG  &= ~SP_STATUS_DMA_BUSY;
}
