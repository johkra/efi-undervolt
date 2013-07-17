all: compile

compile:
	gcc -I. -I/usr/include/efi -I/usr/include/efi/x86_64 -Wall -Wextra -Os -fpic -fshort-wchar -nostdinc -ffreestanding -fno-strict-aliasing -fno-stack-protector -Wsign-compare -mno-sse -mno-mmx -mno-red-zone -DEFI_FUNCTION_WRAPPER -DGNU_EFI_USE_MS_ABI -c hello.c -o hello.o
	ld -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -nostdlib -znocombreloc -L /usr/lib /usr/lib/crt0-efi-x86_64.o hello.o \
	-o hello.so -lefi -lgnuefi /usr/lib/gcc/x86_64-unknown-linux-gnu/4.8.1/libgcc.a; \
nm -D -u hello.so | grep ' U ' && exit 1 || :
	objcopy -j .text -j .sdata -j .data -j .dynamic \
  -j .dynsym -j .rel -j .rela -j .reloc -j .eh_frame \
  --target=efi-app-x86_64 hello.so hello.efi

deploy:
	su -c "/home/johkra/efi/copy-file.sh"

clean:
	rm -fr hello.o hello.so hello.efi

qemu:
	qemu-system-x86_64 -enable-kvm -net none -m 1024 -bios /usr/share/ovmf/x86_64/bios.bin -hda /home/johkra/win8efi.img
