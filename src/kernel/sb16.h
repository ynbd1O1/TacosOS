#pragma once
#include <stdint.h>

#define SB16_BASE 0x220
#define SB16_DSP_RESET (SB16_BASE + 0x6)
#define SB16_DSP_READ (SB16_BASE + 0xA)
#define SB16_DSP_WRITE (SB16_BASE + 0xC)
#define SB16_DSP_WRITE_STATUS (SB16_BASE + 0xC)
#define SB16_DSP_READ_STATUS (SB16_BASE + 0xE)
#define SB16_DSP_INT16_ACK (SB16_BASE + 0xF)

bool sb16_init();
bool sb16_play_pcm(void *buffer, uint32_t length, uint16_t hz);
