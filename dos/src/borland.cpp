// DOS platform layer - Borland C runtime functions.

#include "dos/borland.h"

#undef toupper  // the host versions below, on unsigned char values
#undef tolower

#include "internal.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <string>

int _argc = 0;
char** _argv = nullptr;

namespace dos {
void set_program_args(int argc, char** argv)
{
    _argc = argc;
    _argv = argv;
}
}  // namespace dos

//=========================================================================
// Memory
//=========================================================================

void movedata(unsigned srcseg, unsigned srcoff, unsigned dstseg, unsigned dstoff, std::size_t n)
{
    dos::copy(dos::real_ptr(static_cast<std::uint16_t>(dstseg), static_cast<std::uint16_t>(dstoff)),
              dos::real_ptr(static_cast<std::uint16_t>(srcseg), static_cast<std::uint16_t>(srcoff)), n);
}

namespace {
// DOS programs sometimes read memory right after freeing it (the DOS heap
// leaves it untouched until it is allocated again), so freed blocks are
// kept for a while before going back to the host allocator.
std::deque<void*> g_freed;
std::mutex g_heap_lock;
}  // namespace

void* farmalloc(unsigned long size)
{
    return std::malloc(size ? size : 1);
}

void* farcalloc(unsigned long n, unsigned long size)
{
    return std::calloc(n ? n : 1, size ? size : 1);
}

void* farrealloc(void* block, unsigned long size)
{
    return std::realloc(block, size ? size : 1);
}

void farfree(void* block)
{
    if (!block)
        return;
    std::lock_guard lock(g_heap_lock);
    g_freed.push_back(block);
    while (g_freed.size() > 8) {
        std::free(g_freed.front());
        g_freed.pop_front();
    }
}

unsigned long farcoreleft() { return 500ul * 1024ul; }
unsigned coreleft() { return 60000u; }

//=========================================================================
// Interrupts
//=========================================================================

int int86(int intno, union REGS* in, union REGS* out)
{
    dos::Regs& r = dos::regs();
    r.a.x = in->x.ax;
    r.b.x = in->x.bx;
    r.c.x = in->x.cx;
    r.d.x = in->x.dx;
    r.si = in->x.si;
    r.di = in->x.di;
    r.carry = false;
    dos::software_interrupt(intno);
    out->x.ax = r.a.x;
    out->x.bx = r.b.x;
    out->x.cx = r.c.x;
    out->x.dx = r.d.x;
    out->x.si = r.si;
    out->x.di = r.di;
    out->x.cflag = r.carry ? 1 : 0;
    out->x.flags = r.flags;
    return out->x.ax;
}

int int86x(int intno, union REGS* in, union REGS* out, struct SREGS* seg)
{
    dos::regs().es = seg->es;
    dos::regs().ds = seg->ds;
    const int ax = int86(intno, in, out);
    seg->es = dos::regs().es;
    seg->ds = dos::regs().ds;
    return ax;
}

//=========================================================================
// Speaker and time
//=========================================================================

void delay(unsigned milliseconds)
{
    const double end = dos::now() + milliseconds / 1000.0;
    while (dos::now() < end) {
        dos::poll();
        dos::sleep_until(std::min(end, dos::now() + 0.005));
    }
}

void sound(unsigned frequency)
{
    if (frequency < 19)
        frequency = 19;
    const unsigned divisor = static_cast<unsigned>(dos::Pit::kClock / frequency);
    outportb(0x43, 0xB6);
    outportb(0x42, static_cast<unsigned char>(divisor));
    outportb(0x42, static_cast<unsigned char>(divisor >> 8));
    outportb(0x61, static_cast<unsigned char>(inportb(0x61) | 3));
}

void nosound() { outportb(0x61, static_cast<unsigned char>(inportb(0x61) & ~3)); }

//=========================================================================
// Keyboard and console
//=========================================================================

namespace {
int g_getch_pending = -1;
}

int getch()
{
    if (g_getch_pending >= 0) {
        const int c = g_getch_pending;
        g_getch_pending = -1;
        return c;
    }
    const int key = dos::bios_key(0);
    if ((key & 0xFF) == 0) {
        g_getch_pending = (key >> 8) & 0xFF;
        return 0;
    }
    return key & 0xFF;
}

int getche()
{
    const int c = getch();
    if (c) {
        const char ch = static_cast<char>(c);
        dos::console_write(std::string_view(&ch, 1));
    }
    return c;
}

int kbhit() { return g_getch_pending >= 0 || dos::bios_key(1) != 0; }

void clrscr()
{
    if (dos::vga().mode() <= 3) {
        for (std::uint32_t a = 0xB8000; a < 0xB8000 + 80 * 25 * 2; a += 2) {
            dos::poke8(dos::linear(a), ' ');
            dos::poke8(dos::linear(a + 1), 0x07);
        }
    }
    dos::console_clear();
}

void gotoxy(int, int) {}

int dos_puts(const char* s)
{
    if (!s)
        s = "";
    std::fputs(s, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
    dos::console_write(s);
    dos::console_write("\n");
    return 0;
}

//=========================================================================
// Files
//=========================================================================

namespace {

constexpr int kFirstHandle = 5;  // 0-4 are the DOS standard devices
std::vector<std::FILE*> g_files;

std::FILE* file(int handle)
{
    const int i = handle - kFirstHandle;
    if (i < 0 || i >= static_cast<int>(g_files.size()))
        return nullptr;
    return g_files[i];
}

int add_file(std::FILE* f)
{
    for (std::size_t i = 0; i < g_files.size(); ++i)
        if (!g_files[i]) {
            g_files[i] = f;
            return static_cast<int>(i) + kFirstHandle;
        }
    g_files.push_back(f);
    return static_cast<int>(g_files.size()) - 1 + kFirstHandle;
}

std::FILE* fopen_path(const std::filesystem::path& p, const char* mode)
{
#if defined(_WIN32)
    std::wstring wmode(mode, mode + std::strlen(mode));
    return _wfopen(p.c_str(), wmode.c_str());
#else
    return std::fopen(p.c_str(), mode);
#endif
}

}  // namespace

int dos_open(const char* path, int access, ...)
{
    if (!path)
        return -1;
    const int acc = access & 7;
    const bool writing = acc == O_WRONLY || acc == O_RDWR || (access & (O_CREAT | O_TRUNC | O_APPEND));
    std::FILE* f = nullptr;
    if (!writing) {
        f = fopen_path(dos::fs::resolve_read(path), "rb");
    } else {
        // Files are updated in the write directory; start from the data copy.
        const auto target = dos::fs::resolve_write(path);
        std::error_code ec;
        const bool exists = std::filesystem::is_regular_file(target, ec);
        if (!exists && !(access & O_TRUNC) && dos::fs::exists(path)) {
            std::filesystem::copy_file(dos::fs::resolve_read(path), target, ec);
        }
        const bool now_exists = std::filesystem::is_regular_file(target, ec);
        if ((access & O_EXCL) && (access & O_CREAT) && now_exists)
            return -1;
        if (access & O_TRUNC)
            f = fopen_path(target, acc == O_RDWR ? "w+b" : "wb");
        else if (now_exists)
            f = fopen_path(target, "r+b");
        else if (access & O_CREAT)
            f = fopen_path(target, "w+b");
        if (f && (access & O_APPEND))
            std::fseek(f, 0, SEEK_END);
    }
    if (!f)
        return -1;
    return add_file(f);
}

int dos_creat(const char* path, int) { return dos_open(path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY); }

int dos_read(int handle, void* buf, unsigned len)
{
    std::FILE* f = file(handle);
    if (!f)
        return -1;
    if (dos::is_emulated(buf)) {
        std::vector<std::uint8_t> tmp(len);
        const std::size_t n = std::fread(tmp.data(), 1, len, f);
        dos::copy(buf, tmp.data(), n);
        return static_cast<int>(n);
    }
    std::fseek(f, 0, SEEK_CUR);  // switch between reading and writing
    return static_cast<int>(std::fread(buf, 1, len, f));
}

int dos_write(int handle, const void* buf, unsigned len)
{
    std::FILE* f = file(handle);
    if (!f)
        return -1;
    std::fseek(f, 0, SEEK_CUR);
    if (dos::is_emulated(buf)) {
        std::vector<std::uint8_t> tmp(len);
        dos::copy(tmp.data(), buf, len);
        return static_cast<int>(std::fwrite(tmp.data(), 1, len, f));
    }
    return static_cast<int>(std::fwrite(buf, 1, len, f));
}

int dos_close(int handle)
{
    std::FILE* f = file(handle);
    if (!f)
        return -1;
    std::fclose(f);
    g_files[handle - kFirstHandle] = nullptr;
    return 0;
}

long dos_lseek(int handle, long offset, int fromwhere)
{
    std::FILE* f = file(handle);
    if (!f || std::fseek(f, offset, fromwhere))
        return -1;
    return std::ftell(f);
}

long dos_tell(int handle)
{
    std::FILE* f = file(handle);
    return f ? std::ftell(f) : -1;
}

int dos_eof(int handle)
{
    std::FILE* f = file(handle);
    if (!f)
        return -1;
    const long pos = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    const long end = std::ftell(f);
    std::fseek(f, pos, SEEK_SET);
    return pos >= end ? 1 : 0;
}

long dos_filelength(int handle)
{
    std::FILE* f = file(handle);
    if (!f)
        return -1;
    const long pos = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    const long end = std::ftell(f);
    std::fseek(f, pos, SEEK_SET);
    return end;
}

int dos_access(const char* path, int mode)
{
    if (!path || !dos::fs::exists(path))
        return -1;
    (void)mode;
    return 0;
}

int dos_unlink(const char* path)
{
    if (!path)
        return -1;
    std::error_code ec;
    return std::filesystem::remove(dos::fs::resolve_write(path), ec) ? 0 : -1;
}

//=========================================================================
// Strings
//=========================================================================

char* dos_ultoa(unsigned long value, char* str, int radix)
{
    if (radix < 2 || radix > 36) {
        str[0] = 0;
        return str;
    }
    char tmp[40];
    int n = 0;
    do {
        const int d = static_cast<int>(value % static_cast<unsigned long>(radix));
        tmp[n++] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
        value /= static_cast<unsigned long>(radix);
    } while (value);
    for (int i = 0; i < n; ++i)
        str[i] = tmp[n - 1 - i];
    str[n] = 0;
    return str;
}

char* dos_ltoa(long value, char* str, int radix)
{
    if (radix == 10 && value < 0) {
        str[0] = '-';
        dos_ultoa(static_cast<unsigned long>(-(value + 1)) + 1ul, str + 1, radix);
        return str;
    }
    // other radixes print the 32 bit two's complement, like Borland
    return dos_ultoa(static_cast<std::uint32_t>(value), str, radix);
}

char* dos_itoa(int value, char* str, int radix)
{
    if (radix == 10)
        return dos_ltoa(value, str, radix);
    return dos_ultoa(static_cast<std::uint16_t>(value), str, radix);  // 16 bit int
}

// NULL compares like an empty string: on DOS reading through a null pointer
// just reads the interrupt table, which is what argv[argc] == NULL hits.
int dos_stricmp(const char* a, const char* b)
{
    if (!a) a = "";
    if (!b) b = "";
    for (;; ++a, ++b) {
        const int ca = std::tolower(static_cast<unsigned char>(*a));
        const int cb = std::tolower(static_cast<unsigned char>(*b));
        if (ca != cb || !ca)
            return ca - cb;
    }
}

int dos_strnicmp(const char* a, const char* b, std::size_t n)
{
    if (!a) a = "";
    if (!b) b = "";
    for (std::size_t i = 0; i < n; ++i, ++a, ++b) {
        const int ca = std::tolower(static_cast<unsigned char>(*a));
        const int cb = std::tolower(static_cast<unsigned char>(*b));
        if (ca != cb || !ca)
            return ca - cb;
    }
    return 0;
}

char* dos_strupr(char* s)
{
    for (char* p = s; *p; ++p)
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    return s;
}

char* dos_strlwr(char* s)
{
    for (char* p = s; *p; ++p)
        *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    return s;
}

//=========================================================================
// Program
//=========================================================================

void dos_exit(int code)
{
    std::fflush(stdout);
    dos::exit_program(code);
}
