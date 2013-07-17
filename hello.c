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

void read_boot_option(UINT16 code) {
	HARDDRIVE_DEVICE_PATH hd_dv_path;
	FILEPATH_DEVICE_PATH* fp_dv_path = AllocateZeroPool(SIZE_OF_FILEPATH_DEVICE_PATH + sizeof(EFI_DEVICE_PATH));
	EFI_DEVICE_PATH end_path;
	UINTN var_size, str_size, opt_size;
	UINT16 FilePathListLength;
	void* boot;
	void* pos;
	CHAR16 VarBoot[16] = { 0 };
	CHAR16* Description;
	UINT8* OptionalData;

	SPrint(VarBoot, sizeof(VarBoot), L"Boot%04x", code);
  	boot = LibGetVariableAndSize(VarBoot, &gEfiGlobalVariableGuid, &var_size);

	pos = boot;
	pos += sizeof(UINT32); // Skip Attributes	
	CopyMem(&FilePathListLength, pos, sizeof(UINT16));
	pos += sizeof(UINT16);

	str_size = StrSize(pos);
	Description = AllocatePool(str_size);
	StrCpy(Description, pos);
	pos += str_size;

	/* Skip ACPI/PCI/ATAPI device path elements. This is needed for elements created with TianoCore */
	if (*(UINT8*)pos == 0x02 && *(UINT8*)(pos+1) == 0x01) {
		pos += 12;
	}	
	if (*(UINT8*)pos == 0x01 && *(UINT8*)(pos+1) == 0x01) {
		pos += 6;
	}	
	if (*(UINT8*)pos == 0x03 && *(UINT8*)(pos+1) == 0x01) {
		pos += 8;
	}	

	CopyMem(&hd_dv_path, pos, 42);
	pos += 42;

	if (hd_dv_path.Header.Type != 0x04 || hd_dv_path.Header.SubType != 0x01) {
		FreePool(Description);
		FreePool(fp_dv_path);
		return;
	}

	CopyMem(&fp_dv_path->Header, pos, 4);
	pos += 4;

	if (fp_dv_path->Header.Type != 0x04 || fp_dv_path->Header.SubType != 0x04) {
		Print(L"Invalid file path header: %x %x\n", fp_dv_path->Header.Type, fp_dv_path->Header.SubType);
		FreePool(Description);
		FreePool(fp_dv_path);
		return;
	}
	
	fp_dv_path = ReallocatePool(fp_dv_path, SIZE_OF_FILEPATH_DEVICE_PATH + sizeof(EFI_DEVICE_PATH), (UINT16)*fp_dv_path->Header.Length);
	StrCpy((CHAR16*)&fp_dv_path->PathName, pos);
	pos += (UINT16)*fp_dv_path->Header.Length - 4;

	CopyMem(&end_path.Type, pos, sizeof(UINT8));
	pos += sizeof(UINT8);
	CopyMem(&end_path.SubType, pos, sizeof(UINT8));
	pos += sizeof(UINT8);
	CopyMem(&end_path.Length, pos, 2*sizeof(UINT8));
	pos += 2*sizeof(UINT8);

	if (end_path.Type != 0x7f || end_path.SubType != 0xff) {
		Print(L"Expected end device path header.\n");
		FreePool(Description);
		FreePool(fp_dv_path);
		return;
	}

	opt_size = var_size - sizeof(UINT32) - sizeof(UINT16) - StrSize(Description) - FilePathListLength;

	OptionalData = AllocatePool(opt_size);
	CopyMem(OptionalData, pos, opt_size);

	Print(L"%s\n", Description);
	Print(L"%s\n", fp_dv_path->PathName);
	DumpHex(0, 0, opt_size, OptionalData);

	FreePool(Description);
	FreePool(fp_dv_path);
	FreePool(OptionalData);
}


EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  UINT16* boot_order;
  
  CHAR8 buffer[7];
  UINTN size, i;


  InitializeLib(ImageHandle, SystemTable);

  print_time();
  print_vendor();
  print_extended_vendor();

  boot_order = LibGetVariableAndSize(VarBootOrder, &gEfiGlobalVariableGuid, &size);
  CopyMem(buffer+4, "\r\n\0", 3);
  for (i = 0; i < size/sizeof(UINT16); i++) {
    read_boot_option(*(boot_order+i));    
  }
  FreePool(boot_order);

  Input(L"Press any key to continue", NULL, 0);
 
  return EFI_SUCCESS;
}
