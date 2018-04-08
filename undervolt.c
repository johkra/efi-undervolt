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

UINT64 rdmsr(UINT32 reg) {
	UINT32 upper, lower;
	asm("rdmsr;": "=d" (upper), "=a" (lower): "c" (reg));

	return ((UINT64)upper << 32) + lower;
}

void wrmsr(UINT32 reg, UINT64 value) {
	UINT32 upper, lower;
	upper = value >> 32;
	lower = value & 0xffffffff;
	asm("wrmsr;": : "d" (upper), "a" (lower), "c" (reg));
}

void set_current_p_state(UINT8 num_p_state) {
	UINT32 reg = 0xc0010062;
	UINT64 value = rdmsr(reg);

	value = (value & 0xFFFFFFFFFFFFFFF8) + (num_p_state & 0x7);

	wrmsr(reg, value);
}

void configure_p_state(UINT8 num_p_state, UINT8 multi, double vcore) {
	UINT32 reg = 0xc0010064 + num_p_state;
	UINT64 value = rdmsr(reg);

	// See CoreCOF in BKDG for details
	UINTN fid = multi - 0x10;
	// Hardcode divisor to 0; this requires multiplicator values > 16
	UINT8 did = 0;
	UINT8 vid = (1.55-vcore)/0.00625;

	value = (value & 0xFFFFFFFFFFFFFFC0) + (fid & 0x3F);
	value = (value & 0xFFFFFFFFFFFFFE3F) + ((did & 0x7) << 6);
	value = (value & 0xFFFFFFFFFFFE01FF) + ((vid & 0xFF) << 9);

	wrmsr(reg, value);
}

void disable_boost() {
	UINT32 reg = 0xc0010015;
	UINT64 value = rdmsr(reg);

	value |= 1 << 25;

	wrmsr(reg, value);
}

void enable_boost() {
	UINT32 reg = 0xc0010015;
	UINT64 value = rdmsr(reg);

	value &= ~(1 << 25);

	wrmsr(reg, value);
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	EFI_HANDLE image;
	EFI_STATUS err;

	EFI_DEVICE_PATH *path;
	EFI_LOADED_IMAGE *loaded_image;

	InitializeLib(ImageHandle, SystemTable);

	set_current_p_state(7);
	disable_boost();

	configure_p_state(0, 30, 1.0625);
	configure_p_state(1, 30, 1.0625);
	configure_p_state(2, 30, 1.0625);
	configure_p_state(3, 28, 1.025);
	configure_p_state(4, 26, 1.0);

	enable_boost();

        err = uefi_call_wrapper(BS->OpenProtocol, 6, ImageHandle, &LoadedImageProtocol, (void **)&loaded_image,
                                ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(err)) {
                Print(L"Error getting a LoadedImageProtocol handle: %r ", err);
                return err;
        }

	path = FileDevicePath(loaded_image->DeviceHandle, LOADER);

        err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, ImageHandle, path , NULL, 0, &image);
        if (EFI_ERROR(err)) {
                Print(L"Error loading image: %r\n", err);
                return err;
        }

	FreePool(loaded_image);
	FreePool(path);

	err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
	if (EFI_ERROR(err)) {
		Print(L"Error starting selected image: %r\n", err);
	}
 
	return EFI_SUCCESS;
}
