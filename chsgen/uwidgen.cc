//  ------------------------------------------------------------------
//  Generate the character-width tables GoldED+ compiles in.
//
//  GoldED+ decides how many screen columns a codepoint occupies from
//  its own tables rather than from the host wcwidth(): DOS and OS/2
//  have none, and the ones that exist elsewhere are usually years out
//  of date. The price is that the tables have to be regenerated when
//  Unicode gains characters, which is what this program is for.
//
//  It reads three files from the Unicode Character Database and writes
//  goldlib/gall/guwidth.inc:
//
//      EastAsianWidth.txt          ucd/
//      DerivedGeneralCategory.txt  ucd/extracted/
//      emoji-data.txt              ucd/emoji/
//
//  To move to a new revision of Unicode, drop the three files from
//  https://www.unicode.org/Public/<version>/ucd/ into this directory
//  and run `make uwidth'. Nothing else needs touching.
//
//  Usage: uwidgen <ucd-directory> <output-file>
//  ------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

const uint32_t CPMAX = 0x110000;

static unsigned char width[CPMAX];      // columns each codepoint takes
static unsigned char gcb[CPMAX];        // grapheme cluster break class
static std::string   ucdversion;


//  ------------------------------------------------------------------
//  The grapheme break classes of UAX #29, in the order the reader
//  declares them. GCB_EXTPICT is not one of Unicode's classes: it is
//  the Extended_Pictographic property, which rule GB11 needs, parked
//  here because every character that carries it would otherwise be
//  GCB_OTHER and the two never collide.

static const char* gcbnames[] =
{
    "Other", "CR", "LF", "Control", "Extend", "ZWJ", "Regional_Indicator",
    "Prepend", "SpacingMark", "L", "V", "T", "LV", "LVT", "ExtPict"
};
const int GCB_OTHER = 0, GCB_LV = 12, GCB_LVT = 13, GCB_EXTPICT = 14;
const int GCBCOUNT = (int)(sizeof(gcbnames)/sizeof(gcbnames[0]));


//  ------------------------------------------------------------------
//  Walk one UCD file, calling back with every (range, value) it states.
//  All three files share the layout: an optional range, a semicolon,
//  a value, and a comment introduced by '#'.

typedef void (*ucdfunc)(uint32_t first, uint32_t last, const char* value);

static void ucdread(const std::string& dir, const char* name, ucdfunc fn)
{
    std::string path = dir + "/" + name;
    FILE* fp = fopen(path.c_str(), "r");
    if(fp == NULL)
    {
        fprintf(stderr, "uwidgen: cannot read %s\n", path.c_str());
        exit(1);
    }

    //  The first line names the file and its revision, e.g.
    //  "# EastAsianWidth-17.0.0.txt". Keep it for the banner.
    char line[1024];
    while(fgets(line, sizeof(line), fp))
    {
        char* hash = strchr(line, '#');
        if(hash)
        {
            if(ucdversion.empty())
            {
                const char* dash = strchr(hash, '-');
                if(dash and strstr(hash, ".txt"))
                {
                    std::string v(dash + 1);
                    size_t dot = v.find(".txt");
                    if(dot != std::string::npos)
                        ucdversion = v.substr(0, dot);
                }
            }
            *hash = '\0';
        }

        char* semi = strchr(line, ';');
        if(semi == NULL)
            continue;
        *semi++ = '\0';

        //  The value runs to the next semicolon, if any, and loses its
        //  surrounding blanks.
        char* vend = strchr(semi, ';');
        if(vend)
            *vend = '\0';
        while(*semi == ' ' or *semi == '\t')
            semi++;
        char* t = semi + strlen(semi);
        while(t > semi and (t[-1] == ' ' or t[-1] == '\t' or t[-1] == '\n' or t[-1] == '\r'))
            *--t = '\0';

        uint32_t first = 0, last = 0;
        const char* dots = strstr(line, "..");
        if(dots)
        {
            first = (uint32_t)strtoul(line, NULL, 16);
            last  = (uint32_t)strtoul(dots + 2, NULL, 16);
        }
        else
        {
            first = last = (uint32_t)strtoul(line, NULL, 16);
        }

        if(last >= CPMAX or last < first)
            continue;

        fn(first, last, semi);
    }

    fclose(fp);
}


//  ------------------------------------------------------------------
//  East_Asian_Width W (wide) and F (fullwidth) take two columns. The
//  rest - narrow, halfwidth, ambiguous and neutral - take one. We do
//  not try to be clever about Ambiguous: a terminal that draws it wide
//  is configured that way, and guessing would be worse than not.

static void eastasian(uint32_t first, uint32_t last, const char* value)
{
    if(strcmp(value, "W") and strcmp(value, "F"))
        return;
    for(uint32_t cp = first; cp <= last; cp++)
        width[cp] = 2;
}


//  ------------------------------------------------------------------
//  Anything with Emoji_Presentation is drawn as a picture rather than
//  as text, and every terminal that draws it at all gives it two
//  columns. Unicode marks these Wide as well nowadays, so this mostly
//  confirms what East_Asian_Width already said.

static void emoji(uint32_t first, uint32_t last, const char* value)
{
    if(strcmp(value, "Emoji_Presentation"))
        return;
    for(uint32_t cp = first; cp <= last; cp++)
        width[cp] = 2;
}


//  ------------------------------------------------------------------
//  Combining marks take no room of their own, and neither do the
//  invisible formatting characters - with one exception: SOFT HYPHEN
//  is Cf but is drawn, so it keeps its column.
//
//  This runs last, so a mark that some other file called wide still
//  ends up at zero.

static void category(uint32_t first, uint32_t last, const char* value)
{
    bool zero = (strcmp(value, "Mn") == 0)      // nonspacing mark
             or (strcmp(value, "Me") == 0)      // enclosing mark
             or (strcmp(value, "Cf") == 0);     // format character

    if(not zero)
        return;

    for(uint32_t cp = first; cp <= last; cp++)
    {
        if(cp == 0x00AD)                        // SOFT HYPHEN is drawn
            continue;
        width[cp] = 0;
    }
}


//  ------------------------------------------------------------------
//  Grapheme cluster break classes, straight out of the file.
//
//  LV and LVT are dropped: the file spells out all 11172 Hangul
//  syllables one at a time, which would be 798 ranges to say what one
//  division tells us. The reader works them out instead.

static void graphemebreak(uint32_t first, uint32_t last, const char* value)
{
    int cls = GCB_OTHER;
    for(int n = 1; n < GCBCOUNT; n++)
    {
        if(strcmp(value, gcbnames[n]) == 0)
        {
            cls = n;
            break;
        }
    }

    if(cls == GCB_OTHER or cls == GCB_LV or cls == GCB_LVT)
        return;

    for(uint32_t cp = first; cp <= last; cp++)
        gcb[cp] = (unsigned char)cls;
}


//  ------------------------------------------------------------------
//  Extended_Pictographic, for rule GB11. Only where nothing else has
//  claimed the character.

static void extpict(uint32_t first, uint32_t last, const char* value)
{
    if(strcmp(value, "Extended_Pictographic"))
        return;
    for(uint32_t cp = first; cp <= last; cp++)
        if(gcb[cp] == GCB_OTHER)
            gcb[cp] = (unsigned char)GCB_EXTPICT;
}


//  ------------------------------------------------------------------
//  Write the break classes as ranges, each with the class it carries.

static void emitgcb(FILE* fp, int* ranges)
{
    fprintf(fp, "static const gcbrange gcb_classes[] =\n{\n");

    int count = 0;
    uint32_t cp = 0;
    while(cp < CPMAX)
    {
        if(gcb[cp] == GCB_OTHER)
        {
            cp++;
            continue;
        }

        uint32_t first = cp;
        unsigned char cls = gcb[cp];
        while(cp + 1 < CPMAX and gcb[cp + 1] == cls)
            cp++;

        fprintf(fp, "    { 0x%04X, 0x%04X, GCB_%s },\n", first, cp,
                gcbnames[cls]);
        count++;
        cp++;
    }

    fprintf(fp, "};\n");
    *ranges = count;
}


//  ------------------------------------------------------------------
//  Write one table as the sorted list of ranges the reader expects.

static void emit(FILE* fp, const char* name, unsigned char want, int* ranges)
{
    fprintf(fp, "static const cprange %s[] =\n{\n", name);

    int count = 0, online = 0;
    uint32_t cp = 0;
    while(cp < CPMAX)
    {
        if(width[cp] != want)
        {
            cp++;
            continue;
        }

        uint32_t first = cp;
        while(cp + 1 < CPMAX and width[cp + 1] == want)
            cp++;

        if(online == 0)
            fprintf(fp, "   ");
        fprintf(fp, " { 0x%04X, 0x%04X },", first, cp);
        count++;
        if(++online == 3)
        {
            fprintf(fp, "\n");
            online = 0;
        }
        cp++;
    }

    if(online)
        fprintf(fp, "\n");
    fprintf(fp, "};\n");

    *ranges = count;
}


//  ------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        fprintf(stderr, "usage: uwidgen <ucd-directory> <output-file>\n");
        return 1;
    }

    const std::string dir = argv[1];

    for(uint32_t cp = 0; cp < CPMAX; cp++)
        width[cp] = 1;

    ucdread(dir, "EastAsianWidth.txt", eastasian);
    ucdread(dir, "emoji-data.txt", emoji);
    ucdread(dir, "DerivedGeneralCategory.txt", category);

    ucdread(dir, "GraphemeBreakProperty.txt", graphemebreak);
    ucdread(dir, "emoji-data.txt", extpict);

    //  Conjoining Hangul jamo: a medial vowel or a final consonant is
    //  drawn inside the syllable its leading consonant began, so it
    //  adds nothing of its own. Unicode calls them ordinary letters,
    //  so no property above catches this.
    for(uint32_t cp = 0x1160; cp <= 0x11FF; cp++)
        width[cp] = 0;

    if(ucdversion.empty())
        ucdversion = "unknown";

    FILE* fp = fopen(argv[2], "w");
    if(fp == NULL)
    {
        fprintf(stderr, "uwidgen: cannot write %s\n", argv[2]);
        return 1;
    }

    fprintf(fp,
        "//  ---------------------------------------------------------"
        "---------\n"
        "//  How many screen columns each codepoint occupies.\n"
        "//\n"
        "//  Generated by chsgen/uwidgen from the Unicode Character\n"
        "//  Database, revision %s. Do not edit: run `make uwidth' in\n"
        "//  chsgen instead, after putting the newer UCD files there.\n"
        "//  ---------------------------------------------------------"
        "---------\n\n",
        ucdversion.c_str());

    int zeroes = 0, doubles = 0, breaks = 0;
    emit(fp, "zero_width", 0, &zeroes);
    fprintf(fp, "\n");
    emit(fp, "double_width", 2, &doubles);

    fprintf(fp, "\n\n//  Grapheme cluster break classes - UAX #29.\n\n");
    fprintf(fp, "enum gcbclass\n{\n");
    for(int n = 0; n < GCBCOUNT; n++)
        fprintf(fp, "    GCB_%s%s\n", gcbnames[n],
                n + 1 < GCBCOUNT ? "," : "");
    fprintf(fp, "};\n\nstruct gcbrange\n{\n"
                "    uint32_t first, last;\n"
                "    gcbclass cls;\n};\n\n");
    emitgcb(fp, &breaks);

    fclose(fp);

    printf("uwidgen: Unicode %s, %d zero-width ranges, %d double-width "
           "ranges, %d break-class ranges, %d bytes of table\n",
           ucdversion.c_str(), zeroes, doubles, breaks,
           (int)((zeroes + doubles) * 2 * sizeof(uint32_t)
                 + breaks * (2 * sizeof(uint32_t) + sizeof(int))));
    return 0;
}
