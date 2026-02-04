#include "idt.h"
#include "io.h"
#include "sb16.h"
#include <stdbool.h>
#include <stdint.h>

// --- VGA Text Mode Constants & State ---
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((volatile uint16_t *)0xB8000)
#define COLOR_DEFAULT 0x07 // Light Gray on Black
#define COLOR_PROMPT 0x0B  // Cyan on Black
#define COLOR_LOGO 0x0E    // Yellow on Black
#define COLOR_SUCCESS 0x0A // Light Green on Black
#define COLOR_ERROR 0x0C   // Light Red on Black

static int cursor_x = 0;
static int cursor_y = 0;
bool sb16_active = false;

// --- VGA Functions ---
void update_cursor() {
  uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void scroll() {
  if (cursor_y >= VGA_HEIGHT) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
      for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
      }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
      VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
          (uint16_t)' ' | (COLOR_DEFAULT << 8);
    }
    cursor_y = VGA_HEIGHT - 1;
  }
}

void term_putc(char c, uint8_t color = COLOR_DEFAULT) {
  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\r') {
    cursor_x = 0;
  } else if (c == '\b') {
    if (cursor_x > 0) {
      cursor_x--;
      VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] =
          (uint16_t)' ' | (color << 8);
    }
  } else {
    VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)c | (color << 8);
    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
      cursor_x = 0;
      cursor_y++;
    }
  }
  scroll();
  update_cursor();
}

void term_puts(const char *s, uint8_t color = COLOR_DEFAULT) {
  for (int i = 0; s[i] != '\0'; i++)
    term_putc(s[i], color);
}

void clear_screen() {
  for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    VGA_BUFFER[i] = (uint16_t)' ' | (COLOR_DEFAULT << 8);
  }
  cursor_x = 0;
  cursor_y = 0;
  update_cursor();
}

// --- Keyboard Handling (Polling) ---
uint8_t kbd_scancode() {
  while (!(inb(0x64) & 1))
    ; // Wait for data
  return inb(0x60);
}

char scancode_to_ascii(uint8_t scancode) {
  if (scancode & 0x80)
    return 0; // Ignore release codes
  static const char kbd_map[] = {
      0,   27,  '1',  '2',  '3',  '4', '5', '6',  '7', '8', '9', '0',
      '-', '=', '\b', '\t', 'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
      'o', 'p', '[',  ']',  '\n', 0,   'a', 's',  'd', 'f', 'g', 'h',
      'j', 'k', 'l',  ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
      'b', 'n', 'm',  ',',  '.',  '/', 0,   '*',  0,   ' '};
  if (scancode < sizeof(kbd_map))
    return kbd_map[scancode];
  return 0;
}

// --- Mock Filesystem State ---
#define MAX_FILES 16
#define MAX_DIRS 8
struct MockFile {
  char name[32];
  char parent_dir[32];
  char content[128];
};
static MockFile file_system[MAX_FILES];
static int file_count = 0;
static char current_dir[32] = "/";

static char valid_dirs[MAX_DIRS][32];
static int dir_count = 0;

// --- String Helpers ---
int kstrlen(const char *s) {
  int i = 0;
  while (s[i])
    i++;
  return i;
}

int kstrcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int kstrncmp(const char *s1, const char *s2, int n) {
  while (n > 0 && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  if (*s1 == '\0' && n > 0)
    return 0; // Fixed: technically kstrncmp doesn't care if s1 is shorter if
              // match so far
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void kstrcpy(char *dest, const char *src) {
  while ((*dest++ = *src++))
    ;
}

void kstrcat(char *dest, const char *src) {
  int i = kstrlen(dest);
  int j = 0;
  while (src[j])
    dest[i++] = src[j++];
  dest[i] = '\0';
}

// --- ATA PIO Driver (Basic) ---
#define ATA_PRIMARY_DATA 0x1F0
#define ATA_PRIMARY_ERR 0x1F1
#define ATA_PRIMARY_SECCOUNT 0x1F2
#define ATA_PRIMARY_LBA_LO 0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HI 0x1F5
#define ATA_PRIMARY_DRIVE_SEL 0x1F6
#define ATA_PRIMARY_COMMAND 0x1F7
#define ATA_PRIMARY_STATUS 0x1F7

// --- ATA PIO Driver (Basic) ---
#define ATA_PRIMARY_DATA 0x1F0
#define ATA_PRIMARY_ERR 0x1F1
#define ATA_PRIMARY_SECCOUNT 0x1F2
#define ATA_PRIMARY_LBA_LO 0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HI 0x1F5
#define ATA_PRIMARY_DRIVE_SEL 0x1F6
#define ATA_PRIMARY_COMMAND 0x1F7
#define ATA_PRIMARY_STATUS 0x1F7

// Returns true if successful, false on timeout
bool ata_wait_bsy() {
  int timeout = 100000;
  while (inb(ATA_PRIMARY_STATUS) & 0x80) {
    if (--timeout == 0)
      return false;
  }
  return true;
}

// Returns true if successful, false on timeout
bool ata_wait_drq() {
  int timeout = 100000;
  while (!(inb(ATA_PRIMARY_STATUS) & 0x08)) {
    if (--timeout == 0)
      return false;
  }
  return true;
}

bool ata_read_sector(uint32_t lba, uint16_t *buffer) {
  outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_PRIMARY_SECCOUNT, 1);
  outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
  outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
  outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
  outb(ATA_PRIMARY_COMMAND, 0x20); // Read sectors

  if (!ata_wait_bsy())
    return false;
  if (!ata_wait_drq())
    return false;

  for (int i = 0; i < 256; i++) {
    buffer[i] = inw(ATA_PRIMARY_DATA);
  }
  return true;
}

bool ata_write_sector(uint32_t lba, uint16_t *buffer) {
  outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_PRIMARY_SECCOUNT, 1);
  outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
  outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
  outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
  outb(ATA_PRIMARY_COMMAND, 0x30); // Write sectors

  if (!ata_wait_bsy())
    return false;
  if (!ata_wait_drq())
    return false;

  for (int i = 0; i < 256; i++) {
    outw(ATA_PRIMARY_DATA, buffer[i]);
  }
  // Flush
  if (!ata_wait_bsy())
    return false;
  return true;
}

// --- RTC (Real-Time Clock) Driver ---
#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

uint8_t get_rtc_register(int reg) {
  outb(CMOS_ADDRESS, reg);
  return inb(CMOS_DATA);
}

// Check if RTC "Update In Progress" flag is set
int get_update_in_progress_flag() {
  outb(CMOS_ADDRESS, 0x0A);
  return (inb(CMOS_DATA) & 0x80);
}

// Convert Binary Coded Decimal to Binary
uint8_t bcd2bin(uint8_t bcd) { return ((bcd / 16) * 10) + (bcd & 0x0F); }

struct DateTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint16_t year;
};

// Global startup time for `uptime`
static DateTime boot_time;

void read_rtc(DateTime *dt) {
  uint8_t status_b;

  // Wait until RTC is not updating
  while (get_update_in_progress_flag())
    ;

  dt->second = get_rtc_register(0x00);
  dt->minute = get_rtc_register(0x02);
  dt->hour = get_rtc_register(0x04);
  dt->day = get_rtc_register(0x07);
  dt->month = get_rtc_register(0x08);
  dt->year = get_rtc_register(0x09);

  // Check if we need BCD conversion
  outb(CMOS_ADDRESS, 0x0B);
  status_b = inb(CMOS_DATA);

  if (!(status_b & 0x04)) {
    dt->second = bcd2bin(dt->second);
    dt->minute = bcd2bin(dt->minute);
    dt->hour = bcd2bin(dt->hour);
    dt->day = bcd2bin(dt->day);
    dt->month = bcd2bin(dt->month);
    dt->year = bcd2bin(dt->year);
  }

  // Adjust year (RTC year is last 2 digits)
  dt->year += 2000;
}

// --- Utils ---
static unsigned long int next = 1;
int rand() {
  next = next * 1103515245 + 12345;
  return (unsigned int)(next / 65536) % 32768;
}

// Simple sleep loop (CPU speed dependent)
void sleep(int count) {
  for (volatile int i = 0; i < count; i++)
    ;
}

// --- Filesystem Persistence ---
#define FS_MAGIC "TACOSFS"
#define FS_SECTOR_START 0

void fs_save() {
  uint16_t sector[256];
  // Clear sector
  for (int i = 0; i < 256; i++)
    sector[i] = 0;

  // Header: Magic(7) + file_count(1) + dir_count(1)
  char *hdr = (char *)sector;
  kstrcpy(hdr, FS_MAGIC);
  hdr[8] = (char)file_count;
  hdr[9] = (char)dir_count;

  if (!ata_write_sector(FS_SECTOR_START, sector))
    return;

  // Write directories (Fixed size slots for simplicity)
  // Each directory is 32 bytes. Slot size 32. 16 slots per sector.
  for (int i = 0; i < dir_count; i++) {
    int sec_off = 1 + (i / 16);
    int slot = i % 16;
    if (!ata_read_sector(FS_SECTOR_START + sec_off, sector))
      continue;
    kstrcpy((char *)sector + (slot * 32), valid_dirs[i]);
    ata_write_sector(FS_SECTOR_START + sec_off, sector);
  }

  // Write files
  // Each MockFile is 32(name) + 32(parent) + 128(content) = 192 bytes.
  // 2 files per sector (512 bytes).
  for (int i = 0; i < file_count; i++) {
    int sec_off =
        5 + (i / 2); // Directories take ~4 sectors (MAX_DIRS is small)
    int slot = i % 2;
    if (!ata_read_sector(FS_SECTOR_START + sec_off, sector))
      continue;
    MockFile *dest = (MockFile *)((char *)sector + (slot * 256));
    *dest = file_system[i];
    ata_write_sector(FS_SECTOR_START + sec_off, sector);
  }
}

bool fs_load() {
  uint16_t sector[256];
  if (!ata_read_sector(FS_SECTOR_START, sector))
    return false;
  char *hdr = (char *)sector;

  if (kstrcmp(hdr, FS_MAGIC) != 0) {
    // Not a tacos disk, initialize defaults
    kstrcpy(valid_dirs[dir_count++], "/");
    kstrcpy(valid_dirs[dir_count++], "/home");
    kstrcpy(valid_dirs[dir_count++], "/system");
    kstrcpy(valid_dirs[dir_count++], "/tacos");
    kstrcpy(valid_dirs[dir_count++], "/dev");
    fs_save();
    return true;
  }

  file_count = (unsigned char)hdr[8];
  dir_count = (unsigned char)hdr[9];

  // Read directories
  for (int i = 0; i < dir_count; i++) {
    int sec_off = 1 + (i / 16);
    int slot = i % 16;
    if (ata_read_sector(FS_SECTOR_START + sec_off, sector)) {
      kstrcpy(valid_dirs[i], (char *)sector + (slot * 32));
    }
  }

  // Read files
  for (int i = 0; i < file_count; i++) {
    int sec_off = 5 + (i / 2);
    int slot = i % 2;
    if (ata_read_sector(FS_SECTOR_START + sec_off, sector)) {
      MockFile *src = (MockFile *)((char *)sector + (slot * 256));
      file_system[i] = *src;
    }
  }
  return true;
}

bool fs_init() { return fs_load(); }

// --- Commands ---
void cmd_logo() {
  term_puts("\n", COLOR_LOGO);
  term_puts("      _      \n", COLOR_LOGO);
  term_puts("   --/ \\--   \n", COLOR_LOGO);
  term_puts("  / Tacos \\  \n", COLOR_LOGO);
  term_puts(" |    OS   | \n", COLOR_LOGO);
  term_puts("  \\_______/  \n", COLOR_LOGO);
  term_puts("   \\_____/   \n", COLOR_LOGO);
  term_puts("\n", COLOR_LOGO);
}

// Forward declarations
void play_sound(uint32_t nFrequence);
void nosound();

void cmd_clear() { clear_screen(); }

void cmd_beep() {
  term_puts("Beep! (1 Second Test)...\n", COLOR_SUCCESS);

  // Play 1000Hz tone
  play_sound(1000);

  // Wait ~1 second (calibrated roughly for emulation)
  // Previous 5,000,000 was a bit long/short depending on CPU
  sleep(20000000);

  nosound();
  term_puts("Beep finished.\n", COLOR_DEFAULT);
}

void cmd_echo(const char *args) {
  term_puts(args, COLOR_DEFAULT);
  term_puts("\n", COLOR_DEFAULT);
}

void cmd_date() {
  DateTime dt;
  read_rtc(&dt);

  // Format: DD/MM/YYYY HH:MM:SS
  // Doing manual int-to-string printing since we don't have printf yet
  auto print_num = [](int n) {
    if (n < 10)
      term_putc('0');
    char buf[16];
    int i = 0;
    if (n == 0)
      buf[i++] = '0';
    while (n > 0) {
      buf[i++] = (n % 10) + '0';
      n /= 10;
    }
    while (--i >= 0)
      term_putc(buf[i]);
  };

  print_num(dt.day);
  term_putc('/');
  print_num(dt.month);
  term_putc('/');
  print_num(dt.year);
  term_putc(' ');
  print_num(dt.hour);
  term_putc(':');
  print_num(dt.minute);
  term_putc(':');
  print_num(dt.second);
  term_puts("\n", COLOR_DEFAULT);
}

void cmd_sysinfo() {
  term_puts("OS: TacosOS v0.1.0\n", COLOR_LOGO);
  term_puts("Kernel: Monolithic (Minimal)\n", COLOR_DEFAULT);
  term_puts("Arch: x86_64\n", COLOR_DEFAULT);
  term_puts("Compiler: GCC\n", COLOR_DEFAULT);
  term_puts("Bootloader: Multiboot2 (GRUB)\n", COLOR_DEFAULT);
}

void cmd_uptime() {
  DateTime now;
  read_rtc(&now);

  // Very rough approximation ignoring leap years/days for simplicity
  long boot_seconds =
      boot_time.hour * 3600 + boot_time.minute * 60 + boot_time.second;
  long now_seconds = now.hour * 3600 + now.minute * 60 + now.second;

  long diff = now_seconds - boot_seconds;
  if (diff < 0)
    diff += 24 * 3600; // Handle day rollover

  term_puts("System uptime: ", COLOR_DEFAULT);

  int hours = diff / 3600;
  int minutes = (diff % 3600) / 60;
  int seconds = diff % 60;

  // Manual printf again
  auto print_int = [](int n) {
    if (n == 0)
      term_putc('0');
    char tmp[10];
    int ti = 0;
    while (n > 0) {
      tmp[ti++] = (n % 10) + '0';
      n /= 10;
    }
    while (ti > 0)
      term_putc(tmp[--ti]);
  };

  print_int(hours);
  term_puts("h ");
  print_int(minutes);
  term_puts("m ");
  print_int(seconds);
  term_puts("s\n");
}

uint8_t parse_hex_char(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 0;
}

void cmd_color(const char *arg) {
  if (kstrlen(arg) < 2) {
    term_puts("Usage: color <hex code> (e.g. 0A)\n", COLOR_ERROR);
    return;
  }
  uint8_t bg = parse_hex_char(arg[0]);
  uint8_t fg = parse_hex_char(arg[1]);
  uint8_t new_color = (bg << 4) | fg;

  // Clear screen with new color attribute
  for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    // Keep character, change color
    uint16_t current = VGA_BUFFER[i];
    VGA_BUFFER[i] = (current & 0xFF) | (new_color << 8);
  }
  // Update global color for future prints?
  // Usually terminals only reset defaults on clears, but TacosOS is simple.
  // We'll just define a global variable for this:
  // (Note: Requires changing term_putc to use a global default or caller
  // providing it) For now, let's just clear the screen to apply it everywhere
  ;
}

// Forward declarations for sound
void play_sound(uint32_t nFrequence);
void nosound();

void cmd_matrix() {
  term_puts("Press ESC to stop...\n", COLOR_SUCCESS);

  // Clear any pending keyboard input first
  while (inb(0x64) & 1)
    kbd_scancode();

  while (1) {
    // Check for exit key
    if (inb(0x64) & 1) {
      uint8_t sc = kbd_scancode();
      // Exit on ESC (0x01)
      if (sc == 0x01) {
        break;
      }
      // Ignore other keys (especially release codes like Enter release 0x9C)
    }

    // Draw multiple characters per frame to make it faster/denser
    for (int i = 0; i < 5; i++) {
      int x = rand() % VGA_WIDTH;
      int y = rand() % VGA_HEIGHT;
      char c = (rand() % 93) + 33; // Ascii 33-126

      // Randomly choose between bright green and normal green
      uint8_t color = (rand() % 2) ? 0x0A : 0x02;

      VGA_BUFFER[y * VGA_WIDTH + x] = (uint16_t)c | (color << 8);
    }

    // Sound effects (Digital Rain Bleeps)
    // 10% chance to start a new sound, 5% chance to stop sound
    if ((rand() % 10) == 0) {
      // Random frequency between 200Hz and 2000Hz (wider range)
      play_sound(200 + (rand() % 1800));
    } else if ((rand() % 20) == 0) {
      nosound();
    }

    sleep(40000); // Frame delay
  }

  nosound();
  // Restore default colors
  clear_screen();
}

// --- PC Speaker Driver ---
void play_sound(uint32_t nFrequence) {
  uint32_t Div;
  uint8_t tmp;

  // Set the PIT to the desired frequency
  Div = 1193180 / nFrequence;
  outb(0x43, 0xb6);
  outb(0x42, (uint8_t)(Div));
  outb(0x42, (uint8_t)(Div >> 8));

  // And play the sound using the PC speaker
  tmp = inb(0x61);
  if (tmp != (tmp | 3)) {
    outb(0x61, tmp | 3);
  }

  // Visual Bell: Show Music Note in top-right corner
  VGA_BUFFER[79] = (uint16_t)14 | (0x0E << 8); // Yellow Note
}

void nosound() {
  uint8_t tmp = inb(0x61) & 0xFC;
  outb(0x61, tmp);

  // Clear Visual Bell
  VGA_BUFFER[79] = (uint16_t)' ' | (0x07 << 8);
}

// Notes for "It's Raining Tacos" (Lower Octave for better 8-bit sound)
#define NOTE_GS3 208
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_CS4 277
#define NOTE_DS4 311
#define NOTE_FS4 370

struct Note {
  int freq;
  int duration; // relative units
};

Note song[] = {
    // It's raining tacos
    {NOTE_GS3, 4},
    {NOTE_AS3, 4},
    {NOTE_B3, 4},
    {NOTE_GS3, 4},
    {NOTE_AS3, 4},
    {NOTE_FS4, 8},
    // From out of the sky
    {NOTE_GS3, 4},
    {NOTE_AS3, 4},
    {NOTE_B3, 4},
    {NOTE_CS4, 4},
    {NOTE_B3, 4},
    {NOTE_AS3, 8},
    // Tacos
    {NOTE_GS3, 4},
    {NOTE_AS3, 4},
    {NOTE_B3, 8},
    // No need to ask why
    {NOTE_GS3, 4},
    {NOTE_AS3, 4},
    {NOTE_B3, 4},
    {NOTE_CS4, 4},
    {NOTE_B3, 4},
    {NOTE_AS3, 8},
    // Just open your mouth
    {NOTE_GS3, 4},
    {NOTE_AS3, 4},
    {NOTE_B3, 4},
    {NOTE_CS4, 4},
    {NOTE_DS4, 4},
    {NOTE_CS4, 4},
    // And close your eyes
    {NOTE_B3, 4},
    {NOTE_AS3, 4},
    {NOTE_GS3, 8},
    // Repeat
    {0, 0}};

// --- Tacos Rain Game ---
struct Taco {
  int x, y;
  bool active;
};

void cmd_tacos() {
  clear_screen();
  term_puts("Catch the Tacos! (Use A/D or Arrows) - Press Any Key to Start",
            COLOR_SUCCESS);

  // Wait for start
  // Clear buffer
  for (int i = 0; i < 1000; i++) {
    if (inb(0x64) & 1)
      kbd_scancode();
  }

  // Wait for a valid key down event
  bool valid_start = false;
  while (!valid_start) {
    if (inb(0x64) & 1) {
      uint8_t c = kbd_scancode();
      // Ignore break codes (high bit set)
      if (!(c & 0x80))
        valid_start = true;
    }
  }

  clear_screen();

  if (sb16_active) {
    extern void sb16_play_tacos_melody();
    sb16_play_tacos_melody();
  }

  int width = VGA_WIDTH;
  int height = VGA_HEIGHT - 1; // Reserve bottom line
  int player_x = width / 2;
  int player_y = height - 1;
  int score = 0;
  bool game_over = false;

  Taco tacos[20];
  for (int i = 0; i < 20; i++)
    tacos[i].active = false;

  int loop_tick = 0;

  // Music State
  int note_idx = 0;
  int note_time = 0;

  while (!game_over) {
    // 0. Play Music (Only if SB16 isn't already playing background)
    if (!sb16_active) {
      if (note_time <= 0) {
        // Next note
        if (song[note_idx].freq == 0)
          note_idx = 0; // Loop

        if (song[note_idx].freq > 0) {
          play_sound(song[note_idx].freq);
          VGA_BUFFER[79] = (uint16_t)14 | (0x0E << 8); // Music Note
        } else {
          nosound();
          VGA_BUFFER[79] = (uint16_t)0 | (0x00 << 8); // Black
        }

        note_time = song[note_idx].duration * 3; // Scale duration
        note_idx++;
      } else {
        if (note_time == 1)
          nosound(); // Articulation gap
        note_time--;
      }
    } else {
      // Just show the music note for fun
      VGA_BUFFER[79] = (uint16_t)14 | (0x0E << 8);
    }

    // 1. Input (Responsive)
    for (int k = 0; k < 500; k++) {
      if (inb(0x64) & 1) {
        uint8_t code = kbd_scancode();
        if (code & 0x80)
          continue;
        if (code == 0x1E || code == 0x4B) {
          if (player_x > 0)
            player_x--;
        } else if (code == 0x20 || code == 0x4D) {
          if (player_x < width - 1)
            player_x++;
        } else if (code == 0x01 || code == 0x10) {
          game_over = true;
        }
      }
      sleep(100);
    }

    if (game_over)
      break;

    // 2. Logic
    loop_tick++;

    // Spawn new taco occasionally
    if ((loop_tick % 10) == 0) {
      for (int i = 0; i < 20; i++) {
        if (!tacos[i].active) {
          tacos[i].active = true;
          tacos[i].x = rand() % width;
          tacos[i].y = 0;
          break;
        }
      }
    }

    // Move tacos
    for (int i = 0; i < 20; i++) {
      if (tacos[i].active) {
        tacos[i].y++;

        // Detection
        if (tacos[i].y == player_y) {
          if (tacos[i].x == player_x) {
            // Caught!
            score++;
            tacos[i].active = false;
          }
        } else if (tacos[i].y > player_y) {
          // Missed! Just deactivate it
          tacos[i].active = false;
        }
      }
    }

    if (game_over)
      break;

    // 3. Render
    // Clear screen buffer (fast enough)
    for (int i = 0; i < width * height; i++) {
      VGA_BUFFER[i] = (uint16_t)' ' | (0x0F << 8);
    }

    // Draw Player (as 'U')
    VGA_BUFFER[player_y * width + player_x] =
        (uint16_t)'U' | (COLOR_SUCCESS << 8);

    // Draw Tacos
    for (int i = 0; i < 20; i++) {
      if (tacos[i].active) {
        VGA_BUFFER[tacos[i].y * width + tacos[i].x] =
            (uint16_t)'@' | (0x0E << 8); // Yellow
      }
    }

    // Score
    const char *prefix = "TACOS CAUGHT: ";
    int pidx = 0;
    int dpos = (VGA_HEIGHT - 1) * width;
    while (prefix[pidx])
      VGA_BUFFER[dpos++] = (uint16_t)prefix[pidx++] | (0x17 << 8);

    int s = score;
    if (s == 0)
      VGA_BUFFER[dpos++] = (uint16_t)'0' | (0x17 << 8);
    else {
      // quick print number logic
      char nb[12];
      int ni = 0;
      while (s > 0) {
        nb[ni++] = (s % 10) + '0';
        s /= 10;
      }
      while (ni > 0)
        VGA_BUFFER[dpos++] = (uint16_t)nb[--ni] | (0x17 << 8);
    }

    sleep(50000); // Frame delay
  }

  // Game Over
  clear_screen();
  term_puts("\n\n      GAME OVER - TACO DROPPED!\n", COLOR_ERROR);
  term_puts("      Final Score: ", COLOR_DEFAULT);

  if (score == 0)
    term_putc('0');
  char buf[10];
  int bi = 0;
  while (score > 0) {
    buf[bi++] = (score % 10) + '0';
    score /= 10;
  }
  while (bi > 0)
    term_putc(buf[--bi]);

  term_puts("\n\n      Press Key...\n", COLOR_DEFAULT);
  while (inb(0x64) & 1)
    kbd_scancode();
  while (!(inb(0x64) & 1))
    ;
  kbd_scancode();
  clear_screen();
}

int find_file(const char *name, const char *dir) {
  for (int i = 0; i < file_count; i++) {
    if (kstrcmp(file_system[i].name, name) == 0 &&
        kstrcmp(file_system[i].parent_dir, dir) == 0) {
      return i;
    }
  }
  return -1;
}

void cmd_cp(char *args) {
  // Basic parser for "cp src dest"
  // Limitations: No spaces in filenames supported by this simple parser
  char src[32];
  char dest[32];
  int i = 0, j = 0;

  while (args[i] && args[i] != ' ')
    src[j++] = args[i++];
  src[j] = '\0';

  if (args[i] == '\0') {
    term_puts("Usage: cp <src> <dest>\n", COLOR_ERROR);
    return;
  }
  i++; // Skip space

  j = 0;
  while (args[i])
    dest[j++] = args[i++];
  dest[j] = '\0';

  int idx = find_file(src, current_dir);
  if (idx != -1) {
    if (file_count < MAX_FILES) {
      file_system[file_count] = file_system[idx];
      kstrcpy(file_system[file_count].name, dest);
      // Parent dir remains the same (current_dir) for simplicity
      // unless dest contains ".." or "/" which is too complex for now
      file_count++;
      fs_save();
      term_puts("File copied.\n", COLOR_SUCCESS);
    } else {
      term_puts("Error: File system full.\n", COLOR_ERROR);
    }
  } else {
    term_puts("Error: Source file not found.\n", COLOR_ERROR);
  }
}

void cmd_mv(char *args) {
  // Basic parser for "mv src dest"
  char src[32];
  char dest[32];
  int i = 0, j = 0;

  while (args[i] && args[i] != ' ')
    src[j++] = args[i++];
  src[j] = '\0';

  if (args[i] == '\0') {
    term_puts("Usage: mv <src> <dest>\n", COLOR_ERROR);
    return;
  }
  i++; // Skip space

  j = 0;
  while (args[i])
    dest[j++] = args[i++];
  dest[j] = '\0';

  int idx = find_file(src, current_dir);
  if (idx != -1) {
    kstrcpy(file_system[idx].name, dest);
    fs_save();
    term_puts("File renamed.\n", COLOR_SUCCESS);
  } else {
    term_puts("Error: Source file not found.\n", COLOR_ERROR);
  }
}

// --- System Commands ---
void reboot() {
  term_puts("Rebooting...\n", COLOR_LOGO);
  // Pulse the reset line using the keyboard controller
  uint8_t good = 0x02;
  while (good & 0x02)
    good = inb(0x64);
  outb(0x64, 0xFE);
  while (1)
    asm volatile("hlt"); // Fallback
}

void shutdown() {
  term_puts("Shutting down...\n", COLOR_LOGO);
  // QEMU/VirtualBox/Bochs power off ports
  asm volatile("outw %1, %0" : : "dN"(0x604), "a"((uint16_t)0x2000)); // QEMU
  asm volatile("outw %1, %0"
               :
               : "dN"(0x4004), "a"((uint16_t)0x3400)); // VirtualBox
  asm volatile("outw %1, %0" : : "dN"(0xB004), "a"((uint16_t)0x2000)); // Bochs
  term_puts("Shutdown failed. Hardware does not support magic port.\n",
            COLOR_ERROR);
}

// --- Editing State ---
static bool is_editing = false;
static int editing_file_idx = -1;

void execute_command(char *cmd) {
  if (kstrcmp(cmd, "logo") == 0) {
    cmd_logo();
  } else if (kstrcmp(cmd, "help") == 0) {
    term_puts("Available commands:\n", COLOR_DEFAULT);
    term_puts("  logo            Show the TacosOS logo\n");
    term_puts("  ls              List files in the directory\n");
    term_puts("  cd <path>       Change the current directory\n");
    term_puts("  mkdir <name>    Create a new directory\n");
    term_puts("  new <name>      Create a new file\n");
    term_puts("  open <name>     Open and read a file\n");
    term_puts("  edit <name>     Edit content of a file\n");
    term_puts("  rm <name>       Delete a file\n");
    term_puts("  cp <src> <dst>  Copy a file\n");
    term_puts("  mv <src> <dst>  Rename a file\n");
    term_puts("  clear           Clear the screen\n");
    term_puts("  date            Show current time\n");
    term_puts("  uptime          Show system uptime\n");
    term_puts("  sysinfo         Show system info\n");
    term_puts("  echo <text>     Print text\n");
    term_puts("  color <hex>     Change screen color (e.g. 0A)\n");
    term_puts("  matrix          Enter the matrix\n");
    term_puts("  tacos           Catch falling tacos game\n");
    term_puts("  reboot          Restart the computer\n");
    term_puts("  shutdown        Power off the machine\n");
    term_puts("  beep            Test PC speaker sound\n");
    term_puts("  help            Show this help message\n");
    term_puts("\n", COLOR_DEFAULT);
    term_puts("  Created By YBL (ynbd11)\n", COLOR_LOGO);

  } else if (kstrcmp(cmd, "ls") == 0) {
    bool empty = true;
    int curr_len = kstrlen(current_dir);

    // Show subdirectories
    for (int i = 0; i < dir_count; i++) {
      if (kstrcmp(valid_dirs[i], current_dir) == 0)
        continue;

      bool is_child = false;
      if (kstrcmp(current_dir, "/") == 0) {
        // Child of root has exactly one slash at index 0
        int slash_count = 0;
        for (int j = 0; valid_dirs[i][j]; j++)
          if (valid_dirs[i][j] == '/')
            slash_count++;
        if (slash_count == 1)
          is_child = true;
      } else {
        // Child of /X starts with /X/ and has no slashes after that
        int j = 0;
        while (current_dir[j] && valid_dirs[i][j] == current_dir[j])
          j++;
        if (current_dir[j] == '\0' && valid_dirs[i][j] == '/') {
          int slash_count = 0;
          for (int k = j + 1; valid_dirs[i][k]; k++)
            if (valid_dirs[i][k] == '/')
              slash_count++;
          if (slash_count == 0)
            is_child = true;
        }
      }

      if (is_child) {
        const char *name = valid_dirs[i];
        // Find start of name after current_dir
        if (kstrcmp(current_dir, "/") == 0)
          name += 1;
        else
          name += curr_len + 1;

        term_puts(name, COLOR_PROMPT);
        term_puts("/ ", COLOR_PROMPT);
        empty = false;
      }
    }
    // Show files
    for (int i = 0; i < file_count; i++) {
      if (kstrcmp(file_system[i].parent_dir, current_dir) == 0) {
        term_puts(file_system[i].name, COLOR_DEFAULT);
        term_puts("  ", COLOR_DEFAULT);
        empty = false;
      }
    }
    if (empty) {
      term_puts("Directory empty.\n", COLOR_DEFAULT);
    } else {
      term_puts("\n", COLOR_DEFAULT);
    }

  } else if (kstrcmp(cmd, "clear") == 0) {
    cmd_clear();
  } else if (kstrcmp(cmd, "date") == 0) {
    cmd_date();
  } else if (kstrcmp(cmd, "sysinfo") == 0) {
    cmd_sysinfo();
  } else if (kstrcmp(cmd, "uptime") == 0) {
    cmd_uptime();
  } else if (kstrcmp(cmd, "matrix") == 0) {
    cmd_matrix();
  } else if (kstrcmp(cmd, "tacos") == 0) {
    cmd_tacos();
  } else if (kstrncmp(cmd, "echo ", 5) == 0) {
    cmd_echo(cmd + 5);
  } else if (kstrncmp(cmd, "cp ", 3) == 0) {
    cmd_cp(cmd + 3);
  } else if (kstrncmp(cmd, "mv ", 3) == 0) {
    cmd_mv(cmd + 3);
  } else if (kstrncmp(cmd, "color ", 6) == 0) {
    cmd_color(cmd + 6);
  } else if (kstrcmp(cmd, "reboot") == 0) {
    reboot();
  } else if (kstrcmp(cmd, "shutdown") == 0 ||
             kstrncmp(cmd, "shutdown ", 9) == 0) {
    shutdown();
  } else if (kstrcmp(cmd, "beep") == 0) {
    cmd_beep();
  } else if (kstrcmp(cmd, "playpcm") == 0) {
    extern void cmd_play_test();
    cmd_play_test();
  } else if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ') {
    const char *target = cmd + 3;

    // Handle ".."
    if (kstrcmp(target, "..") == 0 || kstrcmp(target, "/..") == 0) {
      if (kstrcmp(current_dir, "/") == 0) {
        // Already at root
      } else {
        // Find last slash
        int last_slash = 0;
        for (int i = 0; current_dir[i]; i++)
          if (current_dir[i] == '/')
            last_slash = i;
        if (last_slash == 0) {
          kstrcpy(current_dir, "/");
        } else {
          current_dir[last_slash] = '\0';
        }
      }
      term_puts("Navigated to: ", COLOR_SUCCESS);
      term_puts(current_dir, COLOR_SUCCESS);
      term_putc('\n');
    } else {
      char full_target[32];
      if (target[0] == '/') {
        kstrcpy(full_target, target);
      } else {
        kstrcpy(full_target, current_dir);
        if (kstrcmp(current_dir, "/") != 0)
          kstrcat(full_target, "/");
        kstrcat(full_target, target);
      }

      bool found = false;
      for (int i = 0; i < dir_count; i++) {
        if (kstrcmp(valid_dirs[i], full_target) == 0) {
          found = true;
          break;
        }
      }
      if (found) {
        kstrcpy(current_dir, full_target);
        term_puts("Navigated to: ", COLOR_SUCCESS);
        term_puts(current_dir, COLOR_SUCCESS);
        term_putc('\n');
      } else {
        term_puts("Error: Directory not found: ", COLOR_ERROR);
        term_puts(full_target, COLOR_ERROR);
        term_putc('\n');
      }
    }

  } else if (cmd[0] == 'r' && cmd[1] == 'm' && cmd[2] == ' ') {
    const char *target = cmd + 3;

    // 1. Try deleting a file in the current directory
    int file_idx = -1;
    for (int i = 0; i < file_count; i++) {
      if (kstrcmp(file_system[i].name, target) == 0 &&
          kstrcmp(file_system[i].parent_dir, current_dir) == 0) {
        file_idx = i;
        break;
      }
    }

    if (file_idx != -1) {
      for (int i = file_idx; i < file_count - 1; i++)
        file_system[i] = file_system[i + 1];
      file_count--;
      term_puts("File removed.\n", COLOR_SUCCESS);
      fs_save();
      return;
    }

    // 2. Try deleting a directory
    char full_target[32];
    if (target[0] == '/') {
      kstrcpy(full_target, target);
    } else {
      kstrcpy(full_target, current_dir);
      if (kstrcmp(current_dir, "/") != 0)
        kstrcat(full_target, "/");
      kstrcat(full_target, target);
    }

    if (kstrcmp(full_target, "/") == 0) {
      term_puts("Error: Cannot remove root directory.\n", COLOR_ERROR);
      return;
    }

    int dir_idx = -1;
    for (int i = 0; i < dir_count; i++) {
      if (kstrcmp(valid_dirs[i], full_target) == 0) {
        dir_idx = i;
        break;
      }
    }

    if (dir_idx != -1) {
      // Recursive Cleanup: Remove all files and directories starting with this
      // path Important: check if target is a prefix + /
      int target_len = kstrlen(full_target);

      // Remove nested files
      for (int i = 0; i < file_count; i++) {
        // Check if parent_dir starts with full_target
        if (kstrncmp(file_system[i].parent_dir, full_target, target_len) == 0) {
          // Check if it's the exact dir or followed by /
          if (file_system[i].parent_dir[target_len] == '\0' ||
              file_system[i].parent_dir[target_len] == '/') {
            // Shift files
            for (int j = i; j < file_count - 1; j++)
              file_system[j] = file_system[j + 1];
            file_count--;
            i--; // Re-check this index
          }
        }
      }

      // Remove nested directories (except the one we are about to remove)
      for (int i = 0; i < dir_count; i++) {
        if (i == dir_idx)
          continue;
        if (kstrncmp(valid_dirs[i], full_target, target_len) == 0) {
          if (valid_dirs[i][target_len] == '/') {
            // Shift dirs
            for (int j = i; j < dir_count - 1; j++)
              kstrcpy(valid_dirs[j], valid_dirs[j + 1]);
            dir_count--;
            if (i < dir_idx)
              dir_idx--; // Adjust current dir index if shifted
            i--;
          }
        }
      }

      // Remove the target directory itself
      for (int i = dir_idx; i < dir_count - 1; i++)
        kstrcpy(valid_dirs[i], valid_dirs[i + 1]);
      dir_count--;

      // If we just deleted where we are, jump to root
      if (kstrncmp(current_dir, full_target, target_len) == 0) {
        if (current_dir[target_len] == '\0' || current_dir[target_len] == '/') {
          kstrcpy(current_dir, "/");
          term_puts("Current directory removed. Jumped to /.\n", COLOR_PROMPT);
        }
      }

      term_puts("Directory and its contents removed.\n", COLOR_SUCCESS);
      fs_save();
    } else {
      term_puts("Error: '", COLOR_ERROR);
      term_puts(target, COLOR_ERROR);
      term_puts("' not found.\n", COLOR_ERROR);
    }

  } else if (cmd[0] == 'm' && cmd[1] == 'k' && cmd[2] == 'd' && cmd[3] == 'i' &&
             cmd[4] == 'r' && cmd[5] == ' ') {
    const char *name = cmd + 6;
    if (dir_count < MAX_DIRS) {
      char full_path[32];
      if (name[0] == '/') {
        kstrcpy(full_path, name);
      } else {
        kstrcpy(full_path, current_dir);
        if (kstrcmp(current_dir, "/") != 0)
          kstrcat(full_path, "/");
        kstrcat(full_path, name);
      }

      kstrcpy(valid_dirs[dir_count++], full_path);
      term_puts("Directory created: ", COLOR_SUCCESS);
      term_puts(full_path, COLOR_SUCCESS);
      term_putc('\n');
      fs_save();
    } else {
      term_puts("Error: Maximum directory limit reached.\n", COLOR_ERROR);
    }

  } else if (cmd[0] == 'r' && cmd[1] == 'm' && cmd[2] == ' ') {
    const char *target = cmd + 3;
    int found_idx = -1;
    for (int i = 0; i < file_count; i++) {
      if (kstrcmp(file_system[i].name, target) == 0 &&
          kstrcmp(file_system[i].parent_dir, current_dir) == 0) {
        found_idx = i;
        break;
      }
    }
    if (found_idx != -1) {
      for (int i = found_idx; i < file_count - 1; i++) {
        file_system[i] = file_system[i + 1];
      }
      file_count--;
      term_puts("File removed.\n", COLOR_SUCCESS);
    } else {
      term_puts("Error: File not found in current directory.\n", COLOR_ERROR);
    }

  } else if (cmd[0] == 'n' && cmd[1] == 'e' && cmd[2] == 'w' && cmd[3] == ' ') {
    if (file_count < MAX_FILES) {
      kstrcpy(file_system[file_count].name, cmd + 4);
      kstrcpy(file_system[file_count].parent_dir, current_dir);
      kstrcpy(file_system[file_count].content, "Empty taco.");
      file_count++;
      term_puts("File created.\n", COLOR_SUCCESS);
      fs_save();
    } else {
      term_puts("Error: File system full.\n", COLOR_ERROR);
    }

  } else if (cmd[0] == 'o' && cmd[1] == 'p' && cmd[2] == 'e' && cmd[3] == 'n' &&
             cmd[4] == ' ') {
    const char *target = cmd + 5;
    int found_idx = -1;
    for (int i = 0; i < file_count; i++) {
      if (kstrcmp(file_system[i].name, target) == 0 &&
          kstrcmp(file_system[i].parent_dir, current_dir) == 0) {
        found_idx = i;
        break;
      }
    }
    if (found_idx != -1) {
      term_puts("Content: ", COLOR_DEFAULT);
      term_puts(file_system[found_idx].content, COLOR_DEFAULT);
      term_putc('\n');
    } else {
      term_puts("Error: File not found in current directory.\n", COLOR_ERROR);
    }

  } else if (cmd[0] == 'e' && cmd[1] == 'd' && cmd[2] == 'i' && cmd[3] == 't' &&
             cmd[4] == ' ') {
    const char *target = cmd + 5;
    int found_idx = -1;
    for (int i = 0; i < file_count; i++) {
      if (kstrcmp(file_system[i].name, target) == 0 &&
          kstrcmp(file_system[i].parent_dir, current_dir) == 0) {
        found_idx = i;
        break;
      }
    }
    if (found_idx != -1) {
      term_puts("Editing: ", COLOR_SUCCESS);
      term_puts(target, COLOR_SUCCESS);
      term_puts("\nEnter text: ", COLOR_DEFAULT);
      is_editing = true;
      editing_file_idx = found_idx;
    } else {
      term_puts("Error: File not found in current directory.\n", COLOR_ERROR);
    }
  } else if (cmd[0] != '\0') {
    term_puts("Unknown command: ", COLOR_ERROR);
    term_puts(cmd, COLOR_ERROR);
    term_puts(". Type 'help' for options.\n");
  }
}

// --- Main Loop ---
extern "C" void kmain() {
  idt_init();
  clear_screen();

  // Initialize Filesystem
  term_puts("Initializing Filesystem...", COLOR_LOGO);
  if (fs_init()) {
    term_puts(" [OK]\n", COLOR_SUCCESS);
  } else {
    term_puts(" [FAIL] (Disk not ready, using RAM mode)\n", COLOR_ERROR);
  }

  // Initialize SB16
  term_puts("Initializing SB16 Audio...", COLOR_LOGO);
  if (sb16_init()) {
    term_puts(" [OK]\n", COLOR_SUCCESS);
    sb16_active = true;
  } else {
    term_puts(" [FAIL] (Not detected)\n", COLOR_ERROR);
    sb16_active = false;
  }

  term_puts("TacosOS Minimal Terminal initialized.\n", 0x0F);
  term_puts("Display: VGA 80x25 Text Mode\n\n");

  // Show initial logo
  cmd_logo();
  term_puts("Type 'help' for more info.\n\n", COLOR_DEFAULT);

  // Record boot time for uptime command
  read_rtc(&boot_time);

  char cmd_buffer[81];
  int cmd_pos = 0;

  while (1) {
    // Show Prompt or Edit message
    if (is_editing) {
      term_puts("EDITING > ", COLOR_PROMPT);
    } else {
      term_puts(current_dir, COLOR_PROMPT);
      term_puts(" > ", COLOR_PROMPT);
    }

    // Read Command / Content
    cmd_pos = 0;
    while (1) {
      uint8_t scancode = kbd_scancode();
      char c = scancode_to_ascii(scancode);

      if (c == '\n') {
        term_putc('\n');
        cmd_buffer[cmd_pos] = '\0';
        break;
      } else if (c == '\b') {
        if (cmd_pos > 0) {
          cmd_pos--;
          term_putc('\b');
        }
      } else if (c != 0 && cmd_pos < 80) {
        cmd_buffer[cmd_pos++] = c;
        term_putc(c);
      }
    }

    if (is_editing) {
      kstrcpy(file_system[editing_file_idx].content, cmd_buffer);
      term_puts("File updated.\n", COLOR_SUCCESS);
      is_editing = false;
      editing_file_idx = -1;
      fs_save();
    } else {
      execute_command(cmd_buffer);
    }
  }
}
