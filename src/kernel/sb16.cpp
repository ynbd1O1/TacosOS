#include "sb16.h"
#include "io.h"
#include <stdbool.h>

// Helper to wait for DSP
static bool sb16_dsp_write(uint8_t val) {
  int timeout = 100000;
  while ((inb(SB16_DSP_WRITE_STATUS) & 0x80) && timeout--)
    ;
  if (timeout <= 0)
    return false;
  outb(SB16_DSP_WRITE, val);
  return true;
}

static int sb16_dsp_read() {
  int timeout = 100000;
  while (!(inb(SB16_DSP_READ_STATUS) & 0x80) && timeout--)
    ;
  if (timeout <= 0)
    return -1;
  return inb(SB16_DSP_READ);
}

void dma_setup_channel5(void *buffer, uint32_t length);

bool sb16_init() {
  // Reset DSP
  outb(SB16_DSP_RESET, 1);
  // Wait ~3 microseconds
  for (volatile int i = 0; i < 1000; i++)
    ;
  outb(SB16_DSP_RESET, 0);

  // Read 0xAA (Reset Success)
  if (sb16_dsp_read() != 0xAA)
    return false;

  // Set Version 4 (SB16)
  if (!sb16_dsp_write(0xE1))
    return false; // Get Version
  if (sb16_dsp_read() == -1)
    return false; // major
  if (sb16_dsp_read() == -1)
    return false; // minor

  return true;
}

// 64KB static buffer in BSS (DMA safe < 16MB)
static int16_t sound_buffer[32768];

bool sb16_play_pcm(void *buffer, uint32_t length, uint16_t hz) {
  // 1. Setup DMA
  dma_setup_channel5(buffer, length);

  // 2. Configure SB16 for 16-bit PCM (DSP commands)
  if (!sb16_dsp_write(0x41))
    return false; // Set sampling rate
  if (!sb16_dsp_write(hz >> 8))
    return false;
  if (!sb16_dsp_write(hz & 0xFF))
    return false;

  if (!sb16_dsp_write(0xB6))
    return false; // 16-bit PCM Output
  if (!sb16_dsp_write(0x10))
    return false; // Mode: Mono, Signed

  // Sample count is in samples - 1
  uint16_t samples = length / 2 - 1;
  if (!sb16_dsp_write(samples & 0xFF))
    return false;
  if (!sb16_dsp_write(samples >> 8))
    return false;

  return true;
}

// Full melody frequencies for "It's Raining Tacos"
static int pcm_frequencies[] = {415, 466, 494, 415, 466, 740, 415, 466,
                                494, 554, 494, 466, 415, 466, 494, 415,
                                466, 494, 554, 494, 466, 415, 466, 494,
                                554, 622, 554, 494, 466, 415, 0};
static int pcm_durations[] = {2000, 2000, 2000, 2000, 2000, 4000, 2000, 2000,
                              2000, 2000, 2000, 4000, 2000, 2000, 4000, 2000,
                              2000, 2000, 2000, 2000, 4000, 2000, 2000, 2000,
                              2000, 2000, 2000, 2000, 2000, 4000, 0};

void sb16_play_tacos_melody() {
  int offset = 0;
  for (int n = 0; pcm_frequencies[n] != 0 && offset < 32000; n++) {
    int freq = pcm_frequencies[n];
    int dur = pcm_durations[n];

    for (int i = 0; i < dur && offset < 32768; i++) {
      int period = 8000 / freq;
      if (period == 0)
        period = 1;
      // Mixed wave for a "fuller" sound
      int16_t sample = ((i / (period / 2)) % 2) ? 6000 : -6000;
      // Add a bit of decay/envelope
      sample = (sample * (dur - i)) / dur;
      sound_buffer[offset++] = sample;
    }
    // Small gap
    for (int i = 0; i < 100 && offset < 32768; i++)
      sound_buffer[offset++] = 0;
  }
  sb16_play_pcm(sound_buffer, offset * 2, 8000);
}

void cmd_play_test() { sb16_play_tacos_melody(); }
