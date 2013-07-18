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


void ints_to_string(CHAR8* buffer, UINT32* a, UINT32* b, UINT32* c, UINT32* d) {
    UINT32* ints[] = {a, b, c, d, NULL};
    UINT32** pos = ints;

    while(*pos) {
        CopyMem(buffer, (void*)*pos, 4);
        buffer += 4;
        pos++;
    }
}

struct pvclock_wall_clock {
	UINT32   version;
	UINT32   sec;
	UINT32   nsec;
} __attribute__((__packed__));

void print_time() {
  struct pvclock_wall_clock clock;

  wrmsr(0x4b564d00, (UINT32)((UINT64)&clock>>32 & 0xffffffff), (UINT32)((UINT64)&clock & 0xffffffff));
  Print(L"Time: %d\n", clock.sec);
}

void print_vendor() {
  UINT32 eax, ebx, ecx, edx;
  CHAR8 vendor[15] = {0};

  eax = 0;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor, &ebx, &edx, &ecx, NULL);
  Print(L"Vendor: %a\n", vendor);
}

void print_extended_vendor() {
  UINT32 eax, ebx, ecx, edx;
  CHAR8 vendor[50];

  eax = 0x80000002;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor, &eax, &ebx, &ecx, &edx);
  eax = 0x80000003;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor+16, &eax, &ebx, &ecx, &edx);
  eax = 0x80000004;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor+32, &eax, &ebx, &ecx, &edx);
  Print(L"Extended vendor: %a\n", vendor);
}

void boot_option(EFI_HANDLE ImageHandle, CHAR16* skipOption, UINT16 code) {
	void* boot;
	CHAR16 VarBoot[16] = { 0 };
	EFI_HANDLE image;
	EFI_STATUS err;

	CHAR16* Description;
	EFI_DEVICE_PATH* path;

	SPrint(VarBoot, sizeof(VarBoot), L"Boot%04x", code);
  	boot = LibGetVariable(VarBoot, &gEfiGlobalVariableGuid);

	Description = boot + sizeof(UINT32) + sizeof(UINT16);
	
	if (StrCmp(skipOption, Description) == 0) {
		Print(L"Skipping Hello Chain loader.\n");
		FreePool(boot);
		return;
	}

	path = (void*)Description + StrSize(Description);

	Print(L"Description: %s\n", Description);
	Sleep(3);
  	Input(L"Press any key to boot this option.", NULL, 0);
	err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, ImageHandle, path , NULL, 0, &image);

	if (EFI_ERROR(err)) {
		Print(L"Error loading %s: %r", Description, err);
		FreePool(boot);
		Sleep(3);
		return;
	}
	
	err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
	if (EFI_ERROR(err)) {
		Print(L"Error starting %s: %r", Description, err);
		Sleep(3);
	}

	FreePool(boot);
}


EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  UINT16 *boot_order;
  UINT16 *pos;
  UINTN size;

  InitializeLib(ImageHandle, SystemTable);

  print_time();
  print_vendor();
  print_extended_vendor();

  boot_order = LibGetVariableAndSize(VarBootOrder, &gEfiGlobalVariableGuid, &size);
  for (pos = boot_order; pos < boot_order + size; pos++) {
    boot_option(ImageHandle, L"Hello Chain loader", *pos);    
  }
  FreePool(boot_order);

  Input(L"Press any key to continue", NULL, 0);
 
  return EFI_SUCCESS;
}
