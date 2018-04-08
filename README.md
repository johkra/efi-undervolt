Proof of concept code for efi boot loader which undervolts or overclocks a CPU
before starting the OS.

**Current status**: Underclocking/Undervolting has been tested on an A10-5700 and
works. Changing settings requires recompilation.

**Compilation**: The code has been developed on an Arch Linux 64 bit system.
Install gnu-efi-libs and type make to compile.

**Installation**: Copy the `undervolt.efi` file to the EFI system partition and
add a boot entry from the UEFI firmware or using efibootmgr/EasyUEFI.

**Known issues**:

* The code does not support multipliers below 16. I don't change idle P-states,
  so I haven't bothered adding support. Please search for CoreCOF in the [BIOS
and Kernel Developer Guide][BKDG] if you want to add support.
* Only 15h (Trinity) APUs are supported. Check
  [AmdMsrTweaker](https://github.com/mpollice/AmdMsrTweaker) or the relevant
BKDG for required changes for other families.

[BKDG]: https://support.amd.com/TechDocs/42301_15h_Mod_00h-0Fh_BKDG.pdf
