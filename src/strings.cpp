#include "pch.h"
#include "strings.h"
#include "ui.h"
#include "util.h"

wchar_t Normalize(__in wchar_t c)
{
    static std::unordered_map<wchar_t, wchar_t> accents;

    if (c < 128) return ((c >= L'A') && (c <= L'Z')) ? c - L'A' + L'a' : c;

    if (accents.empty())
    {
        accents[0x0041] = 'a';
        accents[0x0042] = 'b';
        accents[0x0043] = 'c';
        accents[0x0044] = 'd';
        accents[0x0045] = 'e';
        accents[0x0046] = 'f';
        accents[0x0047] = 'g';
        accents[0x0048] = 'h';
        accents[0x0049] = 'i';
        accents[0x004a] = 'j';
        accents[0x004b] = 'k';
        accents[0x004c] = 'l';
        accents[0x004d] = 'm';
        accents[0x004e] = 'n';
        accents[0x004f] = 'o';
        accents[0x0050] = 'p';
        accents[0x0051] = 'q';
        accents[0x0052] = 'r';
        accents[0x0053] = 's';
        accents[0x0054] = 't';
        accents[0x0055] = 'u';
        accents[0x0056] = 'v';
        accents[0x0057] = 'w';
        accents[0x0058] = 'x';
        accents[0x0059] = 'y';
        accents[0x005a] = 'z';
        accents[0x0061] = 'a';
        accents[0x0062] = 'b';
        accents[0x0063] = 'c';
        accents[0x0064] = 'd';
        accents[0x0065] = 'e';
        accents[0x0066] = 'f';
        accents[0x0067] = 'g';
        accents[0x0068] = 'h';
        accents[0x0069] = 'i';
        accents[0x006a] = 'j';
        accents[0x006b] = 'k';
        accents[0x006c] = 'l';
        accents[0x006d] = 'm';
        accents[0x006e] = 'n';
        accents[0x006f] = 'o';
        accents[0x0070] = 'p';
        accents[0x0071] = 'q';
        accents[0x0072] = 'r';
        accents[0x0073] = 's';
        accents[0x0074] = 't';
        accents[0x0075] = 'u';
        accents[0x0076] = 'v';
        accents[0x0077] = 'w';
        accents[0x0078] = 'x';
        accents[0x0079] = 'y';
        accents[0x007a] = 'z';
        accents[0x00aa] = 'a';  // feminine ordinal indicator
        accents[0x00ba] = 'o';	// masculine ordinal indicator
        accents[0x00c0] = 'a';	// latin capital letter a with grave
        accents[0x00c1] = 'a';	// latin capital letter a with acute
        accents[0x00c2] = 'a';	// latin capital letter a with circumflex
        accents[0x00c3] = 'a';	// latin capital letter a with tilde
        accents[0x00c4] = 'a';	// latin capital letter a with diaeresis
        accents[0x00c5] = 'a';	// latin capital letter a with ring above
        accents[0x00c6] = 'ae';	// latin capital letter ae -- no decomposition
        accents[0x00c7] = 'c';	// latin capital letter c with cedilla
        accents[0x00c8] = 'e';	// latin capital letter e with grave
        accents[0x00c9] = 'e';	// latin capital letter e with acute
        accents[0x00ca] = 'e';	// latin capital letter e with circumflex
        accents[0x00cb] = 'e';	// latin capital letter e with diaeresis
        accents[0x00cc] = 'i';	// latin capital letter i with grave
        accents[0x00cd] = 'i';	// latin capital letter i with acute
        accents[0x00ce] = 'i';	// latin capital letter i with circumflex
        accents[0x00cf] = 'i';	// latin capital letter i with diaeresis
        accents[0x00d0] = 'd';	// latin capital letter eth -- no decomposition  	// eth [d for vietnamese]
        accents[0x00d1] = 'n';	// latin capital letter n with tilde
        accents[0x00d2] = 'o';	// latin capital letter o with grave
        accents[0x00d3] = 'o';	// latin capital letter o with acute
        accents[0x00d4] = 'o';	// latin capital letter o with circumflex
        accents[0x00d5] = 'o';	// latin capital letter o with tilde
        accents[0x00d6] = 'o';	// latin capital letter o with diaeresis
        accents[0x00d8] = 'o';	// latin capital letter o with stroke -- no decom
        accents[0x00d9] = 'u';	// latin capital letter u with grave
        accents[0x00da] = 'u';	// latin capital letter u with acute
        accents[0x00db] = 'u';	// latin capital letter u with circumflex
        accents[0x00dc] = 'u';	// latin capital letter u with diaeresis
        accents[0x00dd] = 'y';	// latin capital letter y with acute
        accents[0x00de] = 'th';	// latin capital letter thorn -- no decomposition; // thorn - could be nothing other than thorn
        accents[0x00df] = 's';	// latin small letter sharp s -- no decomposition
        accents[0x00e0] = 'a';	// latin small letter a with grave
        accents[0x00e1] = 'a';	// latin small letter a with acute
        accents[0x00e2] = 'a';	// latin small letter a with circumflex
        accents[0x00e3] = 'a';	// latin small letter a with tilde
        accents[0x00e4] = 'a';	// latin small letter a with diaeresis
        accents[0x00e5] = 'a';	// latin small letter a with ring above
        accents[0x00e6] = 'ae';	// latin small letter ae -- no decomposition
        accents[0x00e7] = 'c';	// latin small letter c with cedilla
        accents[0x00e8] = 'e';	// latin small letter e with grave
        accents[0x00e9] = 'e';	// latin small letter e with acute
        accents[0x00ea] = 'e';	// latin small letter e with circumflex
        accents[0x00eb] = 'e';	// latin small letter e with diaeresis
        accents[0x00ec] = 'i';	// latin small letter i with grave
        accents[0x00ed] = 'i';	// latin small letter i with acute
        accents[0x00ee] = 'i';	// latin small letter i with circumflex
        accents[0x00ef] = 'i';	// latin small letter i with diaeresis
        accents[0x00f0] = 'd';	// latin small letter eth -- no decomposition         // small eth, "d" for benefit of vietnamese
        accents[0x00f1] = 'n';	// latin small letter n with tilde
        accents[0x00f2] = 'o';	// latin small letter o with grave
        accents[0x00f3] = 'o';	// latin small letter o with acute
        accents[0x00f4] = 'o';	// latin small letter o with circumflex
        accents[0x00f5] = 'o';	// latin small letter o with tilde
        accents[0x00f6] = 'o';	// latin small letter o with diaeresis
        accents[0x00f8] = 'o';	// latin small letter o with stroke -- no decompo
        accents[0x00f9] = 'u';	// latin small letter u with grave
        accents[0x00fa] = 'u';	// latin small letter u with acute
        accents[0x00fb] = 'u';	// latin small letter u with circumflex
        accents[0x00fc] = 'u';	// latin small letter u with diaeresis
        accents[0x00fd] = 'y';	// latin small letter y with acute
        accents[0x00fe] = 'th';	// latin small letter thorn -- no decomposition  // small thorn
        accents[0x00ff] = 'y';	// latin small letter y with diaeresis
        accents[0x0100] = 'a';	// latin capital letter a with macron
        accents[0x0101] = 'a';	// latin small letter a with macron
        accents[0x0102] = 'a';	// latin capital letter a with breve
        accents[0x0103] = 'a';	// latin small letter a with breve
        accents[0x0104] = 'a';	// latin capital letter a with ogonek
        accents[0x0105] = 'a';	// latin small letter a with ogonek
        accents[0x0106] = 'c';	// latin capital letter c with acute
        accents[0x0107] = 'c';	// latin small letter c with acute
        accents[0x0108] = 'c';	// latin capital letter c with circumflex
        accents[0x0109] = 'c';	// latin small letter c with circumflex
        accents[0x010a] = 'c';	// latin capital letter c with dot above
        accents[0x010b] = 'c';	// latin small letter c with dot above
        accents[0x010c] = 'c';	// latin capital letter c with caron
        accents[0x010d] = 'c';	// latin small letter c with caron
        accents[0x010e] = 'd';	// latin capital letter d with caron
        accents[0x010f] = 'd';	// latin small letter d with caron
        accents[0x0110] = 'd';	// latin capital letter d with stroke -- no decomposition                     // capital d with stroke
        accents[0x0111] = 'd';	// latin small letter d with stroke -- no decomposition                       // small d with stroke
        accents[0x0112] = 'e';	// latin capital letter e with macron
        accents[0x0113] = 'e';	// latin small letter e with macron
        accents[0x0114] = 'e';	// latin capital letter e with breve
        accents[0x0115] = 'e';	// latin small letter e with breve
        accents[0x0116] = 'e';	// latin capital letter e with dot above
        accents[0x0117] = 'e';	// latin small letter e with dot above
        accents[0x0118] = 'e';	// latin capital letter e with ogonek
        accents[0x0119] = 'e';	// latin small letter e with ogonek
        accents[0x011a] = 'e';	// latin capital letter e with caron
        accents[0x011b] = 'e';	// latin small letter e with caron
        accents[0x011c] = 'g';	// latin capital letter g with circumflex
        accents[0x011d] = 'g';	// latin small letter g with circumflex
        accents[0x011e] = 'g';	// latin capital letter g with breve
        accents[0x011f] = 'g';	// latin small letter g with breve
        accents[0x0120] = 'g';	// latin capital letter g with dot above
        accents[0x0121] = 'g';	// latin small letter g with dot above
        accents[0x0122] = 'g';	// latin capital letter g with cedilla
        accents[0x0123] = 'g';	// latin small letter g with cedilla
        accents[0x0124] = 'h';	// latin capital letter h with circumflex
        accents[0x0125] = 'h';	// latin small letter h with circumflex
        accents[0x0126] = 'h';	// latin capital letter h with stroke -- no decomposition
        accents[0x0127] = 'h';	// latin small letter h with stroke -- no decomposition
        accents[0x0128] = 'i';	// latin capital letter i with tilde
        accents[0x0129] = 'i';	// latin small letter i with tilde
        accents[0x012a] = 'i';	// latin capital letter i with macron
        accents[0x012b] = 'i';	// latin small letter i with macron
        accents[0x012c] = 'i';	// latin capital letter i with breve
        accents[0x012d] = 'i';	// latin small letter i with breve
        accents[0x012e] = 'i';	// latin capital letter i with ogonek
        accents[0x012f] = 'i';	// latin small letter i with ogonek
        accents[0x0130] = 'i';	// latin capital letter i with dot above
        accents[0x0131] = 'i';	// latin small letter dotless i -- no decomposition
        accents[0x0132] = 'i';	// latin capital ligature ij    
        accents[0x0133] = 'i';	// latin small ligature ij      
        accents[0x0134] = 'j';	// latin capital letter j with circumflex
        accents[0x0135] = 'j';	// latin small letter j with circumflex
        accents[0x0136] = 'k';	// latin capital letter k with cedilla
        accents[0x0137] = 'k';	// latin small letter k with cedilla
        accents[0x0138] = 'k';	// latin small letter kra -- no decomposition
        accents[0x0139] = 'l';	// latin capital letter l with acute
        accents[0x013a] = 'l';	// latin small letter l with acute
        accents[0x013b] = 'l';	// latin capital letter l with cedilla
        accents[0x013c] = 'l';	// latin small letter l with cedilla
        accents[0x013d] = 'l';	// latin capital letter l with caron
        accents[0x013e] = 'l';	// latin small letter l with caron
        accents[0x013f] = 'l';	// latin capital letter l with middle dot
        accents[0x0140] = 'l';	// latin small letter l with middle dot
        accents[0x0141] = 'l';	// latin capital letter l with stroke -- no decomposition
        accents[0x0142] = 'l';	// latin small letter l with stroke -- no decomposition
        accents[0x0143] = 'n';	// latin capital letter n with acute
        accents[0x0144] = 'n';	// latin small letter n with acute
        accents[0x0145] = 'n';	// latin capital letter n with cedilla
        accents[0x0146] = 'n';	// latin small letter n with cedilla
        accents[0x0147] = 'n';	// latin capital letter n with caron
        accents[0x0148] = 'n';	// latin small letter n with caron
        accents[0x0149] = '\'n';	// latin small letter n preceded by apostrophe                              ;
        accents[0x014a] = 'ng';	// latin capital letter eng -- no decomposition                             ;
        accents[0x014b] = 'ng';	// latin small letter eng -- no decomposition                               ;
        accents[0x014c] = 'o';	// latin capital letter o with macron
        accents[0x014d] = 'o';	// latin small letter o with macron
        accents[0x014e] = 'o';	// latin capital letter o with breve
        accents[0x014f] = 'o';	// latin small letter o with breve
        accents[0x0150] = 'o';	// latin capital letter o with double acute
        accents[0x0151] = 'o';	// latin small letter o with double acute
        accents[0x0152] = 'oe';	// latin capital ligature oe -- no decomposition
        accents[0x0153] = 'oe';	// latin small ligature oe -- no decomposition
        accents[0x0154] = 'r';	// latin capital letter r with acute
        accents[0x0155] = 'r';	// latin small letter r with acute
        accents[0x0156] = 'r';	// latin capital letter r with cedilla
        accents[0x0157] = 'r';	// latin small letter r with cedilla
        accents[0x0158] = 'r';	// latin capital letter r with caron
        accents[0x0159] = 'r';	// latin small letter r with caron
        accents[0x015a] = 's';	// latin capital letter s with acute
        accents[0x015b] = 's';	// latin small letter s with acute
        accents[0x015c] = 's';	// latin capital letter s with circumflex
        accents[0x015d] = 's';	// latin small letter s with circumflex
        accents[0x015e] = 's';	// latin capital letter s with cedilla
        accents[0x015f] = 's';	// latin small letter s with cedilla
        accents[0x0160] = 's';	// latin capital letter s with caron
        accents[0x0161] = 's';	// latin small letter s with caron
        accents[0x0162] = 't';	// latin capital letter t with cedilla
        accents[0x0163] = 't';	// latin small letter t with cedilla
        accents[0x0164] = 't';	// latin capital letter t with caron
        accents[0x0165] = 't';	// latin small letter t with caron
        accents[0x0166] = 't';	// latin capital letter t with stroke -- no decomposition
        accents[0x0167] = 't';	// latin small letter t with stroke -- no decomposition
        accents[0x0168] = 'u';	// latin capital letter u with tilde
        accents[0x0169] = 'u';	// latin small letter u with tilde
        accents[0x016a] = 'u';	// latin capital letter u with macron
        accents[0x016b] = 'u';	// latin small letter u with macron
        accents[0x016c] = 'u';	// latin capital letter u with breve
        accents[0x016d] = 'u';	// latin small letter u with breve
        accents[0x016e] = 'u';	// latin capital letter u with ring above
        accents[0x016f] = 'u';	// latin small letter u with ring above
        accents[0x0170] = 'u';	// latin capital letter u with double acute
        accents[0x0171] = 'u';	// latin small letter u with double acute
        accents[0x0172] = 'u';	// latin capital letter u with ogonek
        accents[0x0173] = 'u';	// latin small letter u with ogonek
        accents[0x0174] = 'w';	// latin capital letter w with circumflex
        accents[0x0175] = 'w';	// latin small letter w with circumflex
        accents[0x0176] = 'y';	// latin capital letter y with circumflex
        accents[0x0177] = 'y';	// latin small letter y with circumflex
        accents[0x0178] = 'y';	// latin capital letter y with diaeresis
        accents[0x0179] = 'z';	// latin capital letter z with acute
        accents[0x017a] = 'z';	// latin small letter z with acute
        accents[0x017b] = 'z';	// latin capital letter z with dot above
        accents[0x017c] = 'z';	// latin small letter z with dot above
        accents[0x017d] = 'z';	// latin capital letter z with caron
        accents[0x017e] = 'z';	// latin small letter z with caron
        accents[0x017f] = 's';	// latin small letter long s    
        accents[0x0180] = 'b';	// latin small letter b with stroke -- no decomposition
        accents[0x0181] = 'b';	// latin capital letter b with hook -- no decomposition
        accents[0x0182] = 'b';	// latin capital letter b with topbar -- no decomposition
        accents[0x0183] = 'b';	// latin small letter b with topbar -- no decomposition
        accents[0x0184] = '6';	// latin capital letter tone six -- no decomposition
        accents[0x0185] = '6';	// latin small letter tone six -- no decomposition
        accents[0x0186] = 'o';	// latin capital letter open o -- no decomposition
        accents[0x0187] = 'c';	// latin capital letter c with hook -- no decomposition
        accents[0x0188] = 'c';	// latin small letter c with hook -- no decomposition
        accents[0x0189] = 'd';	// latin capital letter african d -- no decomposition
        accents[0x018a] = 'd';	// latin capital letter d with hook -- no decomposition
        accents[0x018b] = 'd';	// latin capital letter d with topbar -- no decomposition
        accents[0x018c] = 'd';	// latin small letter d with topbar -- no decomposition
        accents[0x018d] = 'd';	// latin small letter turned delta -- no decomposition
        accents[0x018e] = 'e';	// latin capital letter reversed e -- no decomposition
        accents[0x018f] = 'e';	// latin capital letter schwa -- no decomposition
        accents[0x0190] = 'e';	// latin capital letter open e -- no decomposition
        accents[0x0191] = 'f';	// latin capital letter f with hook -- no decomposition
        accents[0x0192] = 'f';	// latin small letter f with hook -- no decomposition
        accents[0x0193] = 'g';	// latin capital letter g with hook -- no decomposition
        accents[0x0194] = 'g';	// latin capital letter gamma -- no decomposition
        accents[0x0195] = 'hv';	// latin small letter hv -- no decomposition
        accents[0x0196] = 'i';	// latin capital letter iota -- no decomposition
        accents[0x0197] = 'i';	// latin capital letter i with stroke -- no decomposition
        accents[0x0198] = 'k';	// latin capital letter k with hook -- no decomposition
        accents[0x0199] = 'k';	// latin small letter k with hook -- no decomposition
        accents[0x019a] = 'l';	// latin small letter l with bar -- no decomposition
        accents[0x019b] = 'l';	// latin small letter lambda with stroke -- no decomposition
        accents[0x019c] = 'm';	// latin capital letter turned m -- no decomposition
        accents[0x019d] = 'n';	// latin capital letter n with left hook -- no decomposition
        accents[0x019e] = 'n';	// latin small letter n with long right leg -- no decomposition
        accents[0x019f] = 'o';	// latin capital letter o with middle tilde -- no decomposition
        accents[0x01a0] = 'o';	// latin capital letter o with horn
        accents[0x01a1] = 'o';	// latin small letter o with horn
        accents[0x01a2] = 'oi';	// latin capital letter oi -- no decomposition
        accents[0x01a3] = 'oi';	// latin small letter oi -- no decomposition
        accents[0x01a4] = 'p';	// latin capital letter p with hook -- no decomposition
        accents[0x01a5] = 'p';	// latin small letter p with hook -- no decomposition
        accents[0x01a6] = 'yr';	// latin letter yr -- no decomposition
        accents[0x01a7] = '2';	// latin capital letter tone two -- no decomposition
        accents[0x01a8] = '2';	// latin small letter tone two -- no decomposition
        accents[0x01a9] = 's';	// latin capital letter esh -- no decomposition
        accents[0x01aa] = 's';	// latin letter reversed esh loop -- no decomposition
        accents[0x01ab] = 't';	// latin small letter t with palatal hook -- no decomposition
        accents[0x01ac] = 't';	// latin capital letter t with hook -- no decomposition
        accents[0x01ad] = 't';	// latin small letter t with hook -- no decomposition
        accents[0x01ae] = 't';	// latin capital letter t with retroflex hook -- no decomposition
        accents[0x01af] = 'u';	// latin capital letter u with horn
        accents[0x01b0] = 'u';	// latin small letter u with horn
        accents[0x01b1] = 'u';	// latin capital letter upsilon -- no decomposition
        accents[0x01b2] = 'v';	// latin capital letter v with hook -- no decomposition
        accents[0x01b3] = 'y';	// latin capital letter y with hook -- no decomposition
        accents[0x01b4] = 'y';	// latin small letter y with hook -- no decomposition
        accents[0x01b5] = 'z';	// latin capital letter z with stroke -- no decomposition
        accents[0x01b6] = 'z';	// latin small letter z with stroke -- no decomposition
        accents[0x01b7] = 'z';	// latin capital letter ezh -- no decomposition
        accents[0x01b8] = 'z';	// latin capital letter ezh reversed -- no decomposition
        accents[0x01b9] = 'z';	// latin small letter ezh reversed -- no decomposition
        accents[0x01ba] = 'z';	// latin small letter ezh with tail -- no decomposition
        accents[0x01bb] = '2';	// latin letter two with stroke -- no decomposition
        accents[0x01bc] = '5';	// latin capital letter tone five -- no decomposition
        accents[0x01bd] = '5';	// latin small letter tone five -- no decomposition
        accents[0x01be] = '´';	// latin letter inverted glottal stop with stroke -- no decomposition
        accents[0x01bf] = 'w';	// latin letter wynn -- no decomposition
        accents[0x01c0] = '!';	// latin letter dental click -- no decomposition
        accents[0x01c1] = '!';	// latin letter lateral click -- no decomposition
        accents[0x01c2] = '!';	// latin letter alveolar click -- no decomposition
        accents[0x01c3] = '!';	// latin letter retroflex click -- no decomposition
        accents[0x01c4] = 'dz';	// latin capital letter dz with caron
        accents[0x01c5] = 'dz';	// latin capital letter d with small letter z with caron
        accents[0x01c6] = 'd';	// latin small letter dz with caron
        accents[0x01c7] = 'lj';	// latin capital letter lj
        accents[0x01c8] = 'lj';	// latin capital letter l with small letter j
        accents[0x01c9] = 'lj';	// latin small letter lj
        accents[0x01ca] = 'nj';	// latin capital letter nj
        accents[0x01cb] = 'nj';	// latin capital letter n with small letter j
        accents[0x01cc] = 'nj';	// latin small letter nj
        accents[0x01cd] = 'a';	// latin capital letter a with caron
        accents[0x01ce] = 'a';	// latin small letter a with caron
        accents[0x01cf] = 'i';	// latin capital letter i with caron
        accents[0x01d0] = 'i';	// latin small letter i with caron
        accents[0x01d1] = 'o';	// latin capital letter o with caron
        accents[0x01d2] = 'o';	// latin small letter o with caron
        accents[0x01d3] = 'u';	// latin capital letter u with caron
        accents[0x01d4] = 'u';	// latin small letter u with caron
        accents[0x01d5] = 'u';	// latin capital letter u with diaeresis and macron
        accents[0x01d6] = 'u';	// latin small letter u with diaeresis and macron
        accents[0x01d7] = 'u';	// latin capital letter u with diaeresis and acute
        accents[0x01d8] = 'u';	// latin small letter u with diaeresis and acute
        accents[0x01d9] = 'u';	// latin capital letter u with diaeresis and caron
        accents[0x01da] = 'u';	// latin small letter u with diaeresis and caron
        accents[0x01db] = 'u';	// latin capital letter u with diaeresis and grave
        accents[0x01dc] = 'u';	// latin small letter u with diaeresis and grave
        accents[0x01dd] = 'e';	// latin small letter turned e -- no decomposition
        accents[0x01de] = 'a';	// latin capital letter a with diaeresis and macron
        accents[0x01df] = 'a';	// latin small letter a with diaeresis and macron
        accents[0x01e0] = 'a';	// latin capital letter a with dot above and macron
        accents[0x01e1] = 'a';	// latin small letter a with dot above and macron
        accents[0x01e2] = 'ae';	// latin capital letter ae with macron
        accents[0x01e3] = 'ae';	// latin small letter ae with macron
        accents[0x01e4] = 'g';	// latin capital letter g with stroke -- no decomposition
        accents[0x01e5] = 'g';	// latin small letter g with stroke -- no decomposition
        accents[0x01e6] = 'g';	// latin capital letter g with caron
        accents[0x01e7] = 'g';	// latin small letter g with caron
        accents[0x01e8] = 'k';	// latin capital letter k with caron
        accents[0x01e9] = 'k';	// latin small letter k with caron
        accents[0x01ea] = 'o';	// latin capital letter o with ogonek
        accents[0x01eb] = 'o';	// latin small letter o with ogonek
        accents[0x01ec] = 'o';	// latin capital letter o with ogonek and macron
        accents[0x01ed] = 'o';	// latin small letter o with ogonek and macron
        accents[0x01ee] = 'z';	// latin capital letter ezh with caron
        accents[0x01ef] = 'z';	// latin small letter ezh with caron
        accents[0x01f0] = 'j';	// latin small letter j with caron
        accents[0x01f1] = 'dz';	// latin capital letter dz
        accents[0x01f2] = 'dz';	// latin capital letter d with small letter z
        accents[0x01f3] = 'dz';	// latin small letter dz
        accents[0x01f4] = 'g';	// latin capital letter g with acute
        accents[0x01f5] = 'g';	// latin small letter g with acute
        accents[0x01f6] = 'hv';	// latin capital letter hwair -- no decomposition
        accents[0x01f7] = 'w';	// latin capital letter wynn -- no decomposition
        accents[0x01f8] = 'n';	// latin capital letter n with grave
        accents[0x01f9] = 'n';	// latin small letter n with grave
        accents[0x01fa] = 'a';	// latin capital letter a with ring above and acute
        accents[0x01fb] = 'a';	// latin small letter a with ring above and acute
        accents[0x01fc] = 'ae';	// latin capital letter ae with acute
        accents[0x01fd] = 'ae';	// latin small letter ae with acute
        accents[0x01fe] = 'o';	// latin capital letter o with stroke and acute
        accents[0x01ff] = 'o';	// latin small letter o with stroke and acute
        accents[0x0200] = 'a';	// latin capital letter a with double grave
        accents[0x0201] = 'a';	// latin small letter a with double grave
        accents[0x0202] = 'a';	// latin capital letter a with inverted breve
        accents[0x0203] = 'a';	// latin small letter a with inverted breve
        accents[0x0204] = 'e';	// latin capital letter e with double grave
        accents[0x0205] = 'e';	// latin small letter e with double grave
        accents[0x0206] = 'e';	// latin capital letter e with inverted breve
        accents[0x0207] = 'e';	// latin small letter e with inverted breve
        accents[0x0208] = 'i';	// latin capital letter i with double grave
        accents[0x0209] = 'i';	// latin small letter i with double grave
        accents[0x020a] = 'i';	// latin capital letter i with inverted breve
        accents[0x020b] = 'i';	// latin small letter i with inverted breve
        accents[0x020c] = 'o';	// latin capital letter o with double grave
        accents[0x020d] = 'o';	// latin small letter o with double grave
        accents[0x020e] = 'o';	// latin capital letter o with inverted breve
        accents[0x020f] = 'o';	// latin small letter o with inverted breve
        accents[0x0210] = 'r';	// latin capital letter r with double grave
        accents[0x0211] = 'r';	// latin small letter r with double grave
        accents[0x0212] = 'r';	// latin capital letter r with inverted breve
        accents[0x0213] = 'r';	// latin small letter r with inverted breve
        accents[0x0214] = 'u';	// latin capital letter u with double grave
        accents[0x0215] = 'u';	// latin small letter u with double grave
        accents[0x0216] = 'u';	// latin capital letter u with inverted breve
        accents[0x0217] = 'u';	// latin small letter u with inverted breve
        accents[0x0218] = 's';	// latin capital letter s with comma below
        accents[0x0219] = 's';	// latin small letter s with comma below
        accents[0x021a] = 't';	// latin capital letter t with comma below
        accents[0x021b] = 't';	// latin small letter t with comma below
        accents[0x021c] = 'z';	// latin capital letter yogh -- no decomposition
        accents[0x021d] = 'z';	// latin small letter yogh -- no decomposition
        accents[0x021e] = 'h';	// latin capital letter h with caron
        accents[0x021f] = 'h';	// latin small letter h with caron
        accents[0x0220] = 'n';	// latin capital letter n with long right leg -- no decomposition
        accents[0x0221] = 'd';	// latin small letter d with curl -- no decomposition
        accents[0x0222] = 'ou';	// latin capital letter ou -- no decomposition
        accents[0x0223] = 'ou';	// latin small letter ou -- no decomposition
        accents[0x0224] = 'z';	// latin capital letter z with hook -- no decomposition
        accents[0x0225] = 'z';	// latin small letter z with hook -- no decomposition
        accents[0x0226] = 'a';	// latin capital letter a with dot above
        accents[0x0227] = 'a';	// latin small letter a with dot above
        accents[0x0228] = 'e';	// latin capital letter e with cedilla
        accents[0x0229] = 'e';	// latin small letter e with cedilla
        accents[0x022a] = 'o';	// latin capital letter o with diaeresis and macron
        accents[0x022b] = 'o';	// latin small letter o with diaeresis and macron
        accents[0x022c] = 'o';	// latin capital letter o with tilde and macron
        accents[0x022d] = 'o';	// latin small letter o with tilde and macron
        accents[0x022e] = 'o';	// latin capital letter o with dot above
        accents[0x022f] = 'o';	// latin small letter o with dot above
        accents[0x0230] = 'o';	// latin capital letter o with dot above and macron
        accents[0x0231] = 'o';	// latin small letter o with dot above and macron
        accents[0x0232] = 'y';	// latin capital letter y with macron
        accents[0x0233] = 'y';	// latin small letter y with macron
        accents[0x0234] = 'l';	// latin small letter l with curl -- no decomposition
        accents[0x0235] = 'n';	// latin small letter n with curl -- no decomposition
        accents[0x0236] = 't';	// latin small letter t with curl -- no decomposition
        accents[0x0250] = 'a';	// latin small letter turned a -- no decomposition
        accents[0x0251] = 'a';	// latin small letter alpha -- no decomposition
        accents[0x0252] = 'a';	// latin small letter turned alpha -- no decomposition
        accents[0x0253] = 'b';	// latin small letter b with hook -- no decomposition
        accents[0x0254] = 'o';	// latin small letter open o -- no decomposition
        accents[0x0255] = 'c';	// latin small letter c with curl -- no decomposition
        accents[0x0256] = 'd';	// latin small letter d with tail -- no decomposition
        accents[0x0257] = 'd';	// latin small letter d with hook -- no decomposition
        accents[0x0258] = 'e';	// latin small letter reversed e -- no decomposition
        accents[0x0259] = 'e';	// latin small letter schwa -- no decomposition
        accents[0x025a] = 'e';	// latin small letter schwa with hook -- no decomposition
        accents[0x025b] = 'e';	// latin small letter open e -- no decomposition
        accents[0x025c] = 'e';	// latin small letter reversed open e -- no decomposition
        accents[0x025d] = 'e';	// latin small letter reversed open e with hook -- no decomposition
        accents[0x025e] = 'e';	// latin small letter closed reversed open e -- no decomposition
        accents[0x025f] = 'j';	// latin small letter dotless j with stroke -- no decomposition
        accents[0x0260] = 'g';	// latin small letter g with hook -- no decomposition
        accents[0x0261] = 'g';	// latin small letter script g -- no decomposition
        accents[0x0262] = 'g';	// latin letter small capital g -- no decomposition
        accents[0x0263] = 'g';	// latin small letter gamma -- no decomposition
        accents[0x0264] = 'y';	// latin small letter rams horn -- no decomposition
        accents[0x0265] = 'h';	// latin small letter turned h -- no decomposition
        accents[0x0266] = 'h';	// latin small letter h with hook -- no decomposition
        accents[0x0267] = 'h';	// latin small letter heng with hook -- no decomposition
        accents[0x0268] = 'i';	// latin small letter i with stroke -- no decomposition
        accents[0x0269] = 'i';	// latin small letter iota -- no decomposition
        accents[0x026a] = 'i';	// latin letter small capital i -- no decomposition
        accents[0x026b] = 'l';	// latin small letter l with middle tilde -- no decomposition
        accents[0x026c] = 'l';	// latin small letter l with belt -- no decomposition
        accents[0x026d] = 'l';	// latin small letter l with retroflex hook -- no decomposition
        accents[0x026e] = 'lz';	// latin small letter lezh -- no decomposition
        accents[0x026f] = 'm';	// latin small letter turned m -- no decomposition
        accents[0x0270] = 'm';	// latin small letter turned m with long leg -- no decomposition
        accents[0x0271] = 'm';	// latin small letter m with hook -- no decomposition
        accents[0x0272] = 'n';	// latin small letter n with left hook -- no decomposition
        accents[0x0273] = 'n';	// latin small letter n with retroflex hook -- no decomposition
        accents[0x0274] = 'n';	// latin letter small capital n -- no decomposition
        accents[0x0275] = 'o';	// latin small letter barred o -- no decomposition
        accents[0x0276] = 'oe';	// latin letter small capital oe -- no decomposition
        accents[0x0277] = 'o';	// latin small letter closed omega -- no decomposition
        accents[0x0278] = 'ph';	// latin small letter phi -- no decomposition
        accents[0x0279] = 'r';	// latin small letter turned r -- no decomposition
        accents[0x027a] = 'r';	// latin small letter turned r with long leg -- no decomposition
        accents[0x027b] = 'r';	// latin small letter turned r with hook -- no decomposition
        accents[0x027c] = 'r';	// latin small letter r with long leg -- no decomposition
        accents[0x027d] = 'r';	// latin small letter r with tail -- no decomposition
        accents[0x027e] = 'r';	// latin small letter r with fishhook -- no decomposition
        accents[0x027f] = 'r';	// latin small letter reversed r with fishhook -- no decomposition
        accents[0x0280] = 'r';	// latin letter small capital r -- no decomposition
        accents[0x0281] = 'r';	// latin letter small capital inverted r -- no decomposition
        accents[0x0282] = 's';	// latin small letter s with hook -- no decomposition
        accents[0x0283] = 's';	// latin small letter esh -- no decomposition
        accents[0x0284] = 'j';	// latin small letter dotless j with stroke and hook -- no decomposition
        accents[0x0285] = 's';	// latin small letter squat reversed esh -- no decomposition
        accents[0x0286] = 's';	// latin small letter esh with curl -- no decomposition
        accents[0x0287] = 'y';	// latin small letter turned t -- no decomposition
        accents[0x0288] = 't';	// latin small letter t with retroflex hook -- no decomposition
        accents[0x0289] = 'u';	// latin small letter u bar -- no decomposition
        accents[0x028a] = 'u';	// latin small letter upsilon -- no decomposition
        accents[0x028b] = 'u';	// latin small letter v with hook -- no decomposition
        accents[0x028c] = 'v';	// latin small letter turned v -- no decomposition
        accents[0x028d] = 'w';	// latin small letter turned w -- no decomposition
        accents[0x028e] = 'y';	// latin small letter turned y -- no decomposition
        accents[0x028f] = 'y';	// latin letter small capital y -- no decomposition
        accents[0x0290] = 'z';	// latin small letter z with retroflex hook -- no decomposition
        accents[0x0291] = 'z';	// latin small letter z with curl -- no decomposition
        accents[0x0292] = 'z';	// latin small letter ezh -- no decomposition
        accents[0x0293] = 'z';	// latin small letter ezh with curl -- no decomposition
        accents[0x0294] = '\'';	// latin letter glottal stop -- no decomposition
        accents[0x0295] = '\'';	// latin letter pharyngeal voiced fricative -- no decomposition
        accents[0x0296] = '\'';	// latin letter inverted glottal stop -- no decomposition
        accents[0x0297] = 'c';	// latin letter stretched c -- no decomposition
        accents[0x0298] = 'o˜';	// latin letter bilabial click -- no decomposition
        accents[0x0299] = 'b';	// latin letter small capital b -- no decomposition
        accents[0x029a] = 'e';	// latin small letter closed open e -- no decomposition
        accents[0x029b] = 'g';	// latin letter small capital g with hook -- no decomposition
        accents[0x029c] = 'h';	// latin letter small capital h -- no decomposition
        accents[0x029d] = 'j';	// latin small letter j with crossed-tail -- no decomposition
        accents[0x029e] = 'k';	// latin small letter turned k -- no decomposition
        accents[0x029f] = 'l';	// latin letter small capital l -- no decomposition
        accents[0x02a0] = 'q';	// latin small letter q with hook -- no decomposition
        accents[0x02a1] = '\'';	// latin letter glottal stop with stroke -- no decomposition
        accents[0x02a2] = '\'';	// latin letter reversed glottal stop with stroke -- no decomposition
        accents[0x02a3] = 'dz';	// latin small letter dz digraph -- no decomposition
        accents[0x02a4] = 'dz';	// latin small letter dezh digraph -- no decomposition
        accents[0x02a5] = 'dz';	// latin small letter dz digraph with curl -- no decomposition
        accents[0x02a6] = 'ts';	// latin small letter ts digraph -- no decomposition
        accents[0x02a7] = 'ts';	// latin small letter tesh digraph -- no decomposition
        accents[0x02a8] = '.'; // latin small letter tc digraph with curl -- no decomposition
        accents[0x02a9] = 'fn';	// latin small letter feng digraph -- no decomposition
        accents[0x02aa] = 'ls';	// latin small letter ls digraph -- no decomposition
        accents[0x02ab] = 'lz';	// latin small letter lz digraph -- no decomposition
        accents[0x02ac] = 'w';	// latin letter bilabial percussive -- no decomposition
        accents[0x02ad] = 't';	// latin letter bidental percussive -- no decomposition
        accents[0x02ae] = 'h';	// latin small letter turned h with fishhook -- no decomposition
        accents[0x02af] = 'h';	// latin small letter turned h with fishhook and tail -- no decomposition
        accents[0x02b0] = 'h';	// modifier letter small h
        accents[0x02b1] = 'h';	// modifier letter small h with hook
        accents[0x02b2] = 'j';	// modifier letter small j
        accents[0x02b3] = 'r';	// modifier letter small r
        accents[0x02b4] = 'r';	// modifier letter small turned r
        accents[0x02b5] = 'r';	// modifier letter small turned r with hook
        accents[0x02b6] = 'r';	// modifier letter small capital inverted r
        accents[0x02b7] = 'w';	// modifier letter small w
        accents[0x02b8] = 'y';	// modifier letter small y
        accents[0x02e1] = 'l';	// modifier letter small l
        accents[0x02e2] = 's';	// modifier letter small s
        accents[0x02e3] = 'x';	// modifier letter small x
        accents[0x02e4] = '\'';	// modifier letter small reversed glottal stop
        accents[0x1d00] = 'a';	// latin letter small capital a -- no decomposition
        accents[0x1d01] = 'ae';	// latin letter small capital ae -- no decomposition
        accents[0x1d02] = 'ae';	// latin small letter turned ae -- no decomposition
        accents[0x1d03] = 'b';	// latin letter small capital barred b -- no decomposition
        accents[0x1d04] = 'c';	// latin letter small capital c -- no decomposition
        accents[0x1d05] = 'd';	// latin letter small capital d -- no decomposition
        accents[0x1d06] = 'th';	// latin letter small capital eth -- no decomposition
        accents[0x1d07] = 'e';	// latin letter small capital e -- no decomposition
        accents[0x1d08] = 'e';	// latin small letter turned open e -- no decomposition
        accents[0x1d09] = 'i';	// latin small letter turned i -- no decomposition
        accents[0x1d0a] = 'j';	// latin letter small capital j -- no decomposition
        accents[0x1d0b] = 'k';	// latin letter small capital k -- no decomposition
        accents[0x1d0c] = 'l';	// latin letter small capital l with stroke -- no decomposition
        accents[0x1d0d] = 'm';	// latin letter small capital m -- no decomposition
        accents[0x1d0e] = 'n';	// latin letter small capital reversed n -- no decomposition
        accents[0x1d0f] = 'o';	// latin letter small capital o -- no decomposition
        accents[0x1d10] = 'o';	// latin letter small capital open o -- no decomposition
        accents[0x1d11] = 'o';	// latin small letter sideways o -- no decomposition
        accents[0x1d12] = 'o';	// latin small letter sideways open o -- no decomposition
        accents[0x1d13] = 'o';	// latin small letter sideways o with stroke -- no decomposition
        accents[0x1d14] = 'oe';	// latin small letter turned oe -- no decomposition
        accents[0x1d15] = 'ou';	// latin letter small capital ou -- no decomposition
        accents[0x1d16] = 'o';	// latin small letter top half o -- no decomposition
        accents[0x1d17] = 'o';	// latin small letter bottom half o -- no decomposition
        accents[0x1d18] = 'p';	// latin letter small capital p -- no decomposition
        accents[0x1d19] = 'r';	// latin letter small capital reversed r -- no decomposition
        accents[0x1d1a] = 'r';	// latin letter small capital turned r -- no decomposition
        accents[0x1d1b] = 't';	// latin letter small capital t -- no decomposition
        accents[0x1d1c] = 'u';	// latin letter small capital u -- no decomposition
        accents[0x1d1d] = 'u';	// latin small letter sideways u -- no decomposition
        accents[0x1d1e] = 'u';	// latin small letter sideways diaeresized u -- no decomposition
        accents[0x1d1f] = 'm';	// latin small letter sideways turned m -- no decomposition
        accents[0x1d20] = 'v';	// latin letter small capital v -- no decomposition
        accents[0x1d21] = 'w';	// latin letter small capital w -- no decomposition
        accents[0x1d22] = 'z';	// latin letter small capital z -- no decomposition
//        accents[0x1d23] = 'ezh';	// latin letter small capital ezh -- no decomposition
        accents[0x1d24] = '\'';	// latin letter voiced laryngeal spirant -- no decomposition
        accents[0x1d25] = 'l';	// latin letter ain -- no decomposition
        accents[0x1d2c] = 'a';	// modifier letter capital a
        accents[0x1d2d] = 'ae';	// modifier letter capital ae
        accents[0x1d2e] = 'b';	// modifier letter capital b
        accents[0x1d2f] = 'b';	// modifier letter capital barred b -- no decomposition
        accents[0x1d30] = 'd';	// modifier letter capital d
        accents[0x1d31] = 'e';	// modifier letter capital e
        accents[0x1d32] = 'e';	// modifier letter capital reversed e
        accents[0x1d33] = 'g';	// modifier letter capital g
        accents[0x1d34] = 'h';	// modifier letter capital h
        accents[0x1d35] = 'i';	// modifier letter capital i
        accents[0x1d36] = 'j';	// modifier letter capital j
        accents[0x1d37] = 'k';	// modifier letter capital k
        accents[0x1d38] = 'l';	// modifier letter capital l
        accents[0x1d39] = 'm';	// modifier letter capital m
        accents[0x1d3a] = 'n';	// modifier letter capital n
        accents[0x1d3b] = 'n';	// modifier letter capital reversed n -- no decomposition
        accents[0x1d3c] = 'o';	// modifier letter capital o
        accents[0x1d3d] = 'ou';	// modifier letter capital ou
        accents[0x1d3e] = 'p';	// modifier letter capital p
        accents[0x1d3f] = 'r';	// modifier letter capital r
        accents[0x1d40] = 't';	// modifier letter capital t
        accents[0x1d41] = 'u';	// modifier letter capital u
        accents[0x1d42] = 'w';	// modifier letter capital w
        accents[0x1d43] = 'a';	// modifier letter small a
        accents[0x1d44] = 'a';	// modifier letter small turned a
        accents[0x1d46] = 'ae';	// modifier letter small turned ae
        accents[0x1d47] = 'b';    // modifier letter small b
        accents[0x1d48] = 'd';    // modifier letter small d
        accents[0x1d49] = 'e';    // modifier letter small e
        accents[0x1d4a] = 'e';    // modifier letter small schwa
        accents[0x1d4b] = 'e';    // modifier letter small open e
        accents[0x1d4c] = 'e';    // modifier letter small turned open e
        accents[0x1d4d] = 'g';    // modifier letter small g
        accents[0x1d4e] = 'i';    // modifier letter small turned i -- no decomposition
        accents[0x1d4f] = 'k';    // modifier letter small k
        accents[0x1d50] = 'm';	// modifier letter small m
        accents[0x1d51] = 'g';	// modifier letter small eng
        accents[0x1d52] = 'o';	// modifier letter small o
        accents[0x1d53] = 'o';	// modifier letter small open o
        accents[0x1d54] = 'o';	// modifier letter small top half o
        accents[0x1d55] = 'o';	// modifier letter small bottom half o
        accents[0x1d56] = 'p';	// modifier letter small p
        accents[0x1d57] = 't';	// modifier letter small t
        accents[0x1d58] = 'u';	// modifier letter small u
        accents[0x1d59] = 'u';	// modifier letter small sideways u
        accents[0x1d5a] = 'm';	// modifier letter small turned m
        accents[0x1d5b] = 'v';	// modifier letter small v
        accents[0x1d62] = 'i';	// latin subscript small letter i
        accents[0x1d63] = 'r';	// latin subscript small letter r
        accents[0x1d64] = 'u';	// latin subscript small letter u
        accents[0x1d65] = 'v';	// latin subscript small letter v
        accents[0x1d6b] = 'ue';	// latin small letter ue -- no decomposition
        accents[0x1e00] = 'a';	// latin capital letter a with ring below
        accents[0x1e01] = 'a';	// latin small letter a with ring below
        accents[0x1e02] = 'b';	// latin capital letter b with dot above
        accents[0x1e03] = 'b';	// latin small letter b with dot above
        accents[0x1e04] = 'b';	// latin capital letter b with dot below
        accents[0x1e05] = 'b';	// latin small letter b with dot below
        accents[0x1e06] = 'b';	// latin capital letter b with line below
        accents[0x1e07] = 'b';	// latin small letter b with line below
        accents[0x1e08] = 'c';	// latin capital letter c with cedilla and acute
        accents[0x1e09] = 'c';	// latin small letter c with cedilla and acute
        accents[0x1e0a] = 'd';	// latin capital letter d with dot above
        accents[0x1e0b] = 'd';	// latin small letter d with dot above
        accents[0x1e0c] = 'd';	// latin capital letter d with dot below
        accents[0x1e0d] = 'd';	// latin small letter d with dot below
        accents[0x1e0e] = 'd';	// latin capital letter d with line below
        accents[0x1e0f] = 'd';	// latin small letter d with line below
        accents[0x1e10] = 'd';	// latin capital letter d with cedilla
        accents[0x1e11] = 'd';	// latin small letter d with cedilla
        accents[0x1e12] = 'd';	// latin capital letter d with circumflex below
        accents[0x1e13] = 'd';	// latin small letter d with circumflex below
        accents[0x1e14] = 'e';	// latin capital letter e with macron and grave
        accents[0x1e15] = 'e';	// latin small letter e with macron and grave
        accents[0x1e16] = 'e';	// latin capital letter e with macron and acute
        accents[0x1e17] = 'e';	// latin small letter e with macron and acute
        accents[0x1e18] = 'e';	// latin capital letter e with circumflex below
        accents[0x1e19] = 'e';	// latin small letter e with circumflex below
        accents[0x1e1a] = 'e';	// latin capital letter e with tilde below
        accents[0x1e1b] = 'e';	// latin small letter e with tilde below
        accents[0x1e1c] = 'e';	// latin capital letter e with cedilla and breve
        accents[0x1e1d] = 'e';	// latin small letter e with cedilla and breve
        accents[0x1e1e] = 'f';	// latin capital letter f with dot above
        accents[0x1e1f] = 'f';	// latin small letter f with dot above
        accents[0x1e20] = 'g';	// latin capital letter g with macron
        accents[0x1e21] = 'g';	// latin small letter g with macron
        accents[0x1e22] = 'h';	// latin capital letter h with dot above
        accents[0x1e23] = 'h';	// latin small letter h with dot above
        accents[0x1e24] = 'h';	// latin capital letter h with dot below
        accents[0x1e25] = 'h';	// latin small letter h with dot below
        accents[0x1e26] = 'h';	// latin capital letter h with diaeresis
        accents[0x1e27] = 'h';	// latin small letter h with diaeresis
        accents[0x1e28] = 'h';	// latin capital letter h with cedilla
        accents[0x1e29] = 'h';	// latin small letter h with cedilla
        accents[0x1e2a] = 'h';	// latin capital letter h with breve below
        accents[0x1e2b] = 'h';	// latin small letter h with breve below
        accents[0x1e2c] = 'i';	// latin capital letter i with tilde below
        accents[0x1e2d] = 'i';	// latin small letter i with tilde below
        accents[0x1e2e] = 'i';	// latin capital letter i with diaeresis and acute
        accents[0x1e2f] = 'i';	// latin small letter i with diaeresis and acute
        accents[0x1e30] = 'k';	// latin capital letter k with acute
        accents[0x1e31] = 'k';	// latin small letter k with acute
        accents[0x1e32] = 'k';	// latin capital letter k with dot below
        accents[0x1e33] = 'k';	// latin small letter k with dot below
        accents[0x1e34] = 'k';	// latin capital letter k with line below
        accents[0x1e35] = 'k';	// latin small letter k with line below
        accents[0x1e36] = 'l';	// latin capital letter l with dot below
        accents[0x1e37] = 'l';	// latin small letter l with dot below
        accents[0x1e38] = 'l';	// latin capital letter l with dot below and macron
        accents[0x1e39] = 'l';	// latin small letter l with dot below and macron
        accents[0x1e3a] = 'l';	// latin capital letter l with line below
        accents[0x1e3b] = 'l';	// latin small letter l with line below
        accents[0x1e3c] = 'l';	// latin capital letter l with circumflex below
        accents[0x1e3d] = 'l';	// latin small letter l with circumflex below
        accents[0x1e3e] = 'm';	// latin capital letter m with acute
        accents[0x1e3f] = 'm';	// latin small letter m with acute
        accents[0x1e40] = 'm';	// latin capital letter m with dot above
        accents[0x1e41] = 'm';	// latin small letter m with dot above
        accents[0x1e42] = 'm';	// latin capital letter m with dot below
        accents[0x1e43] = 'm';	// latin small letter m with dot below
        accents[0x1e44] = 'n';	// latin capital letter n with dot above
        accents[0x1e45] = 'n';	// latin small letter n with dot above
        accents[0x1e46] = 'n';	// latin capital letter n with dot below
        accents[0x1e47] = 'n';	// latin small letter n with dot below
        accents[0x1e48] = 'n';	// latin capital letter n with line below
        accents[0x1e49] = 'n';	// latin small letter n with line below
        accents[0x1e4a] = 'n';	// latin capital letter n with circumflex below
        accents[0x1e4b] = 'n';	// latin small letter n with circumflex below
        accents[0x1e4c] = 'o';	// latin capital letter o with tilde and acute
        accents[0x1e4d] = 'o';	// latin small letter o with tilde and acute
        accents[0x1e4e] = 'o';	// latin capital letter o with tilde and diaeresis
        accents[0x1e4f] = 'o';	// latin small letter o with tilde and diaeresis
        accents[0x1e50] = 'o';	// latin capital letter o with macron and grave
        accents[0x1e51] = 'o';	// latin small letter o with macron and grave
        accents[0x1e52] = 'o';	// latin capital letter o with macron and acute
        accents[0x1e53] = 'o';	// latin small letter o with macron and acute
        accents[0x1e54] = 'p';	// latin capital letter p with acute
        accents[0x1e55] = 'p';	// latin small letter p with acute
        accents[0x1e56] = 'p';	// latin capital letter p with dot above
        accents[0x1e57] = 'p';	// latin small letter p with dot above
        accents[0x1e58] = 'r';	// latin capital letter r with dot above
        accents[0x1e59] = 'r';	// latin small letter r with dot above
        accents[0x1e5a] = 'r';	// latin capital letter r with dot below
        accents[0x1e5b] = 'r';	// latin small letter r with dot below
        accents[0x1e5c] = 'r';	// latin capital letter r with dot below and macron
        accents[0x1e5d] = 'r';	// latin small letter r with dot below and macron
        accents[0x1e5e] = 'r';	// latin capital letter r with line below
        accents[0x1e5f] = 'r';	// latin small letter r with line below
        accents[0x1e60] = 's';	// latin capital letter s with dot above
        accents[0x1e61] = 's';	// latin small letter s with dot above
        accents[0x1e62] = 's';	// latin capital letter s with dot below
        accents[0x1e63] = 's';	// latin small letter s with dot below
        accents[0x1e64] = 's';	// latin capital letter s with acute and dot above
        accents[0x1e65] = 's';	// latin small letter s with acute and dot above
        accents[0x1e66] = 's';	// latin capital letter s with caron and dot above
        accents[0x1e67] = 's';	// latin small letter s with caron and dot above
        accents[0x1e68] = 's';	// latin capital letter s with dot below and dot above
        accents[0x1e69] = 's';	// latin small letter s with dot below and dot above
        accents[0x1e6a] = 't';	// latin capital letter t with dot above
        accents[0x1e6b] = 't';	// latin small letter t with dot above
        accents[0x1e6c] = 't';	// latin capital letter t with dot below
        accents[0x1e6d] = 't';	// latin small letter t with dot below
        accents[0x1e6e] = 't';	// latin capital letter t with line below
        accents[0x1e6f] = 't';	// latin small letter t with line below
        accents[0x1e70] = 't';	// latin capital letter t with circumflex below
        accents[0x1e71] = 't';	// latin small letter t with circumflex below
        accents[0x1e72] = 'u';	// latin capital letter u with diaeresis below
        accents[0x1e73] = 'u';	// latin small letter u with diaeresis below
        accents[0x1e74] = 'u';	// latin capital letter u with tilde below
        accents[0x1e75] = 'u';	// latin small letter u with tilde below
        accents[0x1e76] = 'u';	// latin capital letter u with circumflex below
        accents[0x1e77] = 'u';	// latin small letter u with circumflex below
        accents[0x1e78] = 'u';	// latin capital letter u with tilde and acute
        accents[0x1e79] = 'u';	// latin small letter u with tilde and acute
        accents[0x1e7a] = 'u';	// latin capital letter u with macron and diaeresis
        accents[0x1e7b] = 'u';	// latin small letter u with macron and diaeresis
        accents[0x1e7c] = 'v';	// latin capital letter v with tilde
        accents[0x1e7d] = 'v';	// latin small letter v with tilde
        accents[0x1e7e] = 'v';	// latin capital letter v with dot below
        accents[0x1e7f] = 'v';	// latin small letter v with dot below
        accents[0x1e80] = 'w';	// latin capital letter w with grave
        accents[0x1e81] = 'w';	// latin small letter w with grave
        accents[0x1e82] = 'w';	// latin capital letter w with acute
        accents[0x1e83] = 'w';	// latin small letter w with acute
        accents[0x1e84] = 'w';	// latin capital letter w with diaeresis
        accents[0x1e85] = 'w';	// latin small letter w with diaeresis
        accents[0x1e86] = 'w';	// latin capital letter w with dot above
        accents[0x1e87] = 'w';	// latin small letter w with dot above
        accents[0x1e88] = 'w';	// latin capital letter w with dot below
        accents[0x1e89] = 'w';	// latin small letter w with dot below
        accents[0x1e8a] = 'x';	// latin capital letter x with dot above
        accents[0x1e8b] = 'x';	// latin small letter x with dot above
        accents[0x1e8c] = 'x';	// latin capital letter x with diaeresis
        accents[0x1e8d] = 'x';	// latin small letter x with diaeresis
        accents[0x1e8e] = 'y';	// latin capital letter y with dot above
        accents[0x1e8f] = 'y';	// latin small letter y with dot above
        accents[0x1e90] = 'z';	// latin capital letter z with circumflex
        accents[0x1e91] = 'z';	// latin small letter z with circumflex
        accents[0x1e92] = 'z';	// latin capital letter z with dot below
        accents[0x1e93] = 'z';	// latin small letter z with dot below
        accents[0x1e94] = 'z';	// latin capital letter z with line below
        accents[0x1e95] = 'z';	// latin small letter z with line below
        accents[0x1e96] = 'h';	// latin small letter h with line below
        accents[0x1e97] = 't';	// latin small letter t with diaeresis
        accents[0x1e98] = 'w';	// latin small letter w with ring above
        accents[0x1e99] = 'y';	// latin small letter y with ring above
        accents[0x1e9a] = 'a';	// latin small letter a with right half ring
        accents[0x1e9b] = 's';	// latin small letter long s with dot above
        accents[0x1ea0] = 'a';	// latin capital letter a with dot below
        accents[0x1ea1] = 'a';	// latin small letter a with dot below
        accents[0x1ea2] = 'a';	// latin capital letter a with hook above
        accents[0x1ea3] = 'a';	// latin small letter a with hook above
        accents[0x1ea4] = 'a';	// latin capital letter a with circumflex and acute
        accents[0x1ea5] = 'a';	// latin small letter a with circumflex and acute
        accents[0x1ea6] = 'a';	// latin capital letter a with circumflex and grave
        accents[0x1ea7] = 'a';	// latin small letter a with circumflex and grave
        accents[0x1ea8] = 'a';	// latin capital letter a with circumflex and hook above
        accents[0x1ea9] = 'a';	// latin small letter a with circumflex and hook above
        accents[0x1eaa] = 'a';	// latin capital letter a with circumflex and tilde
        accents[0x1eab] = 'a';	// latin small letter a with circumflex and tilde
        accents[0x1eac] = 'a';	// latin capital letter a with circumflex and dot below
        accents[0x1ead] = 'a';	// latin small letter a with circumflex and dot below
        accents[0x1eae] = 'a';	// latin capital letter a with breve and acute
        accents[0x1eaf] = 'a';	// latin small letter a with breve and acute
        accents[0x1eb0] = 'a';	// latin capital letter a with breve and grave
        accents[0x1eb1] = 'a';	// latin small letter a with breve and grave
        accents[0x1eb2] = 'a';	// latin capital letter a with breve and hook above
        accents[0x1eb3] = 'a';	// latin small letter a with breve and hook above
        accents[0x1eb4] = 'a';	// latin capital letter a with breve and tilde
        accents[0x1eb5] = 'a';	// latin small letter a with breve and tilde
        accents[0x1eb6] = 'a';	// latin capital letter a with breve and dot below
        accents[0x1eb7] = 'a';	// latin small letter a with breve and dot below
        accents[0x1eb8] = 'e';	// latin capital letter e with dot below
        accents[0x1eb9] = 'e';	// latin small letter e with dot below
        accents[0x1eba] = 'e';	// latin capital letter e with hook above
        accents[0x1ebb] = 'e';	// latin small letter e with hook above
        accents[0x1ebc] = 'e';	// latin capital letter e with tilde
        accents[0x1ebd] = 'e';	// latin small letter e with tilde
        accents[0x1ebe] = 'e';	// latin capital letter e with circumflex and acute
        accents[0x1ebf] = 'e';	// latin small letter e with circumflex and acute
        accents[0x1ec0] = 'e';	// latin capital letter e with circumflex and grave
        accents[0x1ec1] = 'e';	// latin small letter e with circumflex and grave
        accents[0x1ec2] = 'e';	// latin capital letter e with circumflex and hook above
        accents[0x1ec3] = 'e';	// latin small letter e with circumflex and hook above
        accents[0x1ec4] = 'e';	// latin capital letter e with circumflex and tilde
        accents[0x1ec5] = 'e';	// latin small letter e with circumflex and tilde
        accents[0x1ec6] = 'e';	// latin capital letter e with circumflex and dot below
        accents[0x1ec7] = 'e';	// latin small letter e with circumflex and dot below
        accents[0x1ec8] = 'i';	// latin capital letter i with hook above
        accents[0x1ec9] = 'i';	// latin small letter i with hook above
        accents[0x1eca] = 'i';	// latin capital letter i with dot below
        accents[0x1ecb] = 'i';	// latin small letter i with dot below
        accents[0x1ecc] = 'o';	// latin capital letter o with dot below
        accents[0x1ecd] = 'o';	// latin small letter o with dot below
        accents[0x1ece] = 'o';	// latin capital letter o with hook above
        accents[0x1ecf] = 'o';	// latin small letter o with hook above
        accents[0x1ed0] = 'o';	// latin capital letter o with circumflex and acute
        accents[0x1ed1] = 'o';	// latin small letter o with circumflex and acute
        accents[0x1ed2] = 'o';	// latin capital letter o with circumflex and grave
        accents[0x1ed3] = 'o';	// latin small letter o with circumflex and grave
        accents[0x1ed4] = 'o';	// latin capital letter o with circumflex and hook above
        accents[0x1ed5] = 'o';	// latin small letter o with circumflex and hook above
        accents[0x1ed6] = 'o';	// latin capital letter o with circumflex and tilde
        accents[0x1ed7] = 'o';	// latin small letter o with circumflex and tilde
        accents[0x1ed8] = 'o';	// latin capital letter o with circumflex and dot below
        accents[0x1ed9] = 'o';	// latin small letter o with circumflex and dot below
        accents[0x1eda] = 'o';	// latin capital letter o with horn and acute
        accents[0x1edb] = 'o';	// latin small letter o with horn and acute
        accents[0x1edc] = 'o';	// latin capital letter o with horn and grave
        accents[0x1edd] = 'o';	// latin small letter o with horn and grave
        accents[0x1ede] = 'o';	// latin capital letter o with horn and hook above
        accents[0x1edf] = 'o';	// latin small letter o with horn and hook above
        accents[0x1ee0] = 'o';	// latin capital letter o with horn and tilde
        accents[0x1ee1] = 'o';	// latin small letter o with horn and tilde
        accents[0x1ee2] = 'o';	// latin capital letter o with horn and dot below
        accents[0x1ee3] = 'o';	// latin small letter o with horn and dot below
        accents[0x1ee4] = 'u';	// latin capital letter u with dot below
        accents[0x1ee5] = 'u';	// latin small letter u with dot below
        accents[0x1ee6] = 'u';	// latin capital letter u with hook above
        accents[0x1ee7] = 'u';	// latin small letter u with hook above
        accents[0x1ee8] = 'u';	// latin capital letter u with horn and acute
        accents[0x1ee9] = 'u';	// latin small letter u with horn and acute
        accents[0x1eea] = 'u';	// latin capital letter u with horn and grave
        accents[0x1eeb] = 'u';	// latin small letter u with horn and grave
        accents[0x1eec] = 'u';	// latin capital letter u with horn and hook above
        accents[0x1eed] = 'u';	// latin small letter u with horn and hook above
        accents[0x1eee] = 'u';	// latin capital letter u with horn and tilde
        accents[0x1eef] = 'u';	// latin small letter u with horn and tilde
        accents[0x1ef0] = 'u';	// latin capital letter u with horn and dot below
        accents[0x1ef1] = 'u';	// latin small letter u with horn and dot below
        accents[0x1ef2] = 'y';	// latin capital letter y with grave
        accents[0x1ef3] = 'y';	// latin small letter y with grave
        accents[0x1ef4] = 'y';	// latin capital letter y with dot below
        accents[0x1ef5] = 'y';	// latin small letter y with dot below
        accents[0x1ef6] = 'y';	// latin capital letter y with hook above
        accents[0x1ef7] = 'y';	// latin small letter y with hook above
        accents[0x1ef8] = 'y';	// latin capital letter y with tilde
        accents[0x1ef9] = 'y';	// latin small letter y with tilde
        accents[0x2071] = 'i';	// superscript latin small letter i
        accents[0x207f] = 'n';	// superscript latin small letter n
        accents[0x212a] = 'k';	// kelvin sign
        accents[0x212b] = 'a';	// angstrom sign
        accents[0x212c] = 'b';	// script capital b
        accents[0x212d] = 'c';	// black-letter capital c
        accents[0x212f] = 'e';	// script small e
        accents[0x2130] = 'e';	// script capital e
        accents[0x2131] = 'f';	// script capital f
        accents[0x2132] = 'f';	// turned capital f -- no decomposition
        accents[0x2133] = 'm';	// script capital m
        accents[0x2134] = '0';	// script small o
        accents[0x213a] = '0';	// rotated capital q -- no decomposition
        accents[0x2141] = 'g';	// turned sans-serif capital g -- no decomposition
        accents[0x2142] = 'l';	// turned sans-serif capital l -- no decomposition
        accents[0x2143] = 'l';	// reversed sans-serif capital l -- no decomposition
        accents[0x2144] = 'y';	// turned sans-serif capital y -- no decomposition
        accents[0x2145] = 'd';	// double-struck italic capital d
        accents[0x2146] = 'd';	// double-struck italic small d
        accents[0x2147] = 'e';	// double-struck italic small e
        accents[0x2148] = 'i';	// double-struck italic small i
        accents[0x2149] = 'j';	// double-struck italic small j
        accents[0xfb00] = 'ff';	// latin small ligature ff
        accents[0xfb01] = 'fi';	// latin small ligature fi
        accents[0xfb02] = 'fl';	// latin small ligature fl
//        accents[0xfb03] = 'ffi';	// latin small ligature ffi
//        accents[0xfb04] = 'ffl';	// latin small ligature ffl
        accents[0xfb05] = 'st';	// latin small ligature long s t
        accents[0xfb06] = 'st';	// latin small ligature st
        accents[0xff21] = 'a';	// fullwidth latin capital letter b
        accents[0xff22] = 'b';	// fullwidth latin capital letter b
        accents[0xff23] = 'c';	// fullwidth latin capital letter c
        accents[0xff24] = 'd';	// fullwidth latin capital letter d
        accents[0xff25] = 'e';	// fullwidth latin capital letter e
        accents[0xff26] = 'f';	// fullwidth latin capital letter f
        accents[0xff27] = 'g';	// fullwidth latin capital letter g
        accents[0xff28] = 'h';	// fullwidth latin capital letter h
        accents[0xff29] = 'i';	// fullwidth latin capital letter i
        accents[0xff2a] = 'j';	// fullwidth latin capital letter j
        accents[0xff2b] = 'k';	// fullwidth latin capital letter k
        accents[0xff2c] = 'l';	// fullwidth latin capital letter l
        accents[0xff2d] = 'm';	// fullwidth latin capital letter m
        accents[0xff2e] = 'n';	// fullwidth latin capital letter n
        accents[0xff2f] = 'o';	// fullwidth latin capital letter o
        accents[0xff30] = 'p';	// fullwidth latin capital letter p
        accents[0xff31] = 'q';	// fullwidth latin capital letter q
        accents[0xff32] = 'r';	// fullwidth latin capital letter r
        accents[0xff33] = 's';	// fullwidth latin capital letter s
        accents[0xff34] = 't';	// fullwidth latin capital letter t
        accents[0xff35] = 'u';	// fullwidth latin capital letter u
        accents[0xff36] = 'v';	// fullwidth latin capital letter v
        accents[0xff37] = 'w';	// fullwidth latin capital letter w
        accents[0xff38] = 'x';	// fullwidth latin capital letter x
        accents[0xff39] = 'y';	// fullwidth latin capital letter y
        accents[0xff3a] = 'z';	// fullwidth latin capital letter z
        accents[0xff41] = 'a';	// fullwidth latin small letter a
        accents[0xff42] = 'b';	// fullwidth latin small letter b
        accents[0xff43] = 'c';	// fullwidth latin small letter c
        accents[0xff44] = 'd';	// fullwidth latin small letter d
        accents[0xff45] = 'e';	// fullwidth latin small letter e
        accents[0xff46] = 'f';	// fullwidth latin small letter f
        accents[0xff47] = 'g';	// fullwidth latin small letter g
        accents[0xff48] = 'h';	// fullwidth latin small letter h
        accents[0xff49] = 'i';	// fullwidth latin small letter i
        accents[0xff4a] = 'j';	// fullwidth latin small letter j
        accents[0xff4b] = 'k';	// fullwidth latin small letter k
        accents[0xff4c] = 'l';	// fullwidth latin small letter l
        accents[0xff4d] = 'm';	// fullwidth latin small letter m
        accents[0xff4e] = 'n';	// fullwidth latin small letter n
        accents[0xff4f] = 'o';	// fullwidth latin small letter o
        accents[0xff50] = 'p';	// fullwidth latin small letter p
        accents[0xff51] = 'q';	// fullwidth latin small letter q
        accents[0xff52] = 'r';	// fullwidth latin small letter r
        accents[0xff53] = 's';	// fullwidth latin small letter s
        accents[0xff54] = 't';	// fullwidth latin small letter t
        accents[0xff55] = 'u';	// fullwidth latin small letter u
        accents[0xff56] = 'v';	// fullwidth latin small letter v
        accents[0xff57] = 'w';	// fullwidth latin small letter w
        accents[0xff58] = 'x';	// fullwidth latin small letter x
        accents[0xff59] = 'y';	// fullwidth latin small letter y
        accents[0xff5a] = 'z';	// fullwidth latin small letter z
    }

    auto found = accents.find(c);
    if (found != accents.end()) return found->second;
    return iswupper(c) ? towlower(c) : c;
}

void Match::Draw(HDC hdc, const RectI &rr) const
{
    auto r = rr;

    auto normalText = Color::Text;
    auto highlightBk = Lighten(Color::TaskBackground, 64);
    auto propertyText = Emphasize(normalText);

    auto clrOld = SetTextColor(hdc, normalText);

    if (!_prefix.empty())
    {
        SizeI size;

        if (GetTextExtentPoint(hdc, _prefix.c_str(), _prefix.size(), &size))
        {
            auto y = r.top + (r.Height() - size.cy) / 2;

            SetTextColor(hdc, propertyText);
            ExtTextOut(hdc, r.left, y, 0, nullptr, _prefix.c_str(), _prefix.size(), nullptr);
            r.left += size.cx + 2;
            ExtTextOut(hdc, r.left, y, 0, nullptr, L":", 1, nullptr);
            r.left += 8;
            SetTextColor(hdc, normalText);
        }
    }

    if (_selection.empty())
    {
        SizeI size;

        if (GetTextExtentPoint(hdc, _text.c_str(), _text.size(), &size))
        {
            ExtTextOut(hdc, r.left, r.top + (r.Height() - size.cy) / 2, 0, nullptr, _text.c_str(), _text.size(), nullptr);
        }
    }
    else
    {
        auto lenLeft = _selection.begin;
        auto lenMid = _selection.end - lenLeft;
        auto lenRight = _text.size() - (lenLeft + lenMid);

        auto textLeft = _text.c_str();
        auto textMid = _text.c_str() + _selection.begin;
        auto textRight = _text.c_str() + _selection.end;

        SizeI extentLeft;
        SizeI extentMid;
        SizeI extentRight;

        GetTextExtentPoint(hdc, textLeft, lenLeft, &extentLeft);
        GetTextExtentPoint(hdc, textMid, lenMid, &extentMid);
        GetTextExtentPoint(hdc, textRight, lenRight, &extentRight);

        auto hight = Max(extentLeft.cy, extentMid.cy, extentRight.cy);
        auto y = r.top + (r.Height() - hight) / 2;

        if (lenMid > 0)
        {
            auto oldBkCol = SetBkColor(hdc, highlightBk);
            auto oldBkMode = SetBkMode(hdc, OPAQUE);

            ExtTextOut(hdc, r.left + extentLeft.cx, y, 0, nullptr, textMid, lenMid, nullptr);

            SetBkMode(hdc, oldBkMode);
            SetBkColor(hdc, oldBkCol);
        }

        if (lenLeft > 0)
        {
            ExtTextOut(hdc, r.left, y, 0, nullptr, textLeft, lenLeft, nullptr);
        }

        if (lenRight > 0)
        {
            ExtTextOut(hdc, r.left + extentLeft.cx + extentMid.cx, y, 0, nullptr, textRight, lenRight, nullptr);
        }

        /*auto lenEnd = _text - _selection.end;

        if (lenEnd > 0)
        {
        SetTextColor(hdc, normalText);
        auto x = _end > 0 ? positions[_end - 1] : 0;
        ExtTextOut(hdc, r.left + x, y, 0, nullptr, sz + _end, lenEnd, nullptr);
        }*/

    }

    SetTextColor(hdc, clrOld);
}

std::wstring::size_type find_close_bracket(const std::wstring &s, std::wstring::size_type off, wchar_t open_b, wchar_t close_b)
{
    int cnt = 0;

    for (auto i = off; i < s.length(); i++)
    {
        if (s[i] == open_b)
        {
            cnt++;
        }
        else if (s[i] == close_b)
        {
            cnt--;
            if (!cnt)
            {
                return i;
            }
        }
    }
    return std::wstring::npos;
}

static inline const wchar_t* next_delim(const wchar_t* text, wchar_t d)
{
    while (*text)
    {
        if (*text == d) return text;
        text++;
    }

    return text;
}

static inline bool one_of(wchar_t c, const wchar_t* chars)
{
    while (*chars)
    {
        if (*chars == c) return true;
        chars++;
    }

    return false;
}

static inline const wchar_t* next_delim(const wchar_t* text, const wchar_t *delims)
{
    while (*text)
    {
        if (one_of(*text, delims)) return text;
        text++;
    }

    return text;
}

int value_index(const std::wstring& val, const wchar_t *strings, int defValue, wchar_t delim)
{
    assert(!val.empty());
    assert(delim);

    auto idx = 0;
    auto delim_start = strings;
    auto delim_end = next_delim(strings, delim);
    auto strings_end = strings + wcslen(strings);

    while (delim_start < strings_end)
    {
        auto delimLen = delim_end - delim_start;

        if (delimLen == val.size() &&
            _wcsnicmp(delim_start, val.c_str(), delimLen) == 0)
        {
            return idx;
        }

        idx++;

        delim_start = delim_end + 1;
        delim_end = next_delim(delim_start, delim);
    }

    return defValue;
}

bool value_in_list(const std::wstring& val, const wchar_t *strings, wchar_t delim)
{
    return value_index(val, strings, -1, delim) >= 0;
}


std::vector<std::wstring> split_string(const std::wstring& strings, const wchar_t delim)
{
    std::vector<std::wstring> results;

    if (strings.find_first_not_of(delim) == std::wstring::npos)
    {
        if (!strings.empty()) results.push_back(strings);
        return results;
    }

    bool inQuotes = false;
    char quoteChar = 0;
    auto p = strings.c_str();

    std::wstring current;

    while (*p != 0)
    {
        if (is_quote(*p) && (quoteChar == 0 || quoteChar == *p))
        {
            current += *p;
            inQuotes = !inQuotes;
            quoteChar = inQuotes ? *p : 0;
        }
        else if (inQuotes || delim != *p)
        {
            current += *p;
        }
        else
        {
            current = Trim(current);

            if (!current.empty())
            {
                results.push_back(current);
            }

            current.clear();
        }

        p++;
    }

    current = Trim(current);

    if (!current.empty())
    {
        results.push_back(current);
    }

    return results;
}

std::vector<std::wstring> split_string(const std::wstring& strings, const wchar_t *delims, const wchar_t *quote)
{
    //ATLTRACE(L"Split %s \n", strings.c_str());

    std::vector<std::wstring> results;

    if (strings.find_first_not_of(delims) == std::wstring::npos)
    {
        if (!strings.empty()) results.push_back(strings);
        return results;
    }

    if (!strings.empty())
    {
        bool inQuotes = false;
        wchar_t quoteChar = 0;
        auto p = strings.c_str();

        std::wstring current;

        while (*p != 0)
        {
            if (one_of(*p, quote) && (quoteChar == 0 || quoteChar == *p))
            {
                current += *p;
                inQuotes = !inQuotes;

                if (inQuotes)
                {
                    quoteChar = *p;
                    if (quoteChar == L'(') quoteChar = L')';
                    if (quoteChar == L'[') quoteChar = L']';
                }
                else
                {
                    quoteChar = 0;
                }
            }
            else if (inQuotes || !one_of(*p, delims))
            {
                current += *p;
            }
            else
            {
                current = Trim(current);
                if (!current.empty()) results.push_back(current);
                current.clear();
            }

            p++;
        }

        current = Trim(current);
        if (!current.empty()) results.push_back(current);
    }

    return results;
}


//std::vector<std::wstring> split_string(const std::wstring& str, const std::wstring& delims, const std::wstring& delims_preserve, const std::wstring& quote)
//{
//    std::vector<std::wstring> tokens;
//
//    if (str.empty() || (delims.empty() && delims_preserve.empty()))
//    {
//        return tokens;
//    }
//
//    std::wstring all_delims = delims + delims_preserve + quote;
//
//    std::wstring::size_type token_start = 0;
//    std::wstring::size_type token_end = str.find_first_of(all_delims, token_start);
//    std::wstring::size_type token_len = 0;
//
//    std::wstring token;
//    
//    while (true)
//    {
//        while (token_end != std::wstring::npos && quote.find_first_of(str[token_end]) != std::wstring::npos)
//        {
//            if (str[token_end] == _t('('))
//            {
//                token_end = find_close_bracket(str, token_end, _t('('), _t(')'));
//            }
//            else if (str[token_end] == _t('['))
//            {
//                token_end = find_close_bracket(str, token_end, _t('['), _t(']'));
//            }
//            else if (str[token_end] == _t('{'))
//            {
//                token_end = find_close_bracket(str, token_end, _t('{'), _t('}'));
//            }
//            else
//            {
//                token_end = str.find_first_of(str[token_end], token_end + 1);
//            }
//            if (token_end != std::wstring::npos)
//            {
//                token_end = str.find_first_of(all_delims, token_end + 1);
//            }
//        }
//
//        if (token_end == std::wstring::npos)
//        {
//            token_len = std::wstring::npos;
//        }
//        else
//        {
//            token_len = token_end - token_start;
//        }
//
//        token = str.substr(token_start, token_len);
//        if (!token.empty())
//        {
//            tokens.push_back(token);
//        }
//        if (token_end != std::wstring::npos && !delims_preserve.empty() && delims_preserve.find_first_of(str[token_end]) != std::wstring::npos)
//        {
//            tokens.push_back(str.substr(token_end, 1));
//        }
//
//        token_start = token_end;
//        if (token_start == std::wstring::npos) break;
//        token_start++;
//        if (token_start == str.length()) break;
//        token_end = str.find_first_of(all_delims, token_start);
//    }
//
//    return tokens;
//}