// See LICENSE.Cambridge for license details.

#include <stddef.h>
#include "uart.h"
#include "hid.h"
#include "mini-printf.h"

const struct { char scan,lwr,upr; } scancode[] = {
#include "scancode.h"
  };

// LowRISC VGA-compatible display base address
volatile uint16_t *const hid_vga_ptr = (uint16_t *)VgaBase;
// VGA tuning registers
volatile uint64_t *const hid_reg_ptr = (volatile uint64_t *)(VgaBase+16384);
// VGA frame buffer
volatile uint64_t *const hid_fb_ptr = (volatile uint64_t *)(FbBase);
// Downloadable font pointer
volatile uint8_t *const hid_font_ptr = (volatile uint8_t *)(VgaBase+24576);
// HID keyboard
volatile uint32_t *const keyb_base = (volatile uint32_t *)KeybBase;
// HID mouse
volatile uint64_t *const mouse_base = (volatile uint32_t *)MouseBase;

static int addr_int = 0;

void hid_console_putchar(unsigned char ch)
{
  int blank = ' '|0x8F00;
  switch(ch)
    {
    case 8: case 127: if (addr_int & 127) hid_vga_ptr[--addr_int] = blank; break;
    case 13: addr_int = addr_int & -128; break;
    case 10:
      {
        int lmt = (addr_int|127)+1; while (addr_int < lmt) hid_vga_ptr[(addr_int++)] = blank;
        break;
      }
    default: hid_vga_ptr[addr_int++] = ch|0xF00;
    }
  if (addr_int >= LOWRISC_MEM)
    {
      // this is where we scroll
      for (addr_int = LOWRISC_START; addr_int < LOWRISC_MEM; addr_int++)
        if (addr_int < LOWRISC_MEM-128)
          hid_vga_ptr[addr_int] = hid_vga_ptr[addr_int+128];
        else
          hid_vga_ptr[addr_int] = blank;
      addr_int = LOWRISC_MEM-128;
    }
  hid_reg_ptr[LOWRISC_REGS_XCUR] = addr_int & 127;
  hid_reg_ptr[LOWRISC_REGS_YCUR] = addr_int >> 7;
}

void uart_console_putchar(unsigned char ch)
{
  write_serial(ch);
}  

void hid_init(void)
{
  enum {width=1024, height=768};
  int i, j, ghlimit = 80;
  
  addr_int = LOWRISC_START;
  hid_reg_ptr[LOWRISC_REGS_CURSV] = 10;
  hid_reg_ptr[LOWRISC_REGS_XCUR] = 0;
  hid_reg_ptr[LOWRISC_REGS_YCUR] = 32;
  hid_reg_ptr[LOWRISC_REGS_HSTART] = width*2;
  hid_reg_ptr[LOWRISC_REGS_HSYN] = width*2+20;
  hid_reg_ptr[LOWRISC_REGS_HSTOP] = width*2+51;
  hid_reg_ptr[LOWRISC_REGS_VSTART] = height;
  hid_reg_ptr[LOWRISC_REGS_VSTOP] = height+19;
  hid_reg_ptr[LOWRISC_REGS_VPIXSTART ] = 16;
  hid_reg_ptr[LOWRISC_REGS_VPIXSTOP ] = height+16;
  hid_reg_ptr[LOWRISC_REGS_HPIXSTART ] = 128*3;
  hid_reg_ptr[LOWRISC_REGS_HPIXSTOP ] = 128*3+256*6;
  hid_reg_ptr[LOWRISC_REGS_HPIX ] = 5;
  hid_reg_ptr[LOWRISC_REGS_VPIX ] = 11; // squashed vertical display uses 10
  hid_reg_ptr[LOWRISC_REGS_GHLIMIT] = ghlimit / 2;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     0] = 0x20272D;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     1] = 0xE0354F;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     2] = 0xE9374F;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     3] = 0xE1E6E8;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     4] = 0xAA0000;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     5] = 0xAA00AA;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     6] = 0xAA5500;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     7] = 0xAAAAAA;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     8] = 0x555555;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +     9] = 0x5555FF;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +    10] = 0x55FF55;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +    11] = 0x55FFFF;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +    12] = 0xFF5555;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +    13] = 0xFF55FF;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +    14] = 0xFFFF55;
  hid_reg_ptr[LOWRISC_REGS_PALETTE +    15] = 0xFFFFFF;
  for (i = 0; i <= 127; i++)
    {
      const char *zptr = zifu + (i) * 12;
      for (j = 0; j < 12; j++)
        hid_font_ptr[16*i+j] = (0xFC & *zptr++) >> 1;
      for (j = 12; j < 16; j++)
        hid_font_ptr[16*i+j] = 0;
    }
  for (j = 0; j < 16; j++)
    hid_font_ptr[16*i+j] = 0xFF;
  for (i = 0; i < 768*32; i++)
    hid_fb_ptr[i] = 0;
  hid_reg_ptr[LOWRISC_REGS_MODE] = 2;
  for (i = 0; i < 32; i++)
    {
      for (j = 0; j < 128; j++)
        {
          if (j < 108)
            hid_vga_ptr[128*i+j] = 0x8080|(i<<8);
          else if (j < 118)
            hid_vga_ptr[128*i+j] = 0x80;
          else
            hid_vga_ptr[128*i+j] = 0x80|(i<<8);
        }
    }
  draw_logo(ghlimit);
  hid_reg_ptr[LOWRISC_REGS_MODE] = 1;  
}

void hid_send_irq(uint8_t data)
{

}

void hid_send(uint8_t data)
{
  uart_console_putchar(data);
  hid_console_putchar(data);
}

void hid_send_string(const char *str) {
  while (*str) hid_send(*str++);
}

void hid_send_buf(const char *buf, const int32_t len)
{
  int32_t i;
  for (i=0; i<len; i++) hid_send(buf[i]);
}

uint8_t hid_recv()
{
  return -1;
}

// IRQ triggered read
uint8_t hid_read_irq() {
  return -1;
}

// check hid IRQ for read
uint8_t hid_check_read_irq() {
  return 0;
}

// enable hid read IRQ
void hid_enable_read_irq() {

}

// disable hid read IRQ
void hid_disable_read_irq() {

}

int hid_putc(int c, ...) {
  hid_send(c);
  return c;
}

int hid_puts(const char *str) {
  while (*str) hid_send(*str++);
  hid_send('\n');
  return 0;
}

void keyb_main(void)
{
  int i;
  int height = 11;
  int width = 5;
  uint64_t mouse_ev, old_ev = -1;
  for (;;)
    {
      int scan, ascii, event = *keyb_base;
      if (0x200 & ~event)
        {
          *keyb_base = 0; // pop FIFO
          event = *keyb_base & ~0x200;
          scan = scancode[event&~0x100].scan;
          ascii = scancode[event&~0x100].lwr;
          printf("Keyboard event = %X, scancode = %X, ascii = '%c'\n", event, scan, ascii);
          if (0x100 & ~event) switch(scan)
            {
            case 0x50: hid_reg_ptr[LOWRISC_REGS_VPIX] = ++height; printf(" %d,%d", height, width); break;
            case 0x48: hid_reg_ptr[LOWRISC_REGS_VPIX] = --height; printf(" %d,%d", height, width); break;
            case 0x4D: hid_reg_ptr[LOWRISC_REGS_HPIX] = ++width; printf(" %d,%d", height, width); break;
            case 0x4B: hid_reg_ptr[LOWRISC_REGS_HPIX] = --width; printf(" %d,%d", height, width); break;
            case 0xE0: break;
            case 0x39: for (i = 33; i < 47; i++) hid_reg_ptr[i] = rand32(); break;
            default: printf("?%x", scan); break;
            }
        }
      mouse_ev = *mouse_base;
      if (old_ev != mouse_ev)
        {
          int X = mouse_ev & 1023;
          int Y = (mouse_ev>>16) & 1023;
          int Z = (mouse_ev>>32) & 15;
          int ev = (mouse_ev>>36) & 1;
          int right = (mouse_ev>>37) & 1;
          int middle = (mouse_ev>>38) & 1;
          int left = (mouse_ev>>39) & 1;
          printf("Mouse event: X=%d, Y=%d, left=%d, middle=%d, right=%d\n", X, Y, left, middle, right);
          old_ev = mouse_ev;
        }
    }
}

const char zifu[]={
/*-- ^@  --*/
0x00,0x00,0x20,0x60,0xfc,0xfc,0x60,0x20,0x00,0x00,0x00,0x00,
/*-- ^A  --*/
0x00,0x00,0x10,0x18,0xfc,0xfc,0x18,0x10,0x00,0x00,0x00,0x00,
/*-- ^B  --*/
0x00,0x30,0x78,0xfc,0x30,0x30,0x30,0x30,0x30,0x30,0x00,0x00,
/*-- ^C  --*/
0x00,0x30,0x30,0x30,0x30,0x30,0x30,0xfc,0x78,0x30,0x00,0x00,
/*-- ^D  --*/
0x00,0x78,0x84,0x82,0x82,0x82,0x82,0x82,0x84,0x78,0x00,0x00,
/*-- ^E  --*/
0x00,0x78,0x84,0xb2,0xfa,0xfa,0xfa,0xb2,0x84,0x78,0x00,0x00,
/*-- ^F  --*/
0x00,0xfe,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0xfe,0x00,0x00,
/*-- ^G  --*/
0x00,0xfe,0x86,0xce,0xfa,0xb2,0xfa,0xce,0x86,0xfe,0x00,0x00,
/*-- ^H  --*/
0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xf8,0xf0,0xe0,0xc0,0x80,0x00,
/*-- ^I  --*/
0x00,0x00,0x00,0x70,0xf8,0xf8,0xf8,0x70,0x00,0x00,0x00,0x00,
/*-- ^J  --*/
0x00,0x00,0x00,0x00,0x00,0x06,0x1e,0x18,0x30,0x30,0x30,0x30,
/*-- ^K  --*/
0x00,0x00,0x00,0x00,0x00,0x80,0xe0,0x60,0x30,0x30,0x30,0x30,
/*-- ^L  --*/
0x30,0x30,0x30,0x30,0x18,0x1e,0x06,0x00,0x00,0x00,0x00,0x00,
/*-- ^M  --*/
0x30,0x30,0x30,0x30,0x60,0xe0,0x80,0x00,0x00,0x00,0x00,0x00,
/*-- ^N  --*/
0x00,0x00,0x00,0x00,0x00,0xfe,0xfe,0x00,0x00,0x00,0x00,0x00,
/*-- ^O  --*/
0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
/*-- ^P  --*/
0x00,0x00,0x00,0x00,0x00,0xfe,0xfe,0x30,0x30,0x30,0x30,0x30,
/*-- ^Q  --*/
0x30,0x30,0x30,0x30,0x30,0xfe,0xfe,0x00,0x00,0x00,0x00,0x00,
/*-- ^R  --*/
0x30,0x30,0x30,0x30,0x30,0x3e,0x3e,0x30,0x30,0x30,0x30,0x30,
/*-- ^S  --*/
0x30,0x30,0x30,0x30,0x30,0xf0,0xf0,0x30,0x30,0x30,0x30,0x30,
/*-- ^T  --*/
0x30,0x30,0x30,0x30,0x30,0xfe,0xfe,0x30,0x30,0x30,0x30,0x30,
/*-- ^U  --*/
0x00,0xfe,0xfe,0x00,0x00,0xfe,0xfe,0x00,0x00,0xfe,0xfe,0x00,
/*-- ^V  --*/
0xcc,0x98,0xb2,0x66,0xcc,0x98,0xb2,0x66,0xcc,0x98,0xb2,0x66,
/*-- ^W  --*/
0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,
/*-- ^X  --*/
0xd4,0xaa,0xd4,0xaa,0xd4,0xaa,0xd4,0xaa,0xd4,0xaa,0xd4,0xaa,
/*-- ^Y  --*/
0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,
/*-- ^Z  --*/
0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,
/*-- ^[  --*/
0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,
/*-- ^\  --*/
0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- ^]  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,
/*-- ^^  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,
/*-- ^_  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,
/*--     --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- !  --*/
0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00,0x00,
/*-- "  --*/
0x00,0x28,0x50,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- #  --*/
0x00,0x00,0x28,0x28,0xFC,0x28,0x50,0xFC,0x50,0x50,0x00,0x00,
/*-- $  --*/
0x00,0x20,0x78,0xA8,0xA0,0x60,0x30,0x28,0xA8,0xF0,0x20,0x00,
/*-- %  --*/
0x00,0x00,0x48,0xA8,0xB0,0x50,0x28,0x34,0x54,0x48,0x00,0x00,
/*-- &  --*/
0x00,0x00,0x20,0x50,0x50,0x78,0xA8,0xA8,0x90,0x6C,0x00,0x00,
/*-- '  --*/
0x00,0x40,0x40,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- (  --*/
0x00,0x04,0x08,0x10,0x10,0x10,0x10,0x10,0x10,0x08,0x04,0x00,
/*-- )  --*/
0x00,0x40,0x20,0x10,0x10,0x10,0x10,0x10,0x10,0x20,0x40,0x00,
/*-- *  --*/
0x00,0x00,0x00,0x20,0xA8,0x70,0x70,0xA8,0x20,0x00,0x00,0x00,
/*-- +  --*/
0x00,0x00,0x20,0x20,0x20,0xF8,0x20,0x20,0x20,0x00,0x00,0x00,
/*-- ,  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x40,0x80,
/*-- -  --*/
0x00,0x00,0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- .  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,
/*-- /  --*/
0x00,0x08,0x10,0x10,0x10,0x20,0x20,0x40,0x40,0x40,0x80,0x00,
/*-- 0  --*/
0x00,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x00,
/*-- 1  --*/
0x00,0x00,0x20,0x60,0x20,0x20,0x20,0x20,0x20,0x70,0x00,0x00,
/*-- 2  --*/
0x00,0x00,0x70,0x88,0x88,0x10,0x20,0x40,0x80,0xF8,0x00,0x00,
/*-- 3  --*/
0x00,0x00,0x70,0x88,0x08,0x30,0x08,0x08,0x88,0x70,0x00,0x00,
/*-- 4  --*/
0x00,0x00,0x10,0x30,0x50,0x50,0x90,0x78,0x10,0x18,0x00,0x00,
/*-- 5  --*/
0x00,0x00,0xF8,0x80,0x80,0xF0,0x08,0x08,0x88,0x70,0x00,0x00,
/*-- 6  --*/
0x00,0x00,0x70,0x90,0x80,0xF0,0x88,0x88,0x88,0x70,0x00,0x00,
/*-- 7  --*/
0x00,0x00,0xF8,0x90,0x10,0x20,0x20,0x20,0x20,0x20,0x00,0x00,
/*-- 8  --*/
0x00,0x00,0x70,0x88,0x88,0x70,0x88,0x88,0x88,0x70,0x00,0x00,
/*-- 9  --*/
0x00,0x00,0x70,0x88,0x88,0x88,0x78,0x08,0x48,0x70,0x00,0x00,
/*-- :  --*/
0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x20,0x00,0x00,
/*-- ;  --*/
0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x20,0x20,0x00,
/*-- <  --*/
0x00,0x04,0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x04,0x00,0x00,
/*-- =  --*/
0x00,0x00,0x00,0x00,0xF8,0x00,0x00,0xF8,0x00,0x00,0x00,0x00,
/*-- >  --*/
0x00,0x40,0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x40,0x00,0x00,
/*-- ?  --*/
0x00,0x00,0x70,0x88,0x88,0x10,0x20,0x20,0x00,0x20,0x00,0x00,
/*-- @  --*/
0x00,0x00,0x70,0x88,0x98,0xA8,0xA8,0xB8,0x80,0x78,0x00,0x00,
/*-- A  --*/
0x00,0x00,0x20,0x20,0x30,0x50,0x50,0x78,0x48,0xCC,0x00,0x00,
/*-- B  --*/
0x00,0x00,0xF0,0x48,0x48,0x70,0x48,0x48,0x48,0xF0,0x00,0x00,
/*-- C  --*/
0x00,0x00,0x78,0x88,0x80,0x80,0x80,0x80,0x88,0x70,0x00,0x00,
/*-- D  --*/
0x00,0x00,0xF0,0x48,0x48,0x48,0x48,0x48,0x48,0xF0,0x00,0x00,
/*-- E  --*/
0x00,0x00,0xF8,0x48,0x50,0x70,0x50,0x40,0x48,0xF8,0x00,0x00,
/*-- F  --*/
0x00,0x00,0xF8,0x48,0x50,0x70,0x50,0x40,0x40,0xE0,0x00,0x00,
/*-- G  --*/
0x00,0x00,0x38,0x48,0x80,0x80,0x9C,0x88,0x48,0x30,0x00,0x00,
/*-- H  --*/
0x00,0x00,0xCC,0x48,0x48,0x78,0x48,0x48,0x48,0xCC,0x00,0x00,
/*-- I  --*/
0x00,0x00,0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0xF8,0x00,0x00,
/*-- J  --*/
0x00,0x00,0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x90,0xE0,0x00,
/*-- K  --*/
0x00,0x00,0xEC,0x48,0x50,0x60,0x50,0x50,0x48,0xEC,0x00,0x00,
/*-- L  --*/
0x00,0x00,0xE0,0x40,0x40,0x40,0x40,0x40,0x44,0xFC,0x00,0x00,
/*-- M  --*/
0x00,0x00,0xD8,0xD8,0xD8,0xD8,0xA8,0xA8,0xA8,0xA8,0x00,0x00,
/*-- N  --*/
0x00,0x00,0xDC,0x48,0x68,0x68,0x58,0x58,0x48,0xE8,0x00,0x00,
/*-- O  --*/
0x00,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x00,
/*-- P  --*/
0x00,0x00,0xF0,0x48,0x48,0x70,0x40,0x40,0x40,0xE0,0x00,0x00,
/*-- Q  --*/
0x00,0x00,0x70,0x88,0x88,0x88,0x88,0xE8,0x98,0x70,0x18,0x00,
/*-- R  --*/
0x00,0x00,0xF0,0x48,0x48,0x70,0x50,0x48,0x48,0xEC,0x00,0x00,
/*-- S  --*/
0x00,0x00,0x78,0x88,0x80,0x60,0x10,0x08,0x88,0xF0,0x00,0x00,
/*-- T  --*/
0x00,0x00,0xF8,0xA8,0x20,0x20,0x20,0x20,0x20,0x70,0x00,0x00,
/*-- U  --*/
0x00,0x00,0xCC,0x48,0x48,0x48,0x48,0x48,0x48,0x30,0x00,0x00,
/*-- V  --*/
0x00,0x00,0xCC,0x48,0x48,0x50,0x50,0x30,0x20,0x20,0x00,0x00,
/*-- W  --*/
0x00,0x00,0xA8,0xA8,0xA8,0x70,0x50,0x50,0x50,0x50,0x00,0x00,
/*-- X  --*/
0x00,0x00,0xD8,0x50,0x50,0x20,0x20,0x50,0x50,0xD8,0x00,0x00,
/*-- Y  --*/
0x00,0x00,0xD8,0x50,0x50,0x20,0x20,0x20,0x20,0x70,0x00,0x00,
/*-- Z  --*/
0x00,0x00,0xF8,0x90,0x10,0x20,0x20,0x40,0x48,0xF8,0x00,0x00,
/*-- [  --*/
0x00,0x38,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x38,0x00,
/*-- \  --*/
0x00,0x40,0x40,0x40,0x20,0x20,0x10,0x10,0x10,0x08,0x00,0x00,
/*-- ]  --*/
0x00,0x70,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x70,0x00,
/*-- ^  --*/
0x00,0x20,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- _  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFC,
/*-- `  --*/
0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
/*-- a  --*/
0x00,0x00,0x00,0x00,0x00,0x30,0x48,0x38,0x48,0x3C,0x00,0x00,
/*-- b  --*/
0x00,0x00,0xC0,0x40,0x40,0x70,0x48,0x48,0x48,0x70,0x00,0x00,
/*-- c  --*/
0x00,0x00,0x00,0x00,0x00,0x38,0x48,0x40,0x40,0x38,0x00,0x00,
/*-- d  --*/
0x00,0x00,0x18,0x08,0x08,0x38,0x48,0x48,0x48,0x3C,0x00,0x00,
/*-- e  --*/
0x00,0x00,0x00,0x00,0x00,0x30,0x48,0x78,0x40,0x38,0x00,0x00,
/*-- f  --*/
0x00,0x00,0x1C,0x20,0x20,0x78,0x20,0x20,0x20,0x78,0x00,0x00,
/*-- g  --*/
0x00,0x00,0x00,0x00,0x00,0x3C,0x48,0x30,0x40,0x78,0x44,0x38,
/*-- h  --*/
0x00,0x00,0xC0,0x40,0x40,0x70,0x48,0x48,0x48,0xEC,0x00,0x00,
/*-- i  --*/
0x00,0x00,0x20,0x00,0x00,0x60,0x20,0x20,0x20,0x70,0x00,0x00,
/*-- j  --*/
0x00,0x00,0x10,0x00,0x00,0x30,0x10,0x10,0x10,0x10,0x10,0xE0,
/*-- k  --*/
0x00,0x00,0xC0,0x40,0x40,0x5C,0x50,0x70,0x48,0xEC,0x00,0x00,
/*-- l  --*/
0x00,0x00,0xE0,0x20,0x20,0x20,0x20,0x20,0x20,0xF8,0x00,0x00,
/*-- m  --*/
0x00,0x00,0x00,0x00,0x00,0xF0,0xA8,0xA8,0xA8,0xA8,0x00,0x00,
/*-- n  --*/
0x00,0x00,0x00,0x00,0x00,0xF0,0x48,0x48,0x48,0xEC,0x00,0x00,
/*-- o  --*/
0x00,0x00,0x00,0x00,0x00,0x30,0x48,0x48,0x48,0x30,0x00,0x00,
/*-- p  --*/
0x00,0x00,0x00,0x00,0x00,0xF0,0x48,0x48,0x48,0x70,0x40,0xE0,
/*-- q  --*/
0x00,0x00,0x00,0x00,0x00,0x38,0x48,0x48,0x48,0x38,0x08,0x1C,
/*-- r  --*/
0x00,0x00,0x00,0x00,0x00,0xD8,0x60,0x40,0x40,0xE0,0x00,0x00,
/*-- s  --*/
0x00,0x00,0x00,0x00,0x00,0x78,0x40,0x30,0x08,0x78,0x00,0x00,
/*-- t  --*/
0x00,0x00,0x00,0x20,0x20,0x70,0x20,0x20,0x20,0x18,0x00,0x00,
/*-- u  --*/
0x00,0x00,0x00,0x00,0x00,0xD8,0x48,0x48,0x48,0x3C,0x00,0x00,
/*-- v  --*/
0x00,0x00,0x00,0x00,0x00,0xEC,0x48,0x50,0x30,0x20,0x00,0x00,
/*-- w  --*/
0x00,0x00,0x00,0x00,0x00,0xA8,0xA8,0x70,0x50,0x50,0x00,0x00,
/*-- x  --*/
0x00,0x00,0x00,0x00,0x00,0xD8,0x50,0x20,0x50,0xD8,0x00,0x00,
/*-- y  --*/
0x00,0x00,0x00,0x00,0x00,0xEC,0x48,0x50,0x30,0x20,0x20,0xC0,
/*-- z  --*/
0x00,0x00,0x00,0x00,0x00,0x78,0x10,0x20,0x20,0x78,0x00,0x00,
/*-- {  --*/
0x00,0x18,0x10,0x10,0x10,0x20,0x10,0x10,0x10,0x10,0x18,0x00,
/*-- |  --*/
0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
/*-- }  --*/
0x00,0x60,0x20,0x20,0x20,0x10,0x20,0x20,0x20,0x20,0x60,0x00,
/*-- ~  --*/
0x40,0xA4,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
