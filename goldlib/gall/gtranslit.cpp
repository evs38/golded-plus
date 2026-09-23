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
//  Transliteration - see gtranslit.h.
//  ------------------------------------------------------------------

#include <cstring>
#include <cstdlib>
#include <cctype>
#include <string>
#include <vector>
#include <gdefs.h>
#include <gstrall.h>
#include <gctype.h>
#include <gutf8.h>
#include <gtranslit.h>

#if defined(GOLD_ICU)
    #include <unicode/utrans.h>
    #include <unicode/ustring.h>
    #define GOLD_ICU_ANY 1
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #define GOLD_ICU_DYNAMIC 1
    #define GOLD_ICU_ANY 1
#elif defined(__WIN32__) || defined(_WIN32)
    #include <windows.h>
    #define GOLD_ICU_DYNAMIC 1
    #define GOLD_ICU_ANY 1
#endif


//  ------------------------------------------------------------------
//  The rules asked for, and what answers.

static std::string translit_rules   = "Russian-Latin/BGN";
static bool        translit_on      = true;
static bool        translit_table_only = false;
static std::string translit_engine_name;


//  ------------------------------------------------------------------
//  ICU. With GOLD_ICU the headers and the link line come from the
//  build; on macOS and Windows the system's own library is opened at
//  run time - libicucore.dylib and icu.dll export the C entry points
//  under their plain names, without the version suffix the headers
//  would add - so nothing is linked and nothing has to be shipped.

#if defined(GOLD_ICU_DYNAMIC)

typedef uint16_t GUChar;
typedef int      GUErrorCode;
typedef int      GUTransDirection;
struct  GUTransliterator;
struct  GUParseError
{
    int32_t line;
    int32_t offset;
    GUChar  preContext[16];
    GUChar  postContext[16];
};

#if defined(_WIN32)
    #define GICU_CALL __cdecl
#else
    #define GICU_CALL
#endif

typedef GUTransliterator* (GICU_CALL *gicu_utrans_openU_t)(const GUChar*, int32_t, GUTransDirection, const GUChar*, int32_t, GUParseError*, GUErrorCode*);
typedef void   (GICU_CALL *gicu_utrans_transUChars_t)(const GUTransliterator*, GUChar*, int32_t*, int32_t, int32_t, int32_t*, GUErrorCode*);
typedef void   (GICU_CALL *gicu_utrans_close_t)(GUTransliterator*);
typedef GUChar* (GICU_CALL *gicu_u_strFromUTF8_t)(GUChar*, int32_t, int32_t*, const char*, int32_t, GUErrorCode*);
typedef char*  (GICU_CALL *gicu_u_strToUTF8_t)(char*, int32_t, int32_t*, const GUChar*, int32_t, GUErrorCode*);

static gicu_utrans_openU_t       p_utrans_openU       = NULL;
static gicu_utrans_transUChars_t p_utrans_transUChars = NULL;
static gicu_utrans_close_t       p_utrans_close       = NULL;
static gicu_u_strFromUTF8_t      p_u_strFromUTF8      = NULL;
static gicu_u_strToUTF8_t        p_u_strToUTF8        = NULL;
static int  icu_dyn_state = 0;      // 0 untried, 1 loaded, -1 absent

static void* icu_symbol(void* lib, const char* name)
{
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

static bool icu_dynamic_load()
{
    if(icu_dyn_state != 0)
        return icu_dyn_state > 0;
    icu_dyn_state = -1;
#if defined(_WIN32)
    void* lib = (void*)LoadLibraryA("icu.dll");
#else
    void* lib = dlopen("/usr/lib/libicucore.dylib", RTLD_LAZY | RTLD_LOCAL);
#endif
    if(lib == NULL)
        return false;
    p_utrans_openU       = (gicu_utrans_openU_t)icu_symbol(lib, "utrans_openU");
    p_utrans_transUChars = (gicu_utrans_transUChars_t)icu_symbol(lib, "utrans_transUChars");
    p_utrans_close       = (gicu_utrans_close_t)icu_symbol(lib, "utrans_close");
    p_u_strFromUTF8      = (gicu_u_strFromUTF8_t)icu_symbol(lib, "u_strFromUTF8");
    p_u_strToUTF8        = (gicu_u_strToUTF8_t)icu_symbol(lib, "u_strToUTF8");
    if(p_utrans_openU and p_utrans_transUChars and p_utrans_close and p_u_strFromUTF8 and p_u_strToUTF8)
        icu_dyn_state = 1;
    return icu_dyn_state > 0;
}

#define GICU_UTRANS_FORWARD 0
#define GICU_ZERO_ERROR     0
#define GICU_BUFFER_OVERFLOW_ERROR 15

#elif defined(GOLD_ICU)

typedef UChar          GUChar;
typedef UErrorCode     GUErrorCode;
typedef UTransDirection GUTransDirection;
typedef UTransliterator GUTransliterator;
typedef UParseError    GUParseError;
#define p_utrans_openU       utrans_openU
#define p_utrans_transUChars utrans_transUChars
#define p_utrans_close       utrans_close
#define p_u_strFromUTF8      u_strFromUTF8
#define p_u_strToUTF8        u_strToUTF8
#define GICU_UTRANS_FORWARD  UTRANS_FORWARD
#define GICU_ZERO_ERROR      U_ZERO_ERROR
#define GICU_BUFFER_OVERFLOW_ERROR U_BUFFER_OVERFLOW_ERROR
static bool icu_dynamic_load()
{
    return true;
}

#endif


#if defined(GOLD_ICU_ANY)

static GUTransliterator* icu_trans_latin = NULL;    // rules; Any-Latin
static GUTransliterator* icu_trans_ascii = NULL;    // rules; Any-Latin; Latin-ASCII
static int  icu_state = 0;                          // 0 untried, 1 ready, -1 no
static bool icu_rules_unknown = false;

static GUTransliterator* icu_open(const std::string& id)
{
    std::vector<GUChar> uid(id.length() + 1);
    for(size_t n = 0; n < id.length(); n++)
        uid[n] = (GUChar)(unsigned char)id[n];
    uid[id.length()] = 0;
    GUParseError pe;
    GUErrorCode  status = GICU_ZERO_ERROR;
    GUTransliterator* t = p_utrans_openU(&uid[0], (int32_t)id.length(), (GUTransDirection)GICU_UTRANS_FORWARD, NULL, 0, &pe, &status);
    if(status > GICU_ZERO_ERROR)
    {
        if(t)
            p_utrans_close(t);
        return NULL;
    }
    return t;
}

static void icu_close_all()
{
    if(icu_trans_latin)
        p_utrans_close(icu_trans_latin);
    if(icu_trans_ascii)
        p_utrans_close(icu_trans_ascii);
    icu_trans_latin = icu_trans_ascii = NULL;
    icu_state = 0;
    icu_rules_unknown = false;
}

static bool icu_ready()
{
    if(icu_state != 0)
        return icu_state > 0;
    icu_state = -1;
    if(translit_table_only or not icu_dynamic_load())
        return false;

    //  The user's rules first, then whatever script is left over into
    //  Latin; the ASCII form strips the diacritics after that.
    std::string chain = translit_rules.empty() ? std::string("Any-Latin") : translit_rules + "; Any-Latin";
    icu_trans_latin = icu_open(chain);
    if(icu_trans_latin == NULL and not translit_rules.empty())
    {
        //  A rule set ICU does not know: say so in the engine's name
        //  and go on with the general one rather than with nothing.
        icu_rules_unknown = true;
        chain = "Any-Latin";
        icu_trans_latin = icu_open(chain);
    }
    if(icu_trans_latin == NULL)
        return false;
    icu_trans_ascii = icu_open(chain + "; Latin-ASCII");
    if(icu_trans_ascii == NULL)
    {
        icu_close_all();
        icu_state = -1;
        return false;
    }
    icu_state = 1;
    return true;
}

static bool icu_translit(const char* utf8, size_t len, bool ascii, std::string& out)
{
    if(not icu_ready())
        return false;

    GUErrorCode status = GICU_ZERO_ERROR;
    int32_t ulen = 0;
    std::vector<GUChar> ubuf(len + 16);
    p_u_strFromUTF8(&ubuf[0], (int32_t)ubuf.size(), &ulen, utf8, (int32_t)len, &status);
    if(status > GICU_ZERO_ERROR)
        return false;

    //  Transliterated text grows - four letters for one - so give it
    //  room, and more when ICU says that was not enough.
    for(int32_t cap = ulen * 8 + 64; cap < (1 << 24); cap *= 2)
    {
        std::vector<GUChar> tbuf(cap);
        memcpy(&tbuf[0], &ubuf[0], ulen * sizeof(GUChar));
        int32_t tlen  = ulen;
        int32_t limit = ulen;
        status = GICU_ZERO_ERROR;
        p_utrans_transUChars(ascii ? icu_trans_ascii : icu_trans_latin, &tbuf[0], &tlen, cap, 0, &limit, &status);
        if(status == GICU_BUFFER_OVERFLOW_ERROR)
            continue;
        if(status > GICU_ZERO_ERROR)
            return false;

        std::vector<char> obuf(tlen * 4 + 16);
        int32_t olen = 0;
        status = GICU_ZERO_ERROR;
        p_u_strToUTF8(&obuf[0], (int32_t)obuf.size(), &olen, &tbuf[0], tlen, &status);
        if(status > GICU_ZERO_ERROR)
            return false;
        out.assign(&obuf[0], (size_t)olen);
        return true;
    }
    return false;
}

#endif  // GOLD_ICU_ANY


//  ------------------------------------------------------------------
//  The built-in table. Cyrillic and Greek after BGN/PCGN as ICU
//  renders them into ASCII, the Latin letters with diacritics from
//  their Unicode decompositions, the kana after Hepburn.

struct translit_entry
{
    uint32_t    cp;
    const char* out;
};

static const translit_entry translit_latin[] =
{
    { 0x00C0, "A" },
    { 0x00C1, "A" },
    { 0x00C2, "A" },
    { 0x00C3, "A" },
    { 0x00C4, "A" },
    { 0x00C5, "A" },
    { 0x00C6, "AE" },
    { 0x00C7, "C" },
    { 0x00C8, "E" },
    { 0x00C9, "E" },
    { 0x00CA, "E" },
    { 0x00CB, "E" },
    { 0x00CC, "I" },
    { 0x00CD, "I" },
    { 0x00CE, "I" },
    { 0x00CF, "I" },
    { 0x00D0, "D" },
    { 0x00D1, "N" },
    { 0x00D2, "O" },
    { 0x00D3, "O" },
    { 0x00D4, "O" },
    { 0x00D5, "O" },
    { 0x00D6, "O" },
    { 0x00D8, "O" },
    { 0x00D9, "U" },
    { 0x00DA, "U" },
    { 0x00DB, "U" },
    { 0x00DC, "U" },
    { 0x00DD, "Y" },
    { 0x00DE, "Th" },
    { 0x00DF, "ss" },
    { 0x00E0, "a" },
    { 0x00E1, "a" },
    { 0x00E2, "a" },
    { 0x00E3, "a" },
    { 0x00E4, "a" },
    { 0x00E5, "a" },
    { 0x00E6, "ae" },
    { 0x00E7, "c" },
    { 0x00E8, "e" },
    { 0x00E9, "e" },
    { 0x00EA, "e" },
    { 0x00EB, "e" },
    { 0x00EC, "i" },
    { 0x00ED, "i" },
    { 0x00EE, "i" },
    { 0x00EF, "i" },
    { 0x00F0, "d" },
    { 0x00F1, "n" },
    { 0x00F2, "o" },
    { 0x00F3, "o" },
    { 0x00F4, "o" },
    { 0x00F5, "o" },
    { 0x00F6, "o" },
    { 0x00F8, "o" },
    { 0x00F9, "u" },
    { 0x00FA, "u" },
    { 0x00FB, "u" },
    { 0x00FC, "u" },
    { 0x00FD, "y" },
    { 0x00FE, "th" },
    { 0x00FF, "y" },
    { 0x0100, "A" },
    { 0x0101, "a" },
    { 0x0102, "A" },
    { 0x0103, "a" },
    { 0x0104, "A" },
    { 0x0105, "a" },
    { 0x0106, "C" },
    { 0x0107, "c" },
    { 0x0108, "C" },
    { 0x0109, "c" },
    { 0x010A, "C" },
    { 0x010B, "c" },
    { 0x010C, "C" },
    { 0x010D, "c" },
    { 0x010E, "D" },
    { 0x010F, "d" },
    { 0x0110, "D" },
    { 0x0111, "d" },
    { 0x0112, "E" },
    { 0x0113, "e" },
    { 0x0114, "E" },
    { 0x0115, "e" },
    { 0x0116, "E" },
    { 0x0117, "e" },
    { 0x0118, "E" },
    { 0x0119, "e" },
    { 0x011A, "E" },
    { 0x011B, "e" },
    { 0x011C, "G" },
    { 0x011D, "g" },
    { 0x011E, "G" },
    { 0x011F, "g" },
    { 0x0120, "G" },
    { 0x0121, "g" },
    { 0x0122, "G" },
    { 0x0123, "g" },
    { 0x0124, "H" },
    { 0x0125, "h" },
    { 0x0126, "H" },
    { 0x0127, "h" },
    { 0x0128, "I" },
    { 0x0129, "i" },
    { 0x012A, "I" },
    { 0x012B, "i" },
    { 0x012C, "I" },
    { 0x012D, "i" },
    { 0x012E, "I" },
    { 0x012F, "i" },
    { 0x0130, "I" },
    { 0x0131, "i" },
    { 0x0134, "J" },
    { 0x0135, "j" },
    { 0x0136, "K" },
    { 0x0137, "k" },
    { 0x0138, "k" },
    { 0x0139, "L" },
    { 0x013A, "l" },
    { 0x013B, "L" },
    { 0x013C, "l" },
    { 0x013D, "L" },
    { 0x013E, "l" },
    { 0x013F, "L" },
    { 0x0140, "l" },
    { 0x0141, "L" },
    { 0x0142, "l" },
    { 0x0143, "N" },
    { 0x0144, "n" },
    { 0x0145, "N" },
    { 0x0146, "n" },
    { 0x0147, "N" },
    { 0x0148, "n" },
    { 0x0149, "'n" },
    { 0x014A, "N" },
    { 0x014B, "n" },
    { 0x014C, "O" },
    { 0x014D, "o" },
    { 0x014E, "O" },
    { 0x014F, "o" },
    { 0x0150, "O" },
    { 0x0151, "o" },
    { 0x0152, "OE" },
    { 0x0153, "oe" },
    { 0x0154, "R" },
    { 0x0155, "r" },
    { 0x0156, "R" },
    { 0x0157, "r" },
    { 0x0158, "R" },
    { 0x0159, "r" },
    { 0x015A, "S" },
    { 0x015B, "s" },
    { 0x015C, "S" },
    { 0x015D, "s" },
    { 0x015E, "S" },
    { 0x015F, "s" },
    { 0x0160, "S" },
    { 0x0161, "s" },
    { 0x0162, "T" },
    { 0x0163, "t" },
    { 0x0164, "T" },
    { 0x0165, "t" },
    { 0x0166, "T" },
    { 0x0167, "t" },
    { 0x0168, "U" },
    { 0x0169, "u" },
    { 0x016A, "U" },
    { 0x016B, "u" },
    { 0x016C, "U" },
    { 0x016D, "u" },
    { 0x016E, "U" },
    { 0x016F, "u" },
    { 0x0170, "U" },
    { 0x0171, "u" },
    { 0x0172, "U" },
    { 0x0173, "u" },
    { 0x0174, "W" },
    { 0x0175, "w" },
    { 0x0176, "Y" },
    { 0x0177, "y" },
    { 0x0178, "Y" },
    { 0x0179, "Z" },
    { 0x017A, "z" },
    { 0x017B, "Z" },
    { 0x017C, "z" },
    { 0x017D, "Z" },
    { 0x017E, "z" },
    { 0x017F, "s" },
    { 0x0180, "b" },
    { 0x0189, "D" },
    { 0x018A, "D" },
    { 0x0197, "I" },
    { 0x01A0, "O" },
    { 0x01A1, "o" },
    { 0x01AF, "U" },
    { 0x01B0, "u" },
    { 0x01B5, "Z" },
    { 0x01B6, "z" },
    { 0x01CD, "A" },
    { 0x01CE, "a" },
    { 0x01CF, "I" },
    { 0x01D0, "i" },
    { 0x01D1, "O" },
    { 0x01D2, "o" },
    { 0x01D3, "U" },
    { 0x01D4, "u" },
    { 0x01D5, "U" },
    { 0x01D6, "u" },
    { 0x01D7, "U" },
    { 0x01D8, "u" },
    { 0x01D9, "U" },
    { 0x01DA, "u" },
    { 0x01DB, "U" },
    { 0x01DC, "u" },
    { 0x01DE, "A" },
    { 0x01DF, "a" },
    { 0x01E0, "A" },
    { 0x01E1, "a" },
    { 0x01E4, "G" },
    { 0x01E5, "g" },
    { 0x01E6, "G" },
    { 0x01E7, "g" },
    { 0x01E8, "K" },
    { 0x01E9, "k" },
    { 0x01EA, "O" },
    { 0x01EB, "o" },
    { 0x01EC, "O" },
    { 0x01ED, "o" },
    { 0x01F0, "j" },
    { 0x01F4, "G" },
    { 0x01F5, "g" },
    { 0x01F8, "N" },
    { 0x01F9, "n" },
    { 0x01FA, "A" },
    { 0x01FB, "a" },
    { 0x0200, "A" },
    { 0x0201, "a" },
    { 0x0202, "A" },
    { 0x0203, "a" },
    { 0x0204, "E" },
    { 0x0205, "e" },
    { 0x0206, "E" },
    { 0x0207, "e" },
    { 0x0208, "I" },
    { 0x0209, "i" },
    { 0x020A, "I" },
    { 0x020B, "i" },
    { 0x020C, "O" },
    { 0x020D, "o" },
    { 0x020E, "O" },
    { 0x020F, "o" },
    { 0x0210, "R" },
    { 0x0211, "r" },
    { 0x0212, "R" },
    { 0x0213, "r" },
    { 0x0214, "U" },
    { 0x0215, "u" },
    { 0x0216, "U" },
    { 0x0217, "u" },
    { 0x0218, "S" },
    { 0x0219, "s" },
    { 0x021A, "T" },
    { 0x021B, "t" },
    { 0x021E, "H" },
    { 0x021F, "h" },
    { 0x0226, "A" },
    { 0x0227, "a" },
    { 0x0228, "E" },
    { 0x0229, "e" },
    { 0x022A, "O" },
    { 0x022B, "o" },
    { 0x022C, "O" },
    { 0x022D, "o" },
    { 0x022E, "O" },
    { 0x022F, "o" },
    { 0x0230, "O" },
    { 0x0231, "o" },
    { 0x0232, "Y" },
    { 0x0233, "y" },
    { 0x1E00, "A" },
    { 0x1E01, "a" },
    { 0x1E02, "B" },
    { 0x1E03, "b" },
    { 0x1E04, "B" },
    { 0x1E05, "b" },
    { 0x1E06, "B" },
    { 0x1E07, "b" },
    { 0x1E08, "C" },
    { 0x1E09, "c" },
    { 0x1E0A, "D" },
    { 0x1E0B, "d" },
    { 0x1E0C, "D" },
    { 0x1E0D, "d" },
    { 0x1E0E, "D" },
    { 0x1E0F, "d" },
    { 0x1E10, "D" },
    { 0x1E11, "d" },
    { 0x1E12, "D" },
    { 0x1E13, "d" },
    { 0x1E14, "E" },
    { 0x1E15, "e" },
    { 0x1E16, "E" },
    { 0x1E17, "e" },
    { 0x1E18, "E" },
    { 0x1E19, "e" },
    { 0x1E1A, "E" },
    { 0x1E1B, "e" },
    { 0x1E1C, "E" },
    { 0x1E1D, "e" },
    { 0x1E1E, "F" },
    { 0x1E1F, "f" },
    { 0x1E20, "G" },
    { 0x1E21, "g" },
    { 0x1E22, "H" },
    { 0x1E23, "h" },
    { 0x1E24, "H" },
    { 0x1E25, "h" },
    { 0x1E26, "H" },
    { 0x1E27, "h" },
    { 0x1E28, "H" },
    { 0x1E29, "h" },
    { 0x1E2A, "H" },
    { 0x1E2B, "h" },
    { 0x1E2C, "I" },
    { 0x1E2D, "i" },
    { 0x1E2E, "I" },
    { 0x1E2F, "i" },
    { 0x1E30, "K" },
    { 0x1E31, "k" },
    { 0x1E32, "K" },
    { 0x1E33, "k" },
    { 0x1E34, "K" },
    { 0x1E35, "k" },
    { 0x1E36, "L" },
    { 0x1E37, "l" },
    { 0x1E38, "L" },
    { 0x1E39, "l" },
    { 0x1E3A, "L" },
    { 0x1E3B, "l" },
    { 0x1E3C, "L" },
    { 0x1E3D, "l" },
    { 0x1E3E, "M" },
    { 0x1E3F, "m" },
    { 0x1E40, "M" },
    { 0x1E41, "m" },
    { 0x1E42, "M" },
    { 0x1E43, "m" },
    { 0x1E44, "N" },
    { 0x1E45, "n" },
    { 0x1E46, "N" },
    { 0x1E47, "n" },
    { 0x1E48, "N" },
    { 0x1E49, "n" },
    { 0x1E4A, "N" },
    { 0x1E4B, "n" },
    { 0x1E4C, "O" },
    { 0x1E4D, "o" },
    { 0x1E4E, "O" },
    { 0x1E4F, "o" },
    { 0x1E50, "O" },
    { 0x1E51, "o" },
    { 0x1E52, "O" },
    { 0x1E53, "o" },
    { 0x1E54, "P" },
    { 0x1E55, "p" },
    { 0x1E56, "P" },
    { 0x1E57, "p" },
    { 0x1E58, "R" },
    { 0x1E59, "r" },
    { 0x1E5A, "R" },
    { 0x1E5B, "r" },
    { 0x1E5C, "R" },
    { 0x1E5D, "r" },
    { 0x1E5E, "R" },
    { 0x1E5F, "r" },
    { 0x1E60, "S" },
    { 0x1E61, "s" },
    { 0x1E62, "S" },
    { 0x1E63, "s" },
    { 0x1E64, "S" },
    { 0x1E65, "s" },
    { 0x1E66, "S" },
    { 0x1E67, "s" },
    { 0x1E68, "S" },
    { 0x1E69, "s" },
    { 0x1E6A, "T" },
    { 0x1E6B, "t" },
    { 0x1E6C, "T" },
    { 0x1E6D, "t" },
    { 0x1E6E, "T" },
    { 0x1E6F, "t" },
    { 0x1E70, "T" },
    { 0x1E71, "t" },
    { 0x1E72, "U" },
    { 0x1E73, "u" },
    { 0x1E74, "U" },
    { 0x1E75, "u" },
    { 0x1E76, "U" },
    { 0x1E77, "u" },
    { 0x1E78, "U" },
    { 0x1E79, "u" },
    { 0x1E7A, "U" },
    { 0x1E7B, "u" },
    { 0x1E7C, "V" },
    { 0x1E7D, "v" },
    { 0x1E7E, "V" },
    { 0x1E7F, "v" },
    { 0x1E80, "W" },
    { 0x1E81, "w" },
    { 0x1E82, "W" },
    { 0x1E83, "w" },
    { 0x1E84, "W" },
    { 0x1E85, "w" },
    { 0x1E86, "W" },
    { 0x1E87, "w" },
    { 0x1E88, "W" },
    { 0x1E89, "w" },
    { 0x1E8A, "X" },
    { 0x1E8B, "x" },
    { 0x1E8C, "X" },
    { 0x1E8D, "x" },
    { 0x1E8E, "Y" },
    { 0x1E8F, "y" },
    { 0x1E90, "Z" },
    { 0x1E91, "z" },
    { 0x1E92, "Z" },
    { 0x1E93, "z" },
    { 0x1E94, "Z" },
    { 0x1E95, "z" },
    { 0x1E96, "h" },
    { 0x1E97, "t" },
    { 0x1E98, "w" },
    { 0x1E99, "y" },
    { 0x1EA0, "A" },
    { 0x1EA1, "a" },
    { 0x1EA2, "A" },
    { 0x1EA3, "a" },
    { 0x1EA4, "A" },
    { 0x1EA5, "a" },
    { 0x1EA6, "A" },
    { 0x1EA7, "a" },
    { 0x1EA8, "A" },
    { 0x1EA9, "a" },
    { 0x1EAA, "A" },
    { 0x1EAB, "a" },
    { 0x1EAC, "A" },
    { 0x1EAD, "a" },
    { 0x1EAE, "A" },
    { 0x1EAF, "a" },
    { 0x1EB0, "A" },
    { 0x1EB1, "a" },
    { 0x1EB2, "A" },
    { 0x1EB3, "a" },
    { 0x1EB4, "A" },
    { 0x1EB5, "a" },
    { 0x1EB6, "A" },
    { 0x1EB7, "a" },
    { 0x1EB8, "E" },
    { 0x1EB9, "e" },
    { 0x1EBA, "E" },
    { 0x1EBB, "e" },
    { 0x1EBC, "E" },
    { 0x1EBD, "e" },
    { 0x1EBE, "E" },
    { 0x1EBF, "e" },
    { 0x1EC0, "E" },
    { 0x1EC1, "e" },
    { 0x1EC2, "E" },
    { 0x1EC3, "e" },
    { 0x1EC4, "E" },
    { 0x1EC5, "e" },
    { 0x1EC6, "E" },
    { 0x1EC7, "e" },
    { 0x1EC8, "I" },
    { 0x1EC9, "i" },
    { 0x1ECA, "I" },
    { 0x1ECB, "i" },
    { 0x1ECC, "O" },
    { 0x1ECD, "o" },
    { 0x1ECE, "O" },
    { 0x1ECF, "o" },
    { 0x1ED0, "O" },
    { 0x1ED1, "o" },
    { 0x1ED2, "O" },
    { 0x1ED3, "o" },
    { 0x1ED4, "O" },
    { 0x1ED5, "o" },
    { 0x1ED6, "O" },
    { 0x1ED7, "o" },
    { 0x1ED8, "O" },
    { 0x1ED9, "o" },
    { 0x1EDA, "O" },
    { 0x1EDB, "o" },
    { 0x1EDC, "O" },
    { 0x1EDD, "o" },
    { 0x1EDE, "O" },
    { 0x1EDF, "o" },
    { 0x1EE0, "O" },
    { 0x1EE1, "o" },
    { 0x1EE2, "O" },
    { 0x1EE3, "o" },
    { 0x1EE4, "U" },
    { 0x1EE5, "u" },
    { 0x1EE6, "U" },
    { 0x1EE7, "u" },
    { 0x1EE8, "U" },
    { 0x1EE9, "u" },
    { 0x1EEA, "U" },
    { 0x1EEB, "u" },
    { 0x1EEC, "U" },
    { 0x1EED, "u" },
    { 0x1EEE, "U" },
    { 0x1EEF, "u" },
    { 0x1EF0, "U" },
    { 0x1EF1, "u" },
    { 0x1EF2, "Y" },
    { 0x1EF3, "y" },
    { 0x1EF4, "Y" },
    { 0x1EF5, "y" },
    { 0x1EF6, "Y" },
    { 0x1EF7, "y" },
    { 0x1EF8, "Y" },
    { 0x1EF9, "y" },
};

static const translit_entry translit_cyrillic[] =
{
    { 0x0430, "a" },
    { 0x0431, "b" },
    { 0x0432, "v" },
    { 0x0433, "g" },
    { 0x0434, "d" },
    { 0x0435, "e" },
    { 0x0436, "zh" },
    { 0x0437, "z" },
    { 0x0438, "i" },
    { 0x0439, "y" },
    { 0x043A, "k" },
    { 0x043B, "l" },
    { 0x043C, "m" },
    { 0x043D, "n" },
    { 0x043E, "o" },
    { 0x043F, "p" },
    { 0x0440, "r" },
    { 0x0441, "s" },
    { 0x0442, "t" },
    { 0x0443, "u" },
    { 0x0444, "f" },
    { 0x0445, "kh" },
    { 0x0446, "ts" },
    { 0x0447, "ch" },
    { 0x0448, "sh" },
    { 0x0449, "shch" },
    { 0x044A, "\"" },
    { 0x044B, "y" },
    { 0x044C, "'" },
    { 0x044D, "e" },
    { 0x044E, "yu" },
    { 0x044F, "ya" },
    { 0x0450, "e" },
    { 0x0451, "e" },
    { 0x0452, "d" },
    { 0x0453, "g" },
    { 0x0454, "e" },
    { 0x0455, "z" },
    { 0x0456, "i" },
    { 0x0457, "i" },
    { 0x0458, "j" },
    { 0x0459, "l" },
    { 0x045A, "n" },
    { 0x045B, "c" },
    { 0x045C, "k" },
    { 0x045D, "i" },
    { 0x045E, "u" },
    { 0x045F, "d" },
    { 0x0463, "e" },
    { 0x0473, "f" },
    { 0x0475, "i" },
    { 0x0479, "u" },
    { 0x0491, "g" },
    { 0x0493, "gh" },
    { 0x049B, "q" },
    { 0x04A3, "ng" },
    { 0x04AB, "s" },
    { 0x04AF, "u" },
    { 0x04B1, "u" },
    { 0x04B3, "kh" },
    { 0x04B7, "j" },
    { 0x04B9, "ch" },
    { 0x04BB, "h" },
    { 0x04D1, "a" },
    { 0x04D3, "a" },
    { 0x04D7, "e" },
    { 0x04D9, "a" },
    { 0x04E3, "i" },
    { 0x04E5, "i" },
    { 0x04E7, "o" },
    { 0x04E9, "o" },
    { 0x04EF, "u" },
    { 0x04F1, "u" },
    { 0x04F3, "u" },
    { 0x04F5, "ch" },
};

static const translit_entry translit_greek[] =
{
    { 0x0390, "i" },
    { 0x03AC, "a" },
    { 0x03AD, "e" },
    { 0x03AE, "e" },
    { 0x03AF, "i" },
    { 0x03B0, "y" },
    { 0x03B1, "a" },
    { 0x03B2, "v" },
    { 0x03B3, "g" },
    { 0x03B4, "d" },
    { 0x03B5, "e" },
    { 0x03B6, "z" },
    { 0x03B7, "e" },
    { 0x03B8, "th" },
    { 0x03B9, "i" },
    { 0x03BA, "k" },
    { 0x03BB, "l" },
    { 0x03BC, "m" },
    { 0x03BD, "n" },
    { 0x03BE, "x" },
    { 0x03BF, "o" },
    { 0x03C0, "p" },
    { 0x03C1, "r" },
    { 0x03C2, "s" },
    { 0x03C3, "s" },
    { 0x03C4, "t" },
    { 0x03C5, "y" },
    { 0x03C6, "f" },
    { 0x03C7, "ch" },
    { 0x03C8, "ps" },
    { 0x03C9, "o" },
    { 0x03CA, "i" },
    { 0x03CB, "y" },
    { 0x03CC, "o" },
    { 0x03CD, "y" },
    { 0x03CE, "o" },
};

static const translit_entry translit_kana[] =
{
    { 0x3041, "a" },
    { 0x3042, "a" },
    { 0x3043, "i" },
    { 0x3044, "i" },
    { 0x3045, "u" },
    { 0x3046, "u" },
    { 0x3047, "e" },
    { 0x3048, "e" },
    { 0x3049, "o" },
    { 0x304A, "o" },
    { 0x304B, "ka" },
    { 0x304C, "ga" },
    { 0x304D, "ki" },
    { 0x304E, "gi" },
    { 0x304F, "ku" },
    { 0x3050, "gu" },
    { 0x3051, "ke" },
    { 0x3052, "ge" },
    { 0x3053, "ko" },
    { 0x3054, "go" },
    { 0x3055, "sa" },
    { 0x3056, "za" },
    { 0x3057, "shi" },
    { 0x3058, "ji" },
    { 0x3059, "su" },
    { 0x305A, "zu" },
    { 0x305B, "se" },
    { 0x305C, "ze" },
    { 0x305D, "so" },
    { 0x305E, "zo" },
    { 0x305F, "ta" },
    { 0x3060, "da" },
    { 0x3061, "chi" },
    { 0x3062, "ji" },
    { 0x3064, "tsu" },
    { 0x3065, "zu" },
    { 0x3066, "te" },
    { 0x3067, "de" },
    { 0x3068, "to" },
    { 0x3069, "do" },
    { 0x306A, "na" },
    { 0x306B, "ni" },
    { 0x306C, "nu" },
    { 0x306D, "ne" },
    { 0x306E, "no" },
    { 0x306F, "ha" },
    { 0x3070, "ba" },
    { 0x3071, "pa" },
    { 0x3072, "hi" },
    { 0x3073, "bi" },
    { 0x3074, "pi" },
    { 0x3075, "fu" },
    { 0x3076, "bu" },
    { 0x3077, "pu" },
    { 0x3078, "he" },
    { 0x3079, "be" },
    { 0x307A, "pe" },
    { 0x307B, "ho" },
    { 0x307C, "bo" },
    { 0x307D, "po" },
    { 0x307E, "ma" },
    { 0x307F, "mi" },
    { 0x3080, "mu" },
    { 0x3081, "me" },
    { 0x3082, "mo" },
    { 0x3083, "ya" },
    { 0x3084, "ya" },
    { 0x3085, "yu" },
    { 0x3086, "yu" },
    { 0x3087, "yo" },
    { 0x3088, "yo" },
    { 0x3089, "ra" },
    { 0x308A, "ri" },
    { 0x308B, "ru" },
    { 0x308C, "re" },
    { 0x308D, "ro" },
    { 0x308F, "wa" },
    { 0x3090, "wi" },
    { 0x3091, "we" },
    { 0x3092, "wo" },
    { 0x3093, "n" },
    { 0x3094, "vu" },
    { 0x30A1, "a" },
    { 0x30A2, "a" },
    { 0x30A3, "i" },
    { 0x30A4, "i" },
    { 0x30A5, "u" },
    { 0x30A6, "u" },
    { 0x30A7, "e" },
    { 0x30A8, "e" },
    { 0x30A9, "o" },
    { 0x30AA, "o" },
    { 0x30AB, "ka" },
    { 0x30AC, "ga" },
    { 0x30AD, "ki" },
    { 0x30AE, "gi" },
    { 0x30AF, "ku" },
    { 0x30B0, "gu" },
    { 0x30B1, "ke" },
    { 0x30B2, "ge" },
    { 0x30B3, "ko" },
    { 0x30B4, "go" },
    { 0x30B5, "sa" },
    { 0x30B6, "za" },
    { 0x30B7, "shi" },
    { 0x30B8, "ji" },
    { 0x30B9, "su" },
    { 0x30BA, "zu" },
    { 0x30BB, "se" },
    { 0x30BC, "ze" },
    { 0x30BD, "so" },
    { 0x30BE, "zo" },
    { 0x30BF, "ta" },
    { 0x30C0, "da" },
    { 0x30C1, "chi" },
    { 0x30C2, "ji" },
    { 0x30C4, "tsu" },
    { 0x30C5, "zu" },
    { 0x30C6, "te" },
    { 0x30C7, "de" },
    { 0x30C8, "to" },
    { 0x30C9, "do" },
    { 0x30CA, "na" },
    { 0x30CB, "ni" },
    { 0x30CC, "nu" },
    { 0x30CD, "ne" },
    { 0x30CE, "no" },
    { 0x30CF, "ha" },
    { 0x30D0, "ba" },
    { 0x30D1, "pa" },
    { 0x30D2, "hi" },
    { 0x30D3, "bi" },
    { 0x30D4, "pi" },
    { 0x30D5, "fu" },
    { 0x30D6, "bu" },
    { 0x30D7, "pu" },
    { 0x30D8, "he" },
    { 0x30D9, "be" },
    { 0x30DA, "pe" },
    { 0x30DB, "ho" },
    { 0x30DC, "bo" },
    { 0x30DD, "po" },
    { 0x30DE, "ma" },
    { 0x30DF, "mi" },
    { 0x30E0, "mu" },
    { 0x30E1, "me" },
    { 0x30E2, "mo" },
    { 0x30E3, "ya" },
    { 0x30E4, "ya" },
    { 0x30E5, "yu" },
    { 0x30E6, "yu" },
    { 0x30E7, "yo" },
    { 0x30E8, "yo" },
    { 0x30E9, "ra" },
    { 0x30EA, "ri" },
    { 0x30EB, "ru" },
    { 0x30EC, "re" },
    { 0x30ED, "ro" },
    { 0x30EF, "wa" },
    { 0x30F0, "wi" },
    { 0x30F1, "we" },
    { 0x30F2, "wo" },
    { 0x30F3, "n" },
    { 0x30F4, "vu" },
    { 0x30F7, "va" },
    { 0x30F8, "vi" },
    { 0x30F9, "ve" },
    { 0x30FA, "vo" },
};


static const char* translit_lookup(const translit_entry* table, size_t count, uint32_t cp)
{
    size_t lo = 0, hi = count;
    while(lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        if(table[mid].cp < cp)
            lo = mid + 1;
        else if(table[mid].cp > cp)
            hi = mid;
        else
            return table[mid].out;
    }
    return NULL;
}

#define TRANSLIT_LOOKUP(t, cp) translit_lookup(t, sizeof(t)/sizeof(t[0]), cp)

//  One code point out of UTF-8; a bad sequence counts as one byte.
static uint32_t translit_decode(const char* p, size_t left, size_t& used)
{
    unsigned char c = (unsigned char)*p;
    int seq = g_utf8_seqlen(c);
    if(seq <= 1 or (size_t)seq > left)
    {
        used = 1;
        return c;
    }
    uint32_t cp = c & (0xFF >> (seq + 1));
    for(int n = 1; n < seq; n++)
    {
        unsigned char cc = (unsigned char)p[n];
        if((cc & 0xC0) != 0x80)
        {
            used = 1;
            return c;
        }
        cp = (cp << 6) | (cc & 0x3F);
    }
    used = (size_t)seq;
    return cp;
}

static bool is_cyrillic(uint32_t cp)
{
    return cp >= 0x0400 and cp <= 0x04FF;
}

//  The lower-case form of a Cyrillic letter; returns whether the
//  letter was upper-case.
static bool cyr_lower(uint32_t cp, uint32_t& lower)
{
    if(cp >= 0x0410 and cp <= 0x042F) { lower = cp + 0x20; return true; }
    if(cp >= 0x0430 and cp <= 0x044F) { lower = cp; return false; }
    if(cp >= 0x0400 and cp <= 0x040F) { lower = cp + 0x50; return true; }
    if(cp >= 0x0450 and cp <= 0x045F) { lower = cp; return false; }
    if(cp >= 0x0460 and cp <= 0x04FF) { lower = cp | 1; return (cp & 1) == 0; }
    lower = cp;
    return false;
}

//  The vowels after which BGN/PCGN writes U+0435/U+0451 as "ye".
static bool cyr_vowel(uint32_t lower)
{
    switch(lower)
    {
    case 0x0430: case 0x0435: case 0x0451: case 0x0438: case 0x043E: case 0x0443:
    case 0x044B: case 0x044D: case 0x044E: case 0x044F: case 0x0456: case 0x0457:
    case 0x0454:
        return true;
    }
    return false;
}

static bool grk_lower(uint32_t cp, uint32_t& lower)
{
    if(cp >= 0x0391 and cp <= 0x03A9 and cp != 0x03A2) { lower = cp + 0x20; return true; }
    if(cp == 0x0386) { lower = 0x03AC; return true; }
    if(cp >= 0x0388 and cp <= 0x038A) { lower = cp + 0x25; return true; }
    if(cp == 0x038C) { lower = 0x03CC; return true; }
    if(cp == 0x038E or cp == 0x038F) { lower = cp + 0x3F; return true; }
    lower = cp;
    return false;
}

static bool lat_upper(uint32_t cp)
{
    if(cp < 0x80)
        return cp >= 'A' and cp <= 'Z';
    if(cp >= 0xC0 and cp <= 0xDE)
        return cp != 0xD7;
    if(cp >= 0x0100 and cp <= 0x017F)
        return (cp & 1) == 0;
    return false;
}

//  Append 'out' (lower-case Latin) in the case of the source letter:
//  capitalised when it was upper-case, all upper when its neighbour
//  is upper-case too - "YEVGENIY", not "YEvGENIY".
static void translit_append(std::string& s, const char* out, bool upper, bool next_upper, bool prev_upper)
{
    if(not upper)
    {
        s += out;
        return;
    }
    bool all = next_upper or prev_upper;
    for(const char* p = out; *p; p++)
    {
        if(p == out or all)
            s += (char)g_toupper(*p);
        else
            s += *p;
    }
}

static bool kana_small_y(uint32_t cp)
{
    return cp == 0x3083 or cp == 0x3085 or cp == 0x3087 or cp == 0x30E3 or cp == 0x30E5 or cp == 0x30E7;
}

static std::string table_translit(const char* utf8, size_t len, bool ascii)
{
    std::string out;
    out.reserve(len * 2);

    //  Decode first, so that the rules can look at neighbours.
    std::vector<uint32_t> cps;
    std::vector<size_t>   offs;
    std::vector<size_t>   lens;
    for(size_t pos = 0; pos < len;)
    {
        size_t used;
        uint32_t cp = translit_decode(utf8 + pos, len - pos, used);
        cps.push_back(cp);
        offs.push_back(pos);
        lens.push_back(used);
        pos += used;
    }

    std::string pending_kana;   // the romaji of the kana before, for a small ya/yu/yo
    bool sokuon = false;        // a small tsu doubles the consonant that follows

    for(size_t i = 0; i < cps.size(); i++)
    {
        uint32_t cp = cps[i];
        uint32_t prev = i ? cps[i-1] : 0;
        uint32_t next = (i + 1 < cps.size()) ? cps[i+1] : 0;

        //  Kana not written yet goes out before anything that is not
        //  a small ya/yu/yo.
        if(not pending_kana.empty() and not kana_small_y(cp))
        {
            out += pending_kana;
            pending_kana.erase();
        }

        if(cp < 0x80)
        {
            out += (char)cp;
            continue;
        }

        uint32_t lower;
        const char* r;

        //  Cyrillic.
        if(is_cyrillic(cp))
        {
            bool upper = cyr_lower(cp, lower);
            r = TRANSLIT_LOOKUP(translit_cyrillic, lower);
            if(r == NULL)
            {
                out.append(utf8 + offs[i], lens[i]);
                continue;
            }
            //  BGN/PCGN: U+0435 and U+0451 are "ye" at the start of a
            //  word and after a vowel, U+0439, U+044C or U+044A, plain
            //  "e" after any other consonant.
            if(lower == 0x0435 or lower == 0x0451)
            {
                uint32_t pl;
                cyr_lower(prev, pl);
                bool word_start = not is_cyrillic(prev);
                if(word_start or cyr_vowel(pl) or pl == 0x0439 or pl == 0x044C or pl == 0x044A)
                    r = "ye";
            }
            uint32_t nl, pl2;
            bool next_upper = is_cyrillic(next) and cyr_lower(next, nl);
            bool prev_upper = is_cyrillic(prev) and cyr_lower(prev, pl2);
            translit_append(out, r, upper, next_upper, prev_upper);
            continue;
        }

        //  Greek.
        if(cp >= 0x0386 and cp <= 0x03CE)
        {
            bool upper = grk_lower(cp, lower);
            r = TRANSLIT_LOOKUP(translit_greek, lower);
            if(r == NULL)
            {
                out.append(utf8 + offs[i], lens[i]);
                continue;
            }
            uint32_t nl, pl2;
            bool next_upper = grk_lower(next, nl);
            bool prev_upper = grk_lower(prev, pl2);
            translit_append(out, r, upper, next_upper, prev_upper);
            continue;
        }

        //  Kana, Hepburn.
        if(cp >= 0x3041 and cp <= 0x30FC)
        {
            if(cp == 0x3063 or cp == 0x30C3)        // small tsu
            {
                sokuon = true;
                continue;
            }
            if(cp == 0x30FC)                         // prolonged sound mark
                continue;
            r = TRANSLIT_LOOKUP(translit_kana, cp);
            if(r == NULL)
            {
                out.append(utf8 + offs[i], lens[i]);
                continue;
            }
            if(kana_small_y(cp) and not pending_kana.empty())
            {
                //  ki + small ya = kya; shi, chi and ji drop the y too.
                std::string base = pending_kana;
                const char* y = r;
                if(base.length() >= 2 and base[base.length()-1] == 'i')
                {
                    base.erase(base.length() - 1);
                    if(base == "sh" or base == "ch" or base == "j")
                        y = r + 1;
                }
                out += base;
                out += y;
                pending_kana.erase();
                continue;
            }
            std::string romaji = r;
            if(sokuon and not romaji.empty())
            {
                //  Visual C++ 6.0 finds insert(0, 1, c) ambiguous.
                romaji = std::string(1, romaji[0]) + romaji;
                sokuon = false;
            }
            pending_kana = romaji;
            continue;
        }

        //  Latin with diacritics: kept for a destination that has them,
        //  reduced to the base letter for one that has not.
        if(ascii)
        {
            r = TRANSLIT_LOOKUP(translit_latin, cp);
            if(r)
            {
                //  The table gives the base letter in its own case
                //  already; only the multi-letter ones need the rule.
                if(strlen(r) > 1 and lat_upper(cp))
                    translit_append(out, r, true, lat_upper(next), lat_upper(prev));
                else
                    out += r;
                continue;
            }
        }

        //  Nothing known: as it came, for the recoder to substitute.
        out.append(utf8 + offs[i], lens[i]);
    }

    if(not pending_kana.empty())
        out += pending_kana;

    return out;
}


//  ------------------------------------------------------------------

void g_set_translit_rules(const char* rules)
{
    std::string r = rules ? rules : "";
    while(not r.empty() and isspace((unsigned char)r[r.length()-1]))
        r.erase(r.length() - 1);
    while(not r.empty() and isspace((unsigned char)r[0]))
        r.erase(0, 1);

    translit_on = true;
    translit_table_only = false;
    translit_rules.erase();
    translit_engine_name.erase();

    if(r.empty() or strieql(r.c_str(), "NO") or strieql(r.c_str(), "OFF") or strieql(r.c_str(), "NONE"))
        translit_on = false;
    else if(strieql(r.c_str(), "TABLE") or strieql(r.c_str(), "BUILTIN"))
        translit_table_only = true;
    else if(strieql(r.c_str(), "YES") or strieql(r.c_str(), "ON") or strieql(r.c_str(), "DEFAULT") or strieql(r.c_str(), "BGN"))
        translit_rules = "Russian-Latin/BGN";
    else
        translit_rules = r;

#if defined(GOLD_ICU_ANY)
    icu_close_all();
#endif
}

bool g_translit_enabled()
{
    return translit_on;
}

const char* g_translit_engine()
{
    if(not translit_on)
        return "off";
    if(translit_engine_name.empty())
    {
#if defined(GOLD_ICU_ANY)
        if(icu_ready())
        {
            translit_engine_name = "ICU (";
            if(icu_rules_unknown)
                translit_engine_name += translit_rules + " unknown, Any-Latin";
            else
                translit_engine_name += translit_rules.empty() ? std::string("Any-Latin") : translit_rules;
            translit_engine_name += ")";
        }
        else
#endif
        {
            translit_engine_name = "built-in table (BGN/PCGN Cyrillic and Greek, Hepburn kana)";
        }
    }
    return translit_engine_name.c_str();
}

std::string g_translit(const char* utf8, size_t len, bool ascii)
{
    if(utf8 == NULL or len == 0)
        return std::string();
    if(not translit_on)
        return std::string(utf8, len);

#if defined(GOLD_ICU_ANY)
    std::string out;
    if(icu_translit(utf8, len, ascii, out))
        return out;
#endif

    return table_translit(utf8, len, ascii);
}

std::string g_translit(const std::string& utf8, bool ascii)
{
    return g_translit(utf8.data(), utf8.length(), ascii);
}
