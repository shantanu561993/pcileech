// pcileech.c : implementation of core pcileech functionality.
//
// (c) Ulf Frisk, 2016, 2017
// Author: Ulf Frisk, pcileech@frizk.net
//
#include "pcileech.h"
#include "device.h"
#include "device3380.h"
#include "executor.h"
#include "extra.h"
#include "help.h"
#include "memdump.h"
#include "mempatch.h"
#include "util.h"
#include "kmd.h"
#include "vfs.h"

BOOL PCILeechConfigIntialize(_In_ DWORD argc, _In_ char* argv[], _Inout_ PPCILEECH_CONTEXT ctx)
{
	struct ACTION {
		ACTION_TYPE tp;
		LPSTR sz;
	} ACTION;
	const struct ACTION ACTIONS[] = {
		{.tp = INFO,.sz = "info"},
		{.tp = DUMP,.sz = "dump" },
		{.tp = WRITE,.sz = "write" },
		{.tp = PATCH,.sz = "patch" },
		{.tp = SEARCH,.sz = "search" },
		{.tp = KMDLOAD,.sz = "kmdload" },
		{.tp = KMDEXIT,.sz = "kmdexit" },
		{.tp = USB3380_FLASH,.sz = "flash" },
		{.tp = MOUNT,.sz = "mount" },
		{.tp = USB3380_START8051,.sz = "8051start" },
		{.tp = USB3380_STOP8051,.sz = "8051stop" },
		{.tp = DISPLAY,.sz = "display" },
		{.tp = PAGEDISPLAY,.sz = "pagedisplay" },
		{.tp = TESTMEMREAD,.sz = "testmemread" },
		{.tp = TESTMEMREADWRITE,.sz = "testmemreadwrite" },
		{.tp = MAC_FVRECOVER,.sz = "mac_fvrecover" },
		{.tp = MAC_FVRECOVER2,.sz = "mac_fvrecover2" },
		{.tp = MAC_DISABLE_VTD,.sz = "mac_disablevtd" },
		{.tp = PT_PHYS2VIRT,.sz = "pt_phys2virt" },
		{.tp = TLP,.sz = "tlp" },
		{.tp = PROBE,.sz = "probe" },
	};
	DWORD j, i = 1;
	if(argc < 2) { return FALSE; }
	// allocate memory for config struct
	ctx->cfg = LocalAlloc(LMEM_ZEROINIT, sizeof(CONFIG));
	if(!ctx->cfg) { return FALSE; }
	// set defaults
	ctx->cfg->tpAction = NA;
	ctx->cfg->qwAddrMax = ~0;
	ctx->cfg->fOutFile = TRUE;
	ctx->cfg->qwMaxSizeDmaIo = ~0;
	// fetch command line actions/options
	loop:
	while(i < argc) {
		for(j = 0; j < sizeof(ACTIONS) / sizeof(ACTION); j++) { // parse command (if found)
			if(0 == strcmp(argv[i], ACTIONS[j].sz)) {
				ctx->cfg->tpAction = ACTIONS[j].tp;
				i++;
				goto loop;
			}
		}
		if(ctx->cfg->tpAction == NA && 0 != memcmp(argv[i], "-", 1)) {
			ctx->cfg->tpAction = EXEC;
			strcpy_s(ctx->cfg->szShellcodeName, MAX_PATH, argv[i]);
			i++;
			continue;
		}
		// parse options (command not found)
		if(0 == strcmp(argv[i], "-pt")) {
			ctx->cfg->fPageTableScan = TRUE;
			i++;
			continue;
		} else if(0 == strcmp(argv[i], "-all")) {
			ctx->cfg->fPatchAll = TRUE;
			i++;
			continue;
		} else if(0 == strcmp(argv[i], "-force")) {
			ctx->cfg->fForceRW = TRUE;
			i++;
			continue;
		} else if(0 == strcmp(argv[i], "-help")) {
			ctx->cfg->fShowHelp = TRUE;
			i++;
			continue;
		} else if(0 == _stricmp(argv[i], "-usb2")) {
			ctx->cfg->fForceUsb2 = TRUE;
			i++;
			continue;
		} else if(0 == _stricmp(argv[i], "-v")) {
			ctx->cfg->fVerbose = TRUE;
			i++;
			continue;
		} else if(0 == _stricmp(argv[i], "-vv")) {
			ctx->cfg->fVerbose = TRUE;
			ctx->cfg->fVerboseExtra = TRUE;
			i++;
			continue;
		} else if(i + 1 >= argc) {
			return FALSE;
		} else if(0 == strcmp(argv[i], "-min")) {
			ctx->cfg->qwAddrMin = Util_GetNumeric(argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-max")) {
			ctx->cfg->qwAddrMax = Util_GetNumeric(argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-cr3")) {
			ctx->cfg->qwCR3 = Util_GetNumeric(argv[i + 1]);
		}else if(0 == strcmp(argv[i], "-efibase")) {
			ctx->cfg->qwEFI_IBI_SYST = Util_GetNumeric(argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-iosize")) {
			ctx->cfg->qwMaxSizeDmaIo = Util_GetNumeric(argv[i + 1]);
			ctx->cfg->qwMaxSizeDmaIo = ~0xfff & max(0x1000, ctx->cfg->qwMaxSizeDmaIo);
		} else if(0 == strcmp(argv[i], "-tlpwait")) {
			ctx->cfg->dwListenTlpTimeMs = (DWORD)(1000 * Util_GetNumeric(argv[i + 1]));
		} else if(0 == strcmp(argv[i], "-device")) {
			ctx->cfg->dev.tp = PCILEECH_DEVICE_NA;
			if(0 == _stricmp(argv[i + 1], "usb3380")) { 
				ctx->cfg->dev.tp = PCILEECH_DEVICE_USB3380;
			} else if(0 == _stricmp(argv[i + 1], "sp605_ft601")) {
				ctx->cfg->dev.tp = PCILEECH_DEVICE_SP605_FT601;
			} else if(0 == _stricmp(argv[i + 1], "sp605_tcp")) {
				ctx->cfg->dev.tp = PCILEECH_DEVICE_SP605_TCP;
			}
		} else if(0 == strcmp(argv[i], "-device-addr")) {
			ctx->cfg->TcpAddr = inet_addr(argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-device-port")) {
			ctx->cfg->TcpPort = atoi(argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-out")) {
			if((0 == _stricmp(argv[i + 1], "none")) || (0 == _stricmp(argv[i + 1], "null"))) {
				ctx->cfg->fOutFile = FALSE;
			} else {
				strcpy_s(ctx->cfg->szFileOut, MAX_PATH, argv[i + 1]);
			}
		} else if(0 == strcmp(argv[i], "-in")) {
			ctx->cfg->cbIn = max(0x40000, 0x1000 + Util_GetFileSize(argv[i + 1]));
			ctx->cfg->pbIn = LocalAlloc(LMEM_ZEROINIT, ctx->cfg->cbIn);
			if(!ctx->cfg->pbIn) { return FALSE; }
			if(!Util_ParseHexFileBuiltin(argv[i + 1], ctx->cfg->pbIn, (DWORD)ctx->cfg->cbIn, (PDWORD)&ctx->cfg->cbIn)) { return FALSE; }
		} else if(0 == strcmp(argv[i], "-s")) {
			strcpy_s(ctx->cfg->szInS, MAX_PATH, argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-sig")) {
			strcpy_s(ctx->cfg->szSignatureName, MAX_PATH, argv[i + 1]);
		} else if(0 == strcmp(argv[i], "-kmd")) {
			ctx->cfg->qwKMD = strtoull(argv[i + 1], NULL, 16);
			if(ctx->cfg->qwKMD < 0x1000) {
				strcpy_s(ctx->cfg->szKMDName, MAX_PATH, argv[i + 1]);
			} else {
				ctx->cfg->fAddrKMDSetByArgument = TRUE;
			}
		} else if(2 == strlen(argv[i]) && '0' <= argv[i][1] && '9' >= argv[i][1]) { // -0..9 param
			ctx->cfg->qwDataIn[argv[i][1] - '0'] = Util_GetNumeric(argv[i + 1]);
		} else if(!memcmp(argv[i], "-device-opt", 11) && (argv[i][11] >= '0') && (argv[i][11] <= '3')) { // -devopt[0-3] (device options)
			ctx->cfg->qwDeviceOpt[argv[i][11] - '0'] = Util_GetNumeric(argv[i + 1]);
		}
		i += 2;
	}
	if(!ctx->cfg->pbIn) {
		ctx->cfg->pbIn = LocalAlloc(LMEM_ZEROINIT, 0x40000);
	}
	// try correct erroneous options, if needed
	if(ctx->cfg->tpAction == NA) {
		return FALSE;
	}
	return TRUE;
}

VOID PCILeechConfigFixup(_Inout_ PPCILEECH_CONTEXT ctx)
{
	QWORD qw;
	// device specific configuration
	ctx->cfg->qwMaxSizeDmaIo = min(ctx->cfg->qwMaxSizeDmaIo, ctx->cfg->dev.qwMaxSizeDmaIo);
	// no kmd -> max address == max address that device support
	if(!ctx->cfg->szKMDName[0] && !ctx->cfg->qwKMD) {
		if(ctx->cfg->qwAddrMax == 0 || ctx->cfg->qwAddrMax > ctx->cfg->dev.qwAddrMaxNative) {
			ctx->cfg->qwAddrMax = ctx->cfg->dev.qwAddrMaxNative;
		}
	}
	// fixup addresses
	if(ctx->cfg->qwAddrMin > ctx->cfg->qwAddrMax) {
		qw = ctx->cfg->qwAddrMin;
		ctx->cfg->qwAddrMin = ctx->cfg->qwAddrMax;
		ctx->cfg->qwAddrMax = qw;
	}
	ctx->cfg->qwCR3 &= ~0xfff;
	ctx->cfg->qwKMD &= ~0xfff;
}

VOID PCILeechFreeContext(_Inout_ PPCILEECH_CONTEXT ctx)
{
	if(!ctx) { return; }
	KMDClose(ctx);
	DeviceClose(ctx);
	if(ctx->cfg) { 
		if(ctx->cfg->pbIn) { LocalFree(ctx->cfg->pbIn); }
		LocalFree(ctx->cfg); 
	}
	if(ctx) { LocalFree(ctx); }
}

int main(_In_ int argc, _In_ char* argv[])
{
	BOOL result;
	PKMDEXEC pKmdExec = NULL;
	PPCILEECH_CONTEXT ctx;
	printf("\n");
	ctx = LocalAlloc(LMEM_ZEROINIT, sizeof(PCILEECH_CONTEXT));
	if(!ctx) {
		printf("PCILEECH: Out of memory.\n");
		return 1;
	}
	//LPSTR szTMP[] = { "", "-device", "SP605_TCP", "-device-addr", "192.168.1.2", "dump", "-out", "none", "-min", "0x100000000", "-max", "0x110010000" };
	//LPSTR szTMP[] = { "", "kmdload", "-kmd", "win10x64_ntfs_20170919_14240.kmd", "-min", "0x100000000" };
	//LPSTR szTMP[] = { "", "mount", "-kmd", "win10_x64"};
	//LPSTR szTMP[] = { "", "write", "-min", "0xc6010000", "-in", "c:\\temp\\16M_zero.raw"};
	//LPSTR szTMP[] = { "", "pagedisplay", "-min", "0x1000", "-vv"};
	///LPSTR szTMP[] = { "", "dump", "-out", "none", "-min", "0x100000000", "-max", "0x120000000"};
	//LPSTR szTMP[] = { "", "tlp", "-in", "00000000c30000ffc1000000", "-vv"};
	//result = PCILeechConfigIntialize(sizeof(szTMP) / sizeof(LPSTR), szTMP, ctx);
	result = PCILeechConfigIntialize((DWORD)argc, argv, ctx);
	if(!result) {
		Help_ShowGeneral();
		PCILeechFreeContext(ctx);
		return 1;
	}
	if(ctx->cfg->tpAction == EXEC) {
		result = Util_LoadKmdExecShellcode(ctx->cfg->szShellcodeName, &pKmdExec);
		LocalFree(pKmdExec);
		if(!result) {
			Help_ShowGeneral();
			PCILeechFreeContext(ctx);
			return 1;
		}
	}
	// actions that do not require a working initialized connection to a pcileech
	// device to start executing the command are found below:
	if(ctx->cfg->tpAction == INFO || ctx->cfg->tpAction == MAC_FVRECOVER2 || ctx->cfg->tpAction == MAC_DISABLE_VTD || ctx->cfg->fShowHelp) {
		if(ctx->cfg->tpAction == INFO) {
			Help_ShowInfo();
		} else if(ctx->cfg->tpAction == MAC_FVRECOVER2) {
			Action_MacFilevaultRecover(ctx, FALSE);
		} else if(ctx->cfg->tpAction == MAC_DISABLE_VTD) {
			Action_MacDisableVtd(ctx);
		} else if(ctx->cfg->fShowHelp) {
			Help_ShowDetailed(ctx->cfg);
		}
		PCILeechFreeContext(ctx);
		return 0;
	}
	result = DeviceOpen(ctx);
	if(!result) {
		printf("PCILEECH: Failed to connect to the device.\n");
		PCILeechFreeContext(ctx);
		return 1;
	}
	PCILeechConfigFixup(ctx); // post device config adjustments
	if(ctx->cfg->szKMDName[0] || ctx->cfg->qwKMD) {
		result = KMDOpen(ctx);
		if(!result) {
			printf("PCILEECH: Failed to load kernel module.\n");
			PCILeechFreeContext(ctx);
			return 1;
		}
	}
	if(ctx->cfg->tpAction == DUMP) {
		ActionMemoryDump(ctx);
	} else if(ctx->cfg->tpAction == WRITE) {
		ActionMemoryWrite(ctx);
	} else if(ctx->cfg->tpAction == DISPLAY) {
		ActionMemoryDisplay(ctx);
	} else if(ctx->cfg->tpAction == PAGEDISPLAY) {
		ActionMemoryPageDisplay(ctx);
	} else if(ctx->cfg->tpAction == PATCH) {
		ActionPatchAndSearch(ctx);
	} else if(ctx->cfg->tpAction == SEARCH) {
		ActionPatchAndSearch(ctx);
	} else if(ctx->cfg->tpAction == USB3380_FLASH) {
		Action_Device3380_Flash(ctx);
	} else if(ctx->cfg->tpAction == USB3380_START8051) {
		Action_Device3380_8051Start(ctx);
	} else if(ctx->cfg->tpAction == USB3380_STOP8051) {
		Action_Device3380_8051Stop(ctx);
	} else if(ctx->cfg->tpAction == EXEC) {
		ActionExecShellcode(ctx);
	} else if(ctx->cfg->tpAction == TESTMEMREAD || ctx->cfg->tpAction == TESTMEMREADWRITE) {
		ActionMemoryTestReadWrite(ctx);
	} else if(ctx->cfg->tpAction == MAC_FVRECOVER) {
		Action_MacFilevaultRecover(ctx, TRUE);
	} else if(ctx->cfg->tpAction == PT_PHYS2VIRT) {
		Action_PT_Phys2Virt(ctx);
	} else if(ctx->cfg->tpAction == TLP) {
		Action_TlpTx(ctx);
	} else if(ctx->cfg->tpAction == PROBE) {
		ActionMemoryProbe(ctx);
	} else if(ctx->cfg->tpAction == MOUNT) {
		ActionMount(ctx);
	} else if(ctx->cfg->tpAction == KMDLOAD) {
		if(ctx->cfg->qwKMD) {
			printf("KMD: Successfully loaded at address: 0x%08x\n", (DWORD)ctx->cfg->qwKMD);
		} else {
			printf("KMD: Failed. Please supply valid -kmd and optionally -cr3 parameters.\n");
		}
	} else if(ctx->cfg->tpAction == KMDEXIT) {
		KMDUnload(ctx);
		printf("KMD: Hopefully unloaded.\n");
	} else {
		printf("Failed. Not yet implemented.\n");
	}
	if(ctx->phKMD && (ctx->cfg->tpAction != KMDLOAD) && !ctx->cfg->fAddrKMDSetByArgument) {
		KMDUnload(ctx);
		printf("KMD: Hopefully unloaded.\n");
	}
	DeviceListenTlp(ctx, ctx->cfg->dwListenTlpTimeMs);
	PCILeechFreeContext(ctx);
	ExitProcess(0);
	return 0;
}
