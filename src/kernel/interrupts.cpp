#include "idt.h"
#include "io.h"
#include <stdint.h>

static idt_entry_t idt[256];
static idtr_t idtr;

// Primitive handler template
extern "C" void irq_common_stub();

void idt_set_gate(uint8_t n, void *handler, uint8_t flags) {
  uint64_t addr = (uint64_t)handler;
  idt[n].offset_low = addr & 0xFFFF;
  idt[n].selector = 0x08; // Kernel Code Segment
  idt[n].ist = 0;
  idt[n].flags = flags;
  idt[n].offset_mid = (addr >> 16) & 0xFFFF;
  idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
  idt[n].reserved = 0;
}

void pic_remap() {
  // ICW1: Start initialization
  outb(0x20, 0x11);
  outb(0xA0, 0x11);

  // ICW2: Set vector offsets
  outb(0x21, 0x20); // Master: 0x20 - 0x27
  outb(0xA1, 0x28); // Slave: 0x28 - 0x2F

  // ICW3: Cascade
  outb(0x21, 0x04);
  outb(0xA1, 0x02);

  // ICW4: 8086 mode
  outb(0x21, 0x01);
  outb(0xA1, 0x01);

  // Mask all interrupts except for testing later
  outb(0x21, 0xFF);
  outb(0xA1, 0xFF);
}

// Stub for assembly to call back into C++
extern "C" void irq_handler(uint64_t irq) {
  // SB16 uses IRQ 5 by default
  if (irq == 5) {
    // SB16 Interrupt Logic will go here
  }

  // Send EOI
  if (irq >= 8)
    outb(0xA0, 0x20);
  outb(0x20, 0x20);
}

void idt_init() {
  idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;
  idtr.base = (uint64_t)&idt;

  for (int i = 0; i < 256; i++) {
    // Just fill with nulls initially, or a dummy holder
  }

  pic_remap();

  asm volatile("lidt %0" : : "m"(idtr));
  // sti will be called later when we are ready
}
