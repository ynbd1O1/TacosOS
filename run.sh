# Run TacosOS in QEMU with Sound Support enabled
# NOTE: If this fails with "command not found", replace 'qemu-system-x86_64' with the full path to your installation.
# Example: "/c/Program Files/qemu/qemu-system-x86_64.exe"

QEMU_CMD="qemu-system-x86_64"

# Try to find common paths if not in PATH
if ! command -v $QEMU_CMD &> /dev/null; then
    if [ -f "/c/Program Files/qemu/qemu-system-x86_64.exe" ]; then
        QEMU_CMD="/c/Program Files/qemu/qemu-system-x86_64.exe"
    elif [ -f "/c/Program Files (x86)/qemu/qemu-system-x86_64.exe" ]; then
        QEMU_CMD="/c/Program Files (x86)/qemu/qemu-system-x86_64.exe"
    fi
fi

# Create hard disk image if it doesn't exist
if [ ! -f "disk.img" ]; then
    echo "Creating 10MB Disk Image..."
    qemu-img create -f raw disk.img 10M
fi

$QEMU_CMD -hda disk.img -cdrom build/tacos_os.iso -boot d -m 128M -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -device sb16,audiodev=snd0
