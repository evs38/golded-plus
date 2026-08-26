______________________________________________________________________

                   Generated tables, and their data
______________________________________________________________________


Two things in GoldED+ are generated rather than written by hand, and
both are generated here from files of the Unicode Character Database.
The generated output is committed, so an ordinary build needs none of
this - it is only wanted when moving to a newer revision of Unicode.


chsgen - charset conversion tables
----------------------------------

    make
    chsgn<platform> map/<from>.txt map/<to>.txt

reads the two charset maps in map/ and writes a conversion between
them. codepages.bat runs every pair. UnicodeData.txt and
AltUnicodeData.txt supply the character names and decompositions used
to pick a substitute where a character has no exact counterpart;
AltUnicodeData.txt is ours, not Unicode's, and holds the substitutions
this program prefers - it is not regenerated.


uwidgen - character width tables
--------------------------------

    make uwidth

writes goldlib/gall/guwidth.inc, which is how GoldED+ knows that an
emoji takes two screen columns and a combining acute takes none. It
reads three UCD files from this directory:

    EastAsianWidth.txt          W and F are two columns wide
    emoji-data.txt              Emoji_Presentation is two columns wide
    DerivedGeneralCategory.txt  Mn, Me and Cf take no columns

The host wcwidth() is deliberately not used: DOS and OS/2 have none,
and elsewhere it is usually several revisions of Unicode behind - which
is exactly the fault this table was written to avoid.


Moving to a newer Unicode
-------------------------

Fetch the files from

    https://www.unicode.org/Public/<version>/ucd/

taking EastAsianWidth.txt and UnicodeData.txt from that directory,
DerivedGeneralCategory.txt from extracted/ and emoji-data.txt from
emoji/. Put all four here, run `make uwidth', and check the count it
prints. That is the whole procedure.

The files here are revision 17.0.0, dated July 2025.

Replacing UnicodeData.txt does not silently change the charset tables:
going from the previous copy to 17.0.0 was checked by regenerating 237
conversions - every pair from cp866, cp1251 and cp437 to all 79 maps -
and comparing them. They came out identical.
