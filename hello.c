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

void cpuid(UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx) {
        asm("cpuid;" :"+a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx));
}

void rdmsr(UINT32 reg, UINT32 *upper, UINT32 *lower) {
	asm("rdmsr;": "=d" (*upper), "=a" (*lower): "c" (reg));
}

void wrmsr(UINT32 reg, UINT32 upper, UINT32 lower) {
	asm("wrmsr;": : "d" (upper), "a" (lower), "c" (reg));
}

void itox(UINTN n, CHAR8* buffer, UINT8 size) {
    UINT8 value;
    UINT8 i = size * 2;

    while(i--) {
        value = (n >> 4*i) & 0xf;
        if (value > 9) {
            value += 'A' - '0' - 10;
        }
        buffer[size*2-i-1] = '0'+value;
    }
    
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
  struct pvclock_wall_clock* clockaddr = &clock;
  CHAR8 buffer[11];

  wrmsr(0x4b564d00, (UINT32)((UINT64)clockaddr>>32 & 0xffffffff), (UINT32)((UINT64)clockaddr & 0xffffffff));
  itox(clock.sec, buffer, sizeof(clock.sec));
  CopyMem(buffer+8, "\r\n\0", 3);
  APrint(buffer);
}

void print_vendor() {
  UINT32 eax, ebx, ecx, edx;
  CHAR8 vendor[15];

  eax = 0;
  cpuid(&eax, &ebx, &ecx, &edx);
  
  ints_to_string(vendor, &ebx, &edx, &ecx, NULL);
  CopyMem(vendor+12, "\r\n\0", 3);
  APrint(vendor);
}

UINTN strlen(CHAR8* str) {
  CHAR8* pos = str;

  while(*pos != '\0') {
    pos++;
  }
  return pos-str;
}

void print_extended_vendor() {
  UINT32 eax, ebx, ecx, edx;
  CHAR8 vendor[50];
  int offset = 0;
  

  eax = 0x80000002;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor, &eax, &ebx, &ecx, &edx);
  eax = 0x80000003;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor+16, &eax, &ebx, &ecx, &edx);
  eax = 0x80000004;
  cpuid(&eax, &ebx, &ecx, &edx);
  ints_to_string(vendor+32, &eax, &ebx, &ecx, &edx);
  offset = strlen(vendor);
  CopyMem(vendor+offset, "\r\n\0", 3);
  APrint(vendor);

}

void boot_option(EFI_HANDLE ImageHandle, CHAR16* skipOption, UINT16 code) {
	UINTN var_size, str_size;
	void* boot;
	CHAR16 VarBoot[16] = { 0 };
	CHAR16* Description;
	EFI_HANDLE image;
	EFI_STATUS err;

	EFI_DEVICE_PATH* path;

	SPrint(VarBoot, sizeof(VarBoot), L"Boot%04x", code);
  	boot = LibGetVariableAndSize(VarBoot, &gEfiGlobalVariableGuid, &var_size);

	str_size = StrSize(boot + sizeof(UINT32) + sizeof(UINT16));
	Description = AllocatePool(str_size);
	StrCpy(Description, boot + sizeof(UINT32) + sizeof(UINT16));

	if (StrCmp(skipOption, Description) == 0) {
		Print(L"Skipping Hello Chain loader.\n");
		FreePool(Description);
		FreePool(boot);
		return;
	}

	path = boot + sizeof(UINT32) + sizeof(UINT16) + str_size;

	Print(L"Description: %s\n", Description);
  	Input(L"Press any key to boot this option.", NULL, 0);
	err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, ImageHandle, path , NULL, 0, &image);

	FreePool(Description);

	if (EFI_ERROR(err)) {
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		Print(L"Error loading %s: %r", Description, err);
		FreePool(boot);
		return;
	}
	
	err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
	if (EFI_ERROR(err)) {
		FreePool(boot);
		Print(L"Error starting %s: %r", Description, err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
	}

	FreePool(boot);
}


EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  UINT16* boot_order;
  UINTN size, i;

  InitializeLib(ImageHandle, SystemTable);

  print_time();
  print_vendor();
  print_extended_vendor();

  boot_order = LibGetVariableAndSize(VarBootOrder, &gEfiGlobalVariableGuid, &size);
  for (i = 0; i < size/sizeof(UINT16); i++) {
    boot_option(ImageHandle, L"Hello Chain loader", *(boot_order+i));    
  }
  FreePool(boot_order);

  Input(L"Press any key to continue", NULL, 0);
 
  return EFI_SUCCESS;
}
