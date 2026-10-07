// DOS platform layer - Borland C/C++ 2.0-3.1 runtime compatibility.
//
// Include this instead of <dos.h>, <conio.h>, <io.h>, <alloc.h>, <mem.h>,
// <fcntl.h> and <sys\stat.h>. It provides the Borland library functions and
// keywords DOS programs of the era use, implemented on the emulated machine:
//
//   far/near/huge/interrupt          keywords (expand to nothing)
//   MK_FP FP_SEG FP_OFF peek poke    real mode pointers into emulated memory
//   inportb outportb inport outport  emulated I/O ports
//   getvect setvect geninterrupt     interrupt vector table, BIOS/DOS/mouse
//   _AX.._DX _SI _DI _ES _DS int86   pseudo registers
//   disable enable                   the interrupt flag
//   bioskey getch kbhit clrscr       keyboard / console
//   open read write close ...        DOS files (resolved by dos::fs)
//   farmalloc farfree movedata ...   memory
//   itoa ltoa stricmp ...            string helpers
//
// Programs also get `main` renamed to dos_main (the platform owns the real
// entry point, like SDL does) and `exit` routed through the platform so the
// program thread unwinds cleanly.
#pragma once

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dos/machine.hpp"

//=========================================================================
// Keywords
//=========================================================================

#define far
#define near
#define huge
#define interrupt
#define cdecl
#define pascal
#define _cdecl
#define _pascal

//=========================================================================
// Pointers and memory
//=========================================================================

#define MK_FP(seg, ofs) (static_cast<void*>(::dos::real_ptr(static_cast<std::uint16_t>(seg), static_cast<std::uint16_t>(ofs))))

// Only meaningful for pointers into emulated memory (made with MK_FP).
inline unsigned FP_SEG(const void* p) noexcept
{
    return ::dos::is_emulated(p) ? (::dos::linear_of(p) >> 4) : 0u;
}

inline unsigned FP_OFF(const void* p) noexcept
{
    return ::dos::is_emulated(p) ? (::dos::linear_of(p) & 0x0Fu) : 0u;
}

inline int peek(unsigned seg, unsigned off) noexcept
{
    return static_cast<std::int16_t>(::dos::peek16(::dos::real_ptr(static_cast<std::uint16_t>(seg), static_cast<std::uint16_t>(off))));
}

inline char peekb(unsigned seg, unsigned off) noexcept
{
    return static_cast<char>(::dos::peek8(::dos::real_ptr(static_cast<std::uint16_t>(seg), static_cast<std::uint16_t>(off))));
}

inline void poke(unsigned seg, unsigned off, int value) noexcept
{
    ::dos::poke16(::dos::real_ptr(static_cast<std::uint16_t>(seg), static_cast<std::uint16_t>(off)), static_cast<std::uint16_t>(value));
}

inline void pokeb(unsigned seg, unsigned off, char value) noexcept
{
    ::dos::poke8(::dos::real_ptr(static_cast<std::uint16_t>(seg), static_cast<std::uint16_t>(off)), static_cast<std::uint8_t>(value));
}

void movedata(unsigned srcseg, unsigned srcoff, unsigned dstseg, unsigned dstoff, std::size_t n);

// far memory helpers: route video memory through the card
inline void* _fmemcpy(void* dst, const void* src, std::size_t n) noexcept { ::dos::copy(dst, src, n); return dst; }
inline void* _fmemmove(void* dst, const void* src, std::size_t n) noexcept { ::dos::copy(dst, src, n); return dst; }
inline void* _fmemset(void* dst, int v, std::size_t n) noexcept { ::dos::fill(dst, static_cast<std::uint8_t>(v), n); return dst; }

void* farmalloc(unsigned long size);
void* farcalloc(unsigned long n, unsigned long size);
void* farrealloc(void* block, unsigned long size);
void farfree(void* block);
unsigned long farcoreleft();
unsigned coreleft();

//=========================================================================
// Ports and interrupts
//=========================================================================

inline unsigned char inportb(int port) { return ::dos::inb(static_cast<std::uint16_t>(port)); }
inline int inport(int port) { return ::dos::inw(static_cast<std::uint16_t>(port)); }
inline void outportb(int port, unsigned char v) { ::dos::outb(static_cast<std::uint16_t>(port), v); }
inline void outport(int port, int v) { ::dos::outw(static_cast<std::uint16_t>(port), static_cast<std::uint16_t>(v)); }

inline void disable() { ::dos::cli(); }
inline void enable() { ::dos::sti(); }

inline ::dos::isr getvect(int n) noexcept { return ::dos::get_vector(n); }
inline void setvect(int n, ::dos::isr handler) noexcept { ::dos::set_vector(n, handler); }
inline void geninterrupt(int n) { ::dos::software_interrupt(n); }

#define _AX (::dos::regs().a.x)
#define _BX (::dos::regs().b.x)
#define _CX (::dos::regs().c.x)
#define _DX (::dos::regs().d.x)
#define _AL (::dos::regs().a.b[0])
#define _AH (::dos::regs().a.b[1])
#define _BL (::dos::regs().b.b[0])
#define _BH (::dos::regs().b.b[1])
#define _CL (::dos::regs().c.b[0])
#define _CH (::dos::regs().c.b[1])
#define _DL (::dos::regs().d.b[0])
#define _DH (::dos::regs().d.b[1])
#define _SI (::dos::regs().si)
#define _DI (::dos::regs().di)
#define _BP (::dos::regs().bp)
#define _ES (::dos::regs().es)
#define _DS (::dos::regs().ds)
#define _FLAGS (::dos::regs().flags)

struct WORDREGS {
    std::uint16_t ax, bx, cx, dx, si, di, cflag, flags;
};

struct BYTEREGS {
    std::uint8_t al, ah, bl, bh, cl, ch, dl, dh;
};

union REGS {
    struct WORDREGS x;
    struct BYTEREGS h;
};

struct SREGS {
    std::uint16_t es, cs, ss, ds;
};

int int86(int intno, union REGS* in, union REGS* out);
int int86x(int intno, union REGS* in, union REGS* out, struct SREGS* seg);

//=========================================================================
// Speaker and time
//=========================================================================

void delay(unsigned milliseconds);
void sound(unsigned frequency);
void nosound();

//=========================================================================
// Keyboard and console
//=========================================================================

inline int bioskey(int cmd) { return ::dos::bios_key(cmd); }
int getch();
int getche();
int kbhit();
void clrscr();
void gotoxy(int x, int y);
int dos_puts(const char* s);
#define puts dos_puts

//=========================================================================
// Files (io.h / fcntl.h / sys\stat.h)
//=========================================================================

#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_TRUNC
#undef O_EXCL
#undef O_APPEND
#undef O_TEXT
#undef O_BINARY
#undef S_IREAD
#undef S_IWRITE
#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0004
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_EXCL 0x0400
#define O_APPEND 0x0800
#define O_TEXT 0x4000
#define O_BINARY 0x8000
#define S_IREAD 0x0100
#define S_IWRITE 0x0080

int dos_open(const char* path, int access, ...);
int dos_creat(const char* path, int mode);
int dos_read(int handle, void* buf, unsigned len);
int dos_write(int handle, const void* buf, unsigned len);
int dos_close(int handle);
long dos_lseek(int handle, long offset, int fromwhere);
long dos_tell(int handle);
int dos_eof(int handle);
long dos_filelength(int handle);
int dos_access(const char* path, int mode);
int dos_unlink(const char* path);

#define open dos_open
#define creat dos_creat
#define read dos_read
#define write dos_write
#define close dos_close
#define lseek dos_lseek
#define tell dos_tell
#define eof dos_eof
#define filelength dos_filelength
#define access dos_access
#define unlink dos_unlink

//=========================================================================
// Strings (stdlib.h / string.h extensions)
//=========================================================================

// Borland's toupper/tolower return any value outside 'a'-'z' / 'A'-'Z'
// unchanged (code like toupper(bioskey(0)) relies on it); the C library
// ones are undefined for values outside unsigned char.
inline int dos_toupper(int c) noexcept { return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; }
inline int dos_tolower(int c) noexcept { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
#define toupper dos_toupper
#define tolower dos_tolower

char* dos_itoa(int value, char* str, int radix);
char* dos_ltoa(long value, char* str, int radix);
char* dos_ultoa(unsigned long value, char* str, int radix);
int dos_stricmp(const char* a, const char* b);
int dos_strnicmp(const char* a, const char* b, std::size_t n);
char* dos_strupr(char* s);
char* dos_strlwr(char* s);

#define itoa dos_itoa
#define ltoa dos_ltoa
#define ultoa dos_ultoa
#define stricmp dos_stricmp
#define strcmpi dos_stricmp
#define strnicmp dos_strnicmp
#define strupr dos_strupr
#define strlwr dos_strlwr

//=========================================================================
// Program
//=========================================================================

extern int _argc;
extern char** _argv;

[[noreturn]] void dos_exit(int code);
#define exit dos_exit

#define main dos_main
