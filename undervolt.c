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

#define	LOADER L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi"

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

UINT64 rdmsr(UINT32 reg) {
	UINT32 upper, lower;
	asm("rdmsr;": "=d" (upper), "=a" (lower): "c" (reg));

	return ((UINT64)upper << 32) + lower;
}

void wrmsr(UINT32 reg, UINT64 value) {
	UINT32 upper, lower;
	upper = value >> 32;
	lower = value & 0xffffffff;
	Print(L"Writing %016lx to %08x\n", value, reg);
	asm("wrmsr;": : "d" (upper), "a" (lower), "c" (reg));
}

void set_current_p_state(UINT8 num_p_state) {
	UINT32 reg = 0xc0010062;
	UINT64 value = rdmsr(reg);

	value = (value & 0xFFFFFFFFFFFFFFF8) + (num_p_state & 0x7);

	wrmsr(reg, value);
}

void set_p_state(UINT8 num_p_state, UINTN fid, UINT8 did, UINT8 vid) {
	UINT32 reg = 0xc0010064 + num_p_state;
	UINT64 value = rdmsr(reg);

	value = (value & 0xFFFFFFFFFFFFFFC0) + (fid & 0x3F);
	value = (value & 0xFFFFFFFFFFFFFE3F) + ((did & 0x7) << 6);
	value = (value & 0xFFFFFFFFFFFE01FF) + ((vid & 0xFF) << 9);

	wrmsr(reg, value);
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	EFI_HANDLE image;
	EFI_STATUS err;

	EFI_DEVICE_PATH *path;
	EFI_LOADED_IMAGE *loaded_image;

	InitializeLib(ImageHandle, SystemTable);

	Input(L"Press any key to change current p-state", NULL, 0);

	set_current_p_state(4);

/*
	Input(L"Press any key to change P0", NULL, 0);
	set_p_state(0, 18, 0, 60);
	Input(L"Press any key to change P1", NULL, 0);
	set_p_state(1, 14, 0, 78);
*/
	Input(L"Press any key to change P2", NULL, 0);
	set_p_state(2, 12, 0, 84);
	Input(L"Press any key to change P3", NULL, 0);
	set_p_state(3, 12, 0, 84);

	Input(L"Press any key to change current p-state", NULL, 0);
	set_current_p_state(1);
	Input(L"Press any key to change current p-state", NULL, 0);
	set_current_p_state(0);

        err = uefi_call_wrapper(BS->OpenProtocol, 6, ImageHandle, &LoadedImageProtocol, (void **)&loaded_image,
                                ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(err)) {
                Print(L"Error getting a LoadedImageProtocol handle: %r ", err);
		Sleep(3);
                return err;
        }

	path = FileDevicePath(loaded_image->DeviceHandle, LOADER);

        err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, ImageHandle, path , NULL, 0, &image);
        if (EFI_ERROR(err)) {
                Print(L"Error loading image: %r\n", err);
		Sleep(3);
                return err;
        }

	FreePool(loaded_image);
	FreePool(path);

	Input(L"Press any key to continue", NULL, 0);

	err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
	if (EFI_ERROR(err)) {
		Print(L"Error starting selected image: %r\n", err);
	}

	Input(L"Press any key to continue", NULL, 0);
 
	return EFI_SUCCESS;
}
