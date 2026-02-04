#include "io.h"
#include <stdint.h>


// DMA 8237 Channels for SB16 (usually 16-bit Channel 5)
// Registers for Page, Address, and Count
void dma_setup_channel5(void *buffer, uint32_t length) {
  uint32_t addr = (uint32_t)(uint64_t)buffer;
  uint16_t count = (uint16_t)(length / 2 - 1); // 16-bit DMA count is in words
  uint8_t page = (addr >> 16) & 0xFF;

  // Mask channel 5
  outb(0xD4, 0x01 | 0x04);

  // Clear Flip-Flop
  outb(0xD8, 0x00);

  // Mode: Single, Inc, No Auto, Write (Play), Channel 5 (0x49)
  outb(0xD6, 0x49);

  // Offset (Address)
  outb(0xC4, (uint8_t)(addr >> 1)); // 16-bit DMA address is shifted
  outb(0xC4, (uint8_t)(addr >> 9));

  // Page
  outb(0x8B, page);

  // Count
  outb(0xC6, (uint8_t)count);
  outb(0xC6, (uint8_t)(count >> 8));

  // Unmask channel 5
  outb(0xD4, 0x01);
}
