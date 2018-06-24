# Zynq mkbootimage

(c) 2015-2018 [Antmicro](https://antmicro.com)

This is an open-source replacement for the Xilinx `bootgen` application.
It parses a `.bif` file and creates a Zynq boot image in the `.bin` format.

It is written entirely in C.

Required C libraries: `pcre`, `libelf`.

To build this application run:
```
make
```

To use it, type in:
```
./mkbootimage [--zynqmp|-u] <input_bif_file> <output_bin_file>
```

### Zynq-7000

For Zynq-7000 series, `zynq-mkbootimage` currently supports creating boot images
containing the FSBL, bitstream, U-Boot, and Linux-related binary files.

For loading Linux-related images, both the `[load]` and the `[offset]` attributes
are supported.
As opposed to the original `bootgen` utility, file extensions are not required.

For example the following `.bif` file:
```
the_ROM_image:
{
  [bootloader]fsbl.elf
  fpga.bit
  u-boot.elf
  [load=0x2a00000]devicetree.dtb
  [load=0x2000000]uramdisk
  [load=0x3000000]uImage
}
```

used with the following command:
```
./mkbootimage boot.bif boot.bin
```

will generate a `.bin` image which can be used in U-Boot, as follows:
```
bootm 0x3000000 0x2000000 0x2a00000
```

### Zynq UltraScale+

For Zynq UltraScale+, `zynq-mkbootimage` currently supports creating boot images
containing the FSBL, bitstream, U-Boot, ARM trusted software and Linux-related binary images.

For example the following `.bif` file:
```
the_ROM_image:
{
  [fsbl_config] a53_x64
  [bootloader] fsbl.elf
  [destination_device=pl] fpga.bit
  [, destination_cpu=a53-0, exception_level=el-2] bl31.elf
  [, destination_cpu=a53-0, exception_level=el-2] u-boot.elf
  [load=0x2a00000]devicetree.dtb
  [load=0x2000000]uramdisk
  [load=0x3000000]uImage
}
```

used with the following command:
```
./mkbootimage --zynqmp boot.bif boot.bin
```

will generate a `.bin` image, which can be used to successfully boot a Zynq
UltraScale+ machine, and to boot Linux using the following U-Boot command:
```
bootm 0x3000000 0x2000000 0x2a00000
```

Encryption certificates are not supported.
