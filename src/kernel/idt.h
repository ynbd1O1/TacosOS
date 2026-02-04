#pragma once
#include <stdint.h>

struct idt_entry_t {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t flags;
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed));

struct idtr_t {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

void idt_init();
void idt_set_gate(uint8_t n, void *handler, uint8_t flags);
