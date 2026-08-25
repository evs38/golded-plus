//  This may look like C code, but it is really -*- C++ -*-

//  ------------------------------------------------------------------
//  The Goldware Library
//  Copyright (C) 1990-1999 Odinn Sorensen
//  Copyright (C) 1999-2000 Alexander S. Aganichev
//  ------------------------------------------------------------------
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this program; if not, write to the Free
//  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//  MA 02111-1307, USA
//  ------------------------------------------------------------------
//  $Id$
//  ------------------------------------------------------------------
//  Get operating system version.
//  ------------------------------------------------------------------
//  x86_64 port notes:
//    * The old hand-written 32-bit inline assembler (pushfl/popfl, %eax
//      register names, "=m" operands) cannot be assembled in 64-bit mode.
//      All CPU identification now goes through a single portable helper
//      built on <cpuid.h> (GCC/Clang) or __cpuidex() (MSVC).
//    * Extended family/model fields (CPUID.1:EAX[27:20] and [19:16]) are
//      now decoded, otherwise every modern CPU is reported as an
//      Athlon 64 or a Pentium III.
//    * Unknown/modern CPUs fall back to the CPUID brand string
//      (leaves 0x80000002..0x80000004), compacted to fit the name buffer.
//    * Non-x86 targets (aarch64, riscv64, ppc64le...) leave the name
//      empty so that ggetosstring() uses uname().machine instead.
//    * All sprintf() calls are bounded (snprintf).
//  ------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <cstddef>
#include <gstrall.h>
#include <gutlmisc.h>

#if defined(__WIN32__) || defined(_WIN32)
    #include <windows.h>
#elif defined(__GNUC__) || (defined(__WATCOMC__) && defined(__LINUX__))
    #include <sys/utsname.h>
#endif

#if defined(__BEOS__)
    #include <File.h>
    #include <AppFileInfo.h>
#endif


//  ------------------------------------------------------------------
//  Architecture detection
//  ------------------------------------------------------------------

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
    #define GX86_64 1
#endif

#if defined(__i386__) || defined(__i386) || defined(_M_IX86) || defined(__X86__)
    #define GX86_32 1
#endif

#if defined(GX86_64) || defined(GX86_32)
    #define GX86 1
#endif

//  MSVC intrinsics / GCC-Clang cpuid header

#if defined(GX86) && defined(_MSC_VER)
  //  <intrin.h> and the __cpuid/__cpuidex intrinsics arrived with Visual
  //  C++ 2005. Visual C++ 6.0 has neither, but it targets 32-bit x86 only
  //  and its inline assembler knows the instruction, so the older compiler
  //  gets the same thing spelled out by hand - see gcpuid_msvc() below.
  #if (_MSC_VER >= 1400)
    #include <intrin.h>
  #endif
#elif defined(GX86) && (defined(__GNUC__) || defined(__clang__))
    #include <cpuid.h>
#endif


//  ------------------------------------------------------------------

//  MSVC before 2015 has no C99 snprintf.
#if defined(_MSC_VER) && (_MSC_VER < 1900) && !defined(snprintf)
    #define snprintf _snprintf
#endif

//  ------------------------------------------------------------------

#define _MAX_VNAME_LEN  12
#define _MAX_MNAME_LEN  30

//  Character used to join the words of a CPU name taken from the CPUID
//  brand string ("Linux 6.8.0 Intel Core i7-9750H"). Define it as '_'
//  before including to keep the processor field a single word in the
//  resulting OS string.
#ifndef GCFG_CPUNAME_SEP
    #define GCFG_CPUNAME_SEP  ' '
#endif

#ifdef GCFG_NO_CPUID
# define gcpuid(pstr) (pstr)
# define HaveCPUID()  (0)
#else

//  ------------------------------------------------------------------
//  Portable CPUID access.
//
//  gcpuid_max()  - highest supported leaf in the given range
//                  (0 = standard, 0x80000000 = extended);
//                  returns 0 when CPUID is not usable at all.
//  gcpuid_call() - execute CPUID/CPUIDEX, false if the leaf is
//                  not supported.
//  ------------------------------------------------------------------

#if defined(GX86)

//  One CPUID call, whichever way this compiler can make it.

#if defined(_MSC_VER)

static void gcpuid_msvc(int regs[4], int leaf, int subleaf)
{
  #if (_MSC_VER >= 1400)

    __cpuidex(regs, leaf, subleaf);

  #else

    //  Visual C++ 6.0. ebx and esi belong to the caller, so save them
    //  around the instruction; nothing here is an alternative token, so
    //  the macro dance the EFLAGS probe below needs is not repeated.
    __asm
    {
        push    ebx
        push    esi
        mov     eax, leaf
        mov     ecx, subleaf
        cpuid
        mov     esi, regs
        mov     [esi],      eax
        mov     [esi + 4],  ebx
        mov     [esi + 8],  ecx
        mov     [esi + 12], edx
        pop     esi
        pop     ebx
    }

  #endif
}

#endif


static unsigned gcpuid_max(unsigned range)
{
#if defined(_MSC_VER)

  #if defined(GX86_32)
    //  On a 386 the ID flag (EFLAGS bit 21) cannot be toggled and
    //  the CPUID instruction is absent.
    //
    //  'and', 'xor', 'or' and 'not' are instruction mnemonics inside the
    //  block below, but they are also C++ alternative tokens, and MSVC
    //  supplies them as macros - the compiler would see '&&' in the
    //  middle of the assembly and stop with "bad token". Make them plain
    //  words again for the length of the block.
    #pragma push_macro("and")
    #pragma push_macro("or")
    #pragma push_macro("xor")
    #pragma push_macro("not")
    #undef and
    #undef or
    #undef xor
    #undef not

    int has_id = 0;
    __asm
    {
        pushfd
        pop     eax
        mov     ecx, eax
        xor     eax, 0x00200000
        push    eax
        popfd
        pushfd
        pop     eax
        xor     eax, ecx
        and     eax, 0x00200000
        mov     has_id, eax
        push    ecx
        popfd
    }

    #pragma pop_macro("not")
    #pragma pop_macro("xor")
    #pragma pop_macro("or")
    #pragma pop_macro("and")

    if(!has_id)
        return 0;
  #endif

    int regs[4];
    gcpuid_msvc(regs, (int)range, 0);
    unsigned maxleaf = (unsigned)regs[0];
    //  Sanity check: the returned value must belong to the range asked for.
    if((maxleaf & 0x80000000u) != (range & 0x80000000u))
        return 0;
    return maxleaf;

#elif defined(__GNUC__) || defined(__clang__)

    //  __get_cpuid_max() also performs the EFLAGS.ID check on i386
    //  and is PIC-safe (it saves/restores %ebx itself).
    return __get_cpuid_max(range, 0);

#else

    (void)range;
    return 0;

#endif
}


static bool gcpuid_call(unsigned leaf, unsigned subleaf, unsigned regs[4])
{
    regs[0] = regs[1] = regs[2] = regs[3] = 0;

    unsigned maxleaf = gcpuid_max(leaf & 0x80000000u);
    if(!maxleaf || leaf > maxleaf)
        return false;

#if defined(_MSC_VER)

    int r[4];
    gcpuid_msvc(r, (int)leaf, (int)subleaf);
    regs[0] = (unsigned)r[0];
    regs[1] = (unsigned)r[1];
    regs[2] = (unsigned)r[2];
    regs[3] = (unsigned)r[3];
    return true;

#elif defined(__GNUC__) || defined(__clang__)

    __cpuid_count(leaf, subleaf, regs[0], regs[1], regs[2], regs[3]);
    return true;

#else

    (void)subleaf;
    return false;

#endif
}

#endif // GX86


//  ------------------------------------------------------------------

#if defined(__GNUC__)
__attribute__((unused))
#endif
inline static bool HaveCPUID()
{
#if defined(GX86)
    return gcpuid_max(0) != 0;
#else
    return false;
#endif
}


//  ------------------------------------------------------------------
//  Compact the CPUID brand string into something that fits into
//  _MAX_MNAME_LEN bytes, e.g.
//    "Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz" -> "Intel_Core_i7-9750H"
//    "AMD Ryzen 7 5800X 8-Core Processor"       -> "AMD_Ryzen_7_5800X"
//  ------------------------------------------------------------------

#if defined(GX86)

//  Remove every occurrence of "what" from "s" (case sensitive).

static void gvstrdel(char *s, const char *what)
{
    size_t len = strlen(what);
    if(!len)
        return;

    char *p;
    while((p = strstr(s, what)) != NULL)
        memmove(p, p + len, strlen(p + len) + 1);
}


static bool brand_token_is_noise(const char *tok, size_t len)
{
    static const char *noise[] =
    {
        "CPU", "Processor", "processor", "Genuine", "Technologies",
        "Technology", "Inc", "Corp", "Family", "Dual-Core", "Quad-Core",
        "Six-Core", "Eight-Core", "Core(TM)2", "version", "Version", 0
    };

    if(!len)
        return true;

    for(int i = 0; noise[i]; i++)
        if((strlen(noise[i]) == len) && !strncmp(noise[i], tok, len))
            return true;

    //  "8-Core", "16-Core", ...
    if((len > 5) && !strncmp(tok + len - 5, "-Core", 5))
        return true;

    //  A bare clock speed - "3.00GHz", "800MHz" (the "@ x.xxGHz" form is
    //  already cut off earlier).
    if((len > 3) && (!strncmp(tok + len - 3, "GHz", 3) || !strncmp(tok + len - 3, "MHz", 3)))
        return true;

    return false;
}


static bool brand_compact(char *brand, char *dest, size_t size)
{
    if(!size)
        return false;

    *dest = NUL;

    //  Strip the (R)/(TM) marks.
    gvstrdel(brand, "(R)");
    gvstrdel(brand, "(r)");
    gvstrdel(brand, "(TM)");
    gvstrdel(brand, "(tm)");

    //  Cut off the clock speed ("... @ 2.60GHz") and the errata/feature
    //  list that hypervisors append to the model name, e.g. QEMU's
    //  "Intel Core Processor (Haswell, no TSX, IBRS)". Everything from
    //  the first comma on is noise.
    char *cut = strchr(brand, '@');
    if(cut)
        *cut = NUL;

    cut = strchr(brand, ',');
    if(cut)
        *cut = NUL;

    //  Parentheses become token separators, so that the codename inside
    //  them survives: "Core Processor (Haswell" -> "Core" + "Haswell".
    for(char *q = brand; *q; q++)
        if((*q == '(') || (*q == ')') || (*q == '\t'))
            *q = ' ';

    //  Tokenize on whitespace, dropping noise words.
    size_t out = 0;
    const char *p = brand;

    while(*p)
    {
        while(*p == ' ')
            p++;

        if(!*p)
            break;

        const char *tok = p;
        while(*p && (*p != ' '))
            p++;

        size_t len = (size_t)(p - tok);

        if(brand_token_is_noise(tok, len))
            continue;

        //  "with Radeon Graphics" and everything after it is dropped.
        if((len == 4) && !strncmp(tok, "with", 4))
            break;

        //  A token is taken whole or not at all - never cut a word in
        //  half and never leave a trailing separator behind. The only
        //  exception is a first token that is longer than the whole
        //  buffer: a truncated name still beats an empty one.
        size_t need = len + (out ? 1 : 0);
        if(out + need >= size)
        {
            if(out)
                break;
            len = size - 1;
        }

        if(out)
            dest[out++] = GCFG_CPUNAME_SEP;

        memcpy(dest + out, tok, len);
        out += len;
    }

    dest[out] = NUL;

    return out != 0;
}


static bool compact_brandstring(char *dest, size_t size)
{
    unsigned regs[4];
    char brand[3 * 4 * sizeof(unsigned) + 1];
    size_t n = 0;

    if(gcpuid_max(0x80000000u) < 0x80000004u)
        return false;

    for(unsigned leaf = 0x80000002u; leaf <= 0x80000004u; leaf++)
    {
        if(!gcpuid_call(leaf, 0, regs))
            return false;

        for(int r = 0; r < 4; r++)
            for(int b = 0; b < 4; b++)
                brand[n++] = (char)((regs[r] >> (8 * b)) & 0xFF);
    }
    brand[n] = NUL;

    return brand_compact(brand, dest, size);
}

#endif // GX86


//  ------------------------------------------------------------------
//  Build a short CPU name from vendor/family/model.
//  Returns true when the CPU was recognized exactly, false when only
//  a generic F<n>M<n> name could be produced (the caller then tries
//  the brand string instead).
//  ------------------------------------------------------------------

static bool cpuname(unsigned family, unsigned model, const char *v_name, char *m_name, size_t size)
{
    bool known = true;

    if(!strcmp("AuthenticAMD", v_name) || !strcmp("HygonGenuine", v_name))
    {
        switch (family)
        {
        case 4:
            switch (model)
            {
            case 3:
            case 7:
                strxcpy(m_name, "AMD486DX2", size);
                break;
            case 8:
            case 9:
                strxcpy(m_name, "AMD486DX4", size);
                break;
            case 14:
            case 15:
                strxcpy(m_name, "AMD5x86", size);
                break;
            default:
                snprintf(m_name, size, "AMD486_M%u", model);
            }
            break;

        case 5:
            switch (model)
            {
            case 0:
            case 1:
            case 2:
            case 3:
                strxcpy(m_name, "AMD_K5", size);
                break;
            case 6:
            case 7:
                strxcpy(m_name, "AMD_K6", size);
                break;
            case 8:
                strxcpy(m_name, "AMD_K6-2", size);
                break;
            case 9:
            case 10:
            case 11:
            case 12:
                strxcpy(m_name, "AMD_K6-3", size);
                break;
            case 13:
            case 14:
            case 15:
                strxcpy(m_name, "AMD_K6-3+", size);
                break;
            default:
                snprintf(m_name, size, "AMD_F%uM%u", family, model);
            }
            break;

        case 6:
            /* need full F/M/S/Rev to tell Athlon/Duron/Sempron apart */
            snprintf(m_name, size, "AMD_K7_M%u", model);
            break;

        case 15:
            snprintf(m_name, size, "AMD_K8_M%u", model);
            break;

        //  Everything below needs the extended family field.
        case 0x10:
            snprintf(m_name, size, "AMD_K10_M%u", model);
            break;
        case 0x11:
            snprintf(m_name, size, "AMD_K8L_M%u", model);
            break;
        case 0x12:
            snprintf(m_name, size, "AMD_Llano_M%u", model);
            break;
        case 0x14:
            snprintf(m_name, size, "AMD_Bobcat_M%u", model);
            break;
        case 0x15:
            snprintf(m_name, size, "AMD_Bulldozer_M%u", model);
            break;
        case 0x16:
            snprintf(m_name, size, "AMD_Jaguar_M%u", model);
            break;
        case 0x17:
            snprintf(m_name, size, "AMD_Zen_M%u", model);
            break;
        case 0x18:
            snprintf(m_name, size, "Hygon_M%u", model);
            break;
        case 0x19:
            snprintf(m_name, size, "AMD_Zen3_M%u", model);
            break;
        case 0x1A:
            snprintf(m_name, size, "AMD_Zen5_M%u", model);
            break;

        default:
            snprintf(m_name, size, "AMD_F%uM%u", family, model);
            known = false;
        }
    }
    else if(!strcmp("GenuineIntel", v_name))
    {
        switch (family)
        {
        case 4:
            switch (model)
            {
            case 0:
            case 1:
                strxcpy(m_name, "i486DX", size);
                break;
            case 2:
                strxcpy(m_name, "i486SX", size);
                break;
            case 3:
                strxcpy(m_name, "i486DX2", size);
                break;
            case 4:
                strxcpy(m_name, "i486SL", size);
                break;
            case 5:
                strxcpy(m_name, "i486SX2O", size);
                break;
            case 7:
                strxcpy(m_name, "i486DX2E", size);
                break;
            case 8:
                strxcpy(m_name, "i486DX4", size);
                break;
            default:
                snprintf(m_name, size, "i486_M%u", model);
            }
            break;

        case 5:
            switch (model)
            {
            case 1:
                strxcpy(m_name, "iP", size);
                break;
            case 2:
                strxcpy(m_name, "iP54C", size);
                break;
            case 3:
                strxcpy(m_name, "iP_OverDrive", size);
                break;
            case 4:
                strxcpy(m_name, "iP55C", size);
                break;
            default:
                snprintf(m_name, size, "iF%uM%u", family, model);
            }
            break;

        case 6:
            switch (model)
            {
            case 1:
                strxcpy(m_name, "iP-Pro", size);
                break;
            case 3:
            case 5:
                strxcpy(m_name, "iP-II", size);
                break;
            case 6:
                strxcpy(m_name, "iCeleron", size);
                break;
            case 7:
            case 8:
            case 11:
                strxcpy(m_name, "iP-III", size);
                break;
            case 9:
            case 13:
                strxcpy(m_name, "iP-M", size);  // Pentium M "Centrino"
                break;
            default:
                //  Family 6 model >= 14 is Core/Core2/Nehalem/.../Raptor Lake.
                //  There are far too many of them to table - the brand
                //  string is used instead.
                snprintf(m_name, size, "iF%uM%u", family, model);
                known = false;
            }
            break;

        case 15:
            strxcpy(m_name, "iP-IV", size);
            break;

        default:
            snprintf(m_name, size, "iF%uM%u", family, model);
            known = false;
        }
    }
    else if(!strcmp("GenuineTMx86", v_name) || !strcmp("TransmetaCPU", v_name))
    {
        if((family == 15) && (model == 2))
            strxcpy(m_name, "TM8000", size);    // Transmeta Efficeon TM8000
        else
            snprintf(m_name, size, "TM F%uM%u", family, model);
    }
    else if(!strcmp("CyrixInstead", v_name))
        snprintf(m_name, size, "CyrF%uM%u", family, model);
    else if(!strcmp("CentaurHauls", v_name))
    {
        switch (family)
        {
        case 6:  //  VIA C3 Nehemiah = F6M9; VIA C3 Samuel 2 = F6M7
            strxcpy(m_name, "VIA_C3", size);
            break;
        case 7:
            strxcpy(m_name, "VIA_Zhaoxin", size);
            break;
        default:
            snprintf(m_name, size, "VIA F%uM%u", family, model);
            known = false;
        }
    }
    else if(!strcmp("KVMKVMKVM", v_name) || !strcmp("TCGTCGTCGTCG", v_name)
            || !strcmp("VMwareVMware", v_name) || !strcmp("XenVMMXenVMM", v_name)
            || !strcmp("Microsoft Hv", v_name))
    {
        //  Hypervisor vendor leaf leaked into leaf 0 (rare, but happens).
        snprintf(m_name, size, "VM F%uM%u", family, model);
        known = false;
    }
    else
    {
        known = false;

        if(model)
        {
            snprintf(m_name, size, "CPU %.12s-F%uM%u", v_name, family, model);
        }
        else
        {
            switch (family)
            {
            case 0:
                snprintf(m_name, size, "CPU %.12s", v_name);
                break;
            case 3:
            case 4:
                snprintf(m_name, size, "%.12s%s%u86", v_name, v_name[0] ? "-" : "", family);
                break;
            default:
                snprintf(m_name, size, "CPU %.12s-F%uM%u", v_name, family, model);
            }
        }
    }

    return known;
}


//  ------------------------------------------------------------------
//  Fill _cpuname (a buffer of _MAX_MNAME_LEN bytes) with a short
//  processor description. On non-x86 targets the buffer is left empty
//  so that the caller can fall back to uname().machine.
//  ------------------------------------------------------------------

char *gcpuid(char *_cpuname)
{
    *_cpuname = NUL;

#if defined(GX86)

    unsigned regs[4];

    if(!gcpuid_call(0, 0, regs))
    {
        //  No CPUID at all: a 386 or a very early 486.
    #if defined(GX86_32)
        cpuname(0, 0, "x86", _cpuname, _MAX_MNAME_LEN);
    #else
        strxcpy(_cpuname, "x86_64", _MAX_MNAME_LEN);
    #endif
        return _cpuname;
    }

    //  Vendor string: EBX, EDX, ECX (12 bytes + terminator).
    char vendor[_MAX_VNAME_LEN + 1];
    unsigned vreg[3];
    vreg[0] = regs[1];
    vreg[1] = regs[3];
    vreg[2] = regs[2];

    for(int r = 0; r < 3; r++)
        for(int b = 0; b < 4; b++)
            vendor[r * 4 + b] = (char)((vreg[r] >> (8 * b)) & 0xFF);

    vendor[_MAX_VNAME_LEN] = NUL;

    unsigned family = 0, model = 0;

    if(gcpuid_call(1, 0, regs))
    {
        unsigned eax = regs[0];

        family = (eax >> 8) & 0x0F;
        model  = (eax >> 4) & 0x0F;
        //  stepping = eax & 0x0F;   -- not used for the name

        //  Extended family/model (Intel SDM vol.2, CPUID.EAX=1):
        //  DisplayFamily = Family + ExtFamily          when Family == 0x0F
        //  DisplayModel  = (ExtModel << 4) + Model     when Family == 0x06 or 0x0F
        if(family == 0x0F)
            family += (eax >> 20) & 0xFF;

        if((family == 0x06) || (family >= 0x0F))
            model += ((eax >> 16) & 0x0F) << 4;
    }

    if(!cpuname(family, model, vendor, _cpuname, _MAX_MNAME_LEN))
    {
        //  Unknown/modern CPU - the marketing name is far more useful.
        char brand[_MAX_MNAME_LEN];

        if(compact_brandstring(brand, sizeof(brand)))
            strxcpy(_cpuname, brand, _MAX_MNAME_LEN);
    }

#elif defined(MSDOS) || defined(DOS) || defined(__MSDOS__) || defined(_DOS) \
   || defined(WIN32) || defined(__WIN32__) || defined(_WIN) || defined(WINNT) \
   || defined(__OS2__) || defined(OS2)

    cpuname(0, 0, "x86", _cpuname, _MAX_MNAME_LEN);

#else

    //  aarch64, riscv64, ppc64le, ... - let uname() name the machine.
    //  (An empty string tells ggetosstring() to use info.machine.)

#endif

    return _cpuname;
}

#endif // ifdef GCFG_NO_CPUID

//  ------------------------------------------------------------------

char* ggetosstring(void)
{

    static char osstring[256] = "";

    if(*osstring == NUL)
    {

        char processor[_MAX_MNAME_LEN] = "";

#if defined(__UNIX__) || defined(__DJGPP__) || defined(__EMX__)

        struct utsname info;

        (void)gcpuid(processor);

        if(uname(&info) != -1)
        {
            if(!processor[0])
                strxcpy(processor, info.machine, sizeof(processor));

#if defined(__EMX__)
            snprintf(osstring, sizeof(osstring), "%s %s.%s %s", info.sysname, info.version, info.release, processor);
#elif defined(__DJGPP__)
            snprintf(osstring, sizeof(osstring), "%s %s.%s %s", info.sysname, info.release, info.version, processor);
#elif defined(__BEOS__)
            BAppFileInfo appFileInfo;
            version_info sys_ver = {0};
            BFile file("/boot/beos/system/lib/libbe.so", B_READ_ONLY);
            appFileInfo.SetTo(&file);
            appFileInfo.GetVersionInfo(&sys_ver, B_APP_VERSION_KIND);
            snprintf(osstring, sizeof(osstring), "%s %s %s", info.sysname, sys_ver.short_info, processor);
#else
            snprintf(osstring, sizeof(osstring), "%s %s %s", info.sysname, info.release, processor);
#endif
        }
        else
            strxcpy(osstring, "unknown", sizeof(osstring));

#elif defined(__WIN32__) || defined(_WIN32)

        OSVERSIONINFO info;
        SYSTEM_INFO si;
        char ostype[16];

        //  GetNativeSystemInfo() reports the real architecture for a
        //  32-bit binary running under WOW64; it exists on every OS
        //  that can run a 64-bit CPU, so resolve it dynamically.
        typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
        PGNSI pGNSI = (PGNSI)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
        if(pGNSI)
            pGNSI(&si);
        else
            GetSystemInfo(&si);

        memset(&info, 0, sizeof(info));
        info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)  // GetVersionEx is deprecated since Win8.1 SDK
#endif
        if(GetVersionEx(&info))
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        {
            switch(info.dwPlatformId)
            {
            case VER_PLATFORM_WIN32_NT:
                strxcpy(ostype, "WinNT", sizeof(ostype));
                break;
            case VER_PLATFORM_WIN32_WINDOWS:
                strxcpy(ostype, "Win9x", sizeof(ostype));
                break;
            default:
                strxcpy(ostype, "Win32s", sizeof(ostype));
                break;
            }

            //  The original code read the architecture through
            //  *((WORD*)&si), which breaks strict aliasing; the field
            //  is right there.
            //  Borland C++ 5.02's <winbase.h> predates the named union
            //  member and carries only dwOemId; the architecture is its
            //  low word, which is what the original code read.
#if defined(__BORLANDC__) && (__BORLANDC__ < 0x0550)
            const WORD gsi_arch = (WORD)(si.dwOemId & 0xFFFF);
#else
            const WORD gsi_arch = si.wProcessorArchitecture;
#endif
            switch(gsi_arch)
            {
            case PROCESSOR_ARCHITECTURE_INTEL:
            {
                if(HaveCPUID())
                {
                    gcpuid(processor);
                }
                else
                {
                    int cpu;
                    if( (info.dwPlatformId == VER_PLATFORM_WIN32_NT &&
                            info.dwMajorVersion > 3)
#ifdef VER_PLATFORM_WIN32_CE
                            || info.dwPlatformId == VER_PLATFORM_WIN32_CE
#endif
                      )
                        cpu = si.wProcessorLevel;
                    else
                    {
                        switch(si.dwProcessorType)   /* Windows NT 3.5 and earlier */
                        {
                        case PROCESSOR_INTEL_386:
                            cpu = 3;
                            break;
                        case PROCESSOR_INTEL_486:
                            cpu = 4;
                            break;
                        case PROCESSOR_INTEL_PENTIUM:
                            cpu = 5;
                            break;
                        case 6:   /* Pentium Pro or Pentium II */
                            cpu = 6;
                            break;    // <- fall-through bug in the original
                        case 15:  /* Pentium 4 */
                            cpu = 8;
                            break;    // <- fall-through bug in the original
                        default:
                            cpu = 7;
                            break;
                        }
                    }
                    switch(cpu)
                    {
                    case 15:
                        snprintf(processor, sizeof(processor), "i886");
                        break;
                    default:
                        if( cpu>9 ) cpu = cpu%10 + int(cpu/10) + 2;
                        snprintf(processor, sizeof(processor), "i%d86", cpu);
                    }
                }
            }
            break;
#ifdef PROCESSOR_ARCHITECTURE_IA64
            case PROCESSOR_ARCHITECTURE_IA64:
                snprintf(processor, sizeof(processor), "IA64-%u", unsigned(si.wProcessorLevel));
                break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_AMD64
            case PROCESSOR_ARCHITECTURE_AMD64:
                if(HaveCPUID())
                    gcpuid(processor);
                if(!processor[0])
                    snprintf(processor, sizeof(processor), "AMD64-%u", unsigned(si.wProcessorLevel));
                break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM64
            case PROCESSOR_ARCHITECTURE_ARM64:
                snprintf(processor, sizeof(processor), "ARM64-%u", unsigned(si.wProcessorLevel));
                break;
#endif
            case PROCESSOR_ARCHITECTURE_MIPS:
                /* si.wProcessorLevel is of the form 00xx, where xx is an 8-bit
                   implementation number (bits 8-15 of the PRId register). */
                snprintf(processor, sizeof(processor), "MIPS R%u000", unsigned(si.wProcessorLevel));
                break;
            case PROCESSOR_ARCHITECTURE_ALPHA:
                /* si.wProcessorLevel is of the form xxxx, where xxxx is a 16-bit
                   processor version number (the low-order 16 bits of a version
                   number from the firmware). */
                snprintf(processor, sizeof(processor), "Alpha%u", unsigned(si.wProcessorLevel));
                break;
#ifdef PROCESSOR_ARCHITECTURE_ALPHA64
            case PROCESSOR_ARCHITECTURE_ALPHA64:
                snprintf(processor, sizeof(processor), "Alpha%u", unsigned(si.wProcessorLevel));
                break;
#endif
            case PROCESSOR_ARCHITECTURE_PPC:
                /* si.wProcessorLevel is of the form xxxx, where xxxx is a 16-bit
                   processor version number (the high-order 16 bits of the Processor
                   Version Register). */
                switch(si.wProcessorLevel)
                {
                case 1:
                    strxcpy(processor, "PPC601", sizeof(processor));
                    break;
                case 3:
                    strxcpy(processor, "PPC603", sizeof(processor));
                    break;
                case 4:
                    strxcpy(processor, "PPC604", sizeof(processor));
                    break;
                case 6:
                    strxcpy(processor, "PPC603+", sizeof(processor));
                    break;
                case 9:
                    strxcpy(processor, "PPC604+", sizeof(processor));
                    break;
                case 20:
                    strxcpy(processor, "PPC620", sizeof(processor));
                    break;
                default:
                    snprintf(processor, sizeof(processor), "PPC l%u", unsigned(si.wProcessorLevel));
                    break;
                }
                break;
#ifdef PROCESSOR_ARCHITECTURE_SHX
            case PROCESSOR_ARCHITECTURE_SHX:
                snprintf(processor, sizeof(processor), "SH-%u", unsigned(si.wProcessorLevel));
                break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM
            case PROCESSOR_ARCHITECTURE_ARM:
                snprintf(processor, sizeof(processor), "ARM-%u", unsigned(si.wProcessorLevel));
                break;
#endif
            default:
                strxcpy(processor, "CPU-unknown", sizeof(processor));
                break;
            }

            if(info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
                info.dwBuildNumber = info.dwBuildNumber & 0x0000ffffl;

            if(info.dwPlatformId == VER_PLATFORM_WIN32_NT && *info.szCSDVersion != NUL)
            {
                char _tmp[128];
                strxcpy(_tmp, info.szCSDVersion, sizeof(_tmp));
                strchg(_tmp, ' ', '_');
                strisrep(_tmp, "Service_Pack_", "SP");
                snprintf(osstring, sizeof(osstring), "%s %u.%u.%u-%s %s", ostype,
                         unsigned(info.dwMajorVersion), unsigned(info.dwMinorVersion),
                         unsigned(info.dwBuildNumber), _tmp, processor);
            }
            else
                snprintf(osstring, sizeof(osstring), "%s %u.%u.%u %s", ostype,
                         unsigned(info.dwMajorVersion), unsigned(info.dwMinorVersion),
                         unsigned(info.dwBuildNumber), processor);
        }
        else
            strxcpy(osstring, "Win32-unknown", sizeof(osstring));

#elif defined(__MSDOS__) || defined(__OS2__)

#if defined(__MSDOS__)
        const char* osname = "DOS";
#else
        const char* osname = "OS/2";
#endif

        snprintf(osstring, sizeof(osstring), "%s %d.%02d %s", osname, _osmajor, _osminor, gcpuid(processor));

#else

        //  Unknown OS - at least report the CPU.
        (void)gcpuid(processor);
        snprintf(osstring, sizeof(osstring), "unknown %s", processor[0] ? processor : "CPU");

#endif
    }

    return osstring;
}


// -------------------------------------------------------------------
