/*
 * Includes code from the gummiboot project:
 * Copyright (C) 2012-2013 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2012 Harald Hoyer <harald@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
*/
#include <efi.h>
#include <efilib.h>

static EFI_GUID gEfiGlobalVariableGuid = EFI_GLOBAL_VARIABLE;

typedef struct registers registers;
struct registers {
	UINT32 eax;
	UINT32 ebx;
	UINT32 ecx;
	UINT32 edx;
};

void Sleep(UINTN seconds_to_sleep) {
	uefi_call_wrapper(BS->Stall, 1, seconds_to_sleep * 1000 * 1000);
}	

void cpuid(UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx) {
	asm("cpuid;" :"+a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx));
}

void rdmsr(UINT32 reg, UINT32 *upper, UINT32 *lower) {
	asm("rdmsr;": "=d" (*upper), "=a" (*lower): "c" (reg));
}

void wrmsr(UINT32 reg, UINT32 upper, UINT32 lower) {
	asm("wrmsr;": : "d" (upper), "a" (lower), "c" (reg));
}

void print_extended_vendor() {
	union {
		registers regs;
		UINT8 buffer[sizeof(registers)];
	} data;
	CHAR8 vendor[50];

	data.regs.eax = 0x80000002;
	cpuid(&data.regs.eax, &data.regs.ebx, &data.regs.ecx, &data.regs.edx);
	CopyMem(vendor, data.buffer, sizeof(data.buffer));
	data.regs.eax = 0x80000003;
	cpuid(&data.regs.eax, &data.regs.ebx, &data.regs.ecx, &data.regs.edx);
	CopyMem(vendor + 16, data.buffer, sizeof(data.buffer));
	data.regs.eax = 0x80000004;
	cpuid(&data.regs.eax, &data.regs.ebx, &data.regs.ecx, &data.regs.edx);
	CopyMem(vendor + 32, data.buffer, sizeof(data.buffer));
	Print(L"Extended vendor: %a\n", vendor);
}

BOOLEAN load_boot_option(EFI_HANDLE ImageHandle, CHAR16* skipOption, UINT16 code, EFI_HANDLE* image) {
	CHAR16 VarBoot[16] = { 0 };
	EFI_STATUS err;
	void* boot;

	CHAR16* Description;
	EFI_DEVICE_PATH* path;

	SPrint(VarBoot, sizeof(VarBoot), L"Boot%04x", code);
		boot = LibGetVariable(VarBoot, &gEfiGlobalVariableGuid);

	Description = boot + sizeof(UINT32) + sizeof(UINT16);
	
	if (StrCmp(skipOption, Description) == 0) {
		Print(L"Skipping Hello Chain loader.\n");
		FreePool(boot);
		return FALSE;
	}

	path = (void*)Description + StrSize(Description);

	Print(L"Description: %s\n", Description);
		Input(L"Press any key to boot this option.", NULL, 0);
	err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, ImageHandle, path , NULL, 0, image);

	if (EFI_ERROR(err)) {
		Print(L"Error loading %s: %r", Description, err);
		FreePool(boot);
		Sleep(3);
		return FALSE;
	}

	FreePool(boot);
	return TRUE;
}


EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	UINT16 *boot_order;
	UINT16 *pos;
	UINTN size;
	EFI_HANDLE image;
	EFI_STATUS err;

	InitializeLib(ImageHandle, SystemTable);

	print_extended_vendor();

	boot_order = LibGetVariableAndSize(VarBootOrder, &gEfiGlobalVariableGuid, &size);

	for (pos = boot_order; pos < boot_order + size; pos++) {
		if (load_boot_option(ImageHandle, L"Hello Chain loader", *pos, &image)) {		
			FreePool(boot_order);
			err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
			if (EFI_ERROR(err)) {
				Print(L"Error starting selected image: %r", err);
				Sleep(3);
			}
		}
	}

	FreePool(boot_order);

	Input(L"Press any key to continue", NULL, 0);
 
	return EFI_SUCCESS;
}
