#!/bin/bash
set -e

echo "Building Minimal TacosOS..."

mkdir -p build

# Compiler flags
CFLAGS="-ffreestanding -O2 -Wall -Wextra -m64 -fno-stack-protector -fno-exceptions -fno-rtti -mno-red-zone -mcmodel=kernel -fno-pic -mno-sse -mno-mmx -mno-sse2"
INCLUDES="-I src"

# Assemble boot code
echo "Assembling boot code..."
nasm -f elf64 src/arch/x86_64/multiboot_header.asm -o build/multiboot_header.o
nasm -f elf64 src/arch/x86_64/boot.asm -o build/boot.o

# Compile kernel sources
echo "Compiling kernel..."
gcc -c src/kernel/main.cpp -o build/main.o $CFLAGS $INCLUDES
gcc -c src/kernel/interrupts.cpp -o build/interrupts.o $CFLAGS $INCLUDES
gcc -c src/kernel/dma.cpp -o build/dma.o $CFLAGS $INCLUDES
gcc -c src/kernel/sb16.cpp -o build/sb16.o $CFLAGS $INCLUDES

# Link
echo "Linking..."
ld -n -o build/tacos_os.bin -T linker.ld \
    build/multiboot_header.o \
    build/boot.o \
    build/main.o \
    build/interrupts.o \
    build/dma.o \
    build/sb16.o \
    -z max-page-size=0x1000

# Generate ISO
echo "Generating ISO..."
mkdir -p build/isofiles/boot/grub
cp build/tacos_os.bin build/isofiles/boot/kernel.bin

cat > build/isofiles/boot/grub/grub.cfg << EOF
set timeout=0
set default=0

menuentry "TacosOS" {
    multiboot2 /boot/kernel.bin
    boot
}
EOF

grub-mkrescue -o build/tacos_os.iso build/isofiles 2>/dev/null

echo ""
echo "======================================"
echo "Build successful!"
echo "======================================"
echo "Output: build/tacos_os.bin"
echo "ISO: build/tacos_os.iso"
echo ""
echo "Run with QEMU:"
echo "  qemu-system-x86_64 -cdrom build/tacos_os.iso -m 128M -soundhw pcspk"
echo ""
