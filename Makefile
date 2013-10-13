all: compile

compile:
	gcc -I. -I/usr/include/efi -I/usr/include/efi/x86_64 -Wall -Wextra -Os -fpic -fshort-wchar -nostdinc -ffreestanding -fno-strict-aliasing -fno-stack-protector -Wsign-compare -mno-sse -mno-mmx -mno-red-zone -DEFI_FUNCTION_WRAPPER -DGNU_EFI_USE_MS_ABI -c undervolt.c -o undervolt.o
	ld -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -nostdlib -znocombreloc -L /usr/lib /usr/lib/crt0-efi-x86_64.o undervolt.o \
	-o undervolt.so -lefi -lgnuefi /usr/lib/gcc/x86_64-unknown-linux-gnu/4.8.1/libgcc.a; \
nm -D -u undervolt.so | grep ' U ' && exit 1 || :
	objcopy -j .text -j .sdata -j .data -j .dynamic \
  -j .dynsym -j .rel -j .rela -j .reloc -j .eh_frame \
  --target=efi-app-x86_64 undervolt.so undervolt.efi

clean:
	rm -fr undervolt.o undervolt.so undervolt.efi
