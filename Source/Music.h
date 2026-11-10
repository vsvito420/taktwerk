#pragma once
#include <juce_core/juce_core.h>
#include <atomic>

// Scales, styles and the session that all Taktwerk instances share.
namespace tw
{
enum Style { Techno, HardTechno, Psytrance, Hardstyle, Uptempo, Trance, House, numStyles };
inline const char* const styleNames[numStyles] = { "Techno", "Hard Techno", "Psytrance", "Hardstyle", "Uptempo", "Trance", "House" };

enum Type { Drums, Bass, Synth, Pad, numTypes };
inline const char* const typeNames[numTypes] = { "Drums", "Bass", "Synth", "Pad" };

enum : unsigned { T = 1, H = 2, P = 4, S = 8, U = 16, R = 32, O = 64 };

struct ScaleDef { const char* name; int n; int iv[12]; unsigned styles; const char* mood; };

inline const ScaleDef scales[] = {
    { "Major (Ionian)",        7, { 0,2,4,5,7,9,11 },          R|O,         "bright, happy" },
    { "Natural Minor",         7, { 0,2,3,5,7,8,10 },          T|H|P|S|U|R|O, "dark, the safe choice" },
    { "Dorian",                7, { 0,2,3,5,7,9,10 },          T|O|R,       "minor but groovy" },
    { "Phrygian",              7, { 0,1,3,5,7,8,10 },          T|H|P|S|U,   "very dark, tense" },
    { "Lydian",                7, { 0,2,4,6,7,9,11 },          0,           "dreamy, floating" },
    { "Mixolydian",            7, { 0,2,4,5,7,9,10 },          O,           "bright, funky" },
    { "Locrian",               7, { 0,1,3,5,6,8,10 },          T|H,         "unstable, evil" },
    { "Harmonic Minor",        7, { 0,2,3,5,7,8,11 },          H|P|S|U|R,   "dramatic, classic rave" },
    { "Melodic Minor",         7, { 0,2,3,5,7,9,11 },          0,           "smooth, jazzy minor" },
    { "Phrygian Dominant",     7, { 0,1,4,5,7,8,10 },          H|P|U,       "oriental, aggressive" },
    { "Hungarian Minor",       7, { 0,2,3,6,7,8,11 },          U,           "gypsy, sinister" },
    { "Double Harmonic",       7, { 0,1,4,5,7,8,11 },          U|P,         "arabic, extreme" },
    { "Harmonic Major",        7, { 0,2,4,5,7,8,11 },          0,           "bright with a dark turn" },
    { "Dorian #4",             7, { 0,2,3,6,7,9,10 },          0,           "eastern, mysterious" },
    { "Lydian Dominant",       7, { 0,2,4,6,7,9,10 },          0,           "spacey, acid" },
    { "Lydian Augmented",      7, { 0,2,4,6,8,9,11 },          0,           "strange, sci-fi" },
    { "Altered (Super Locrian)",7,{ 0,1,3,4,6,8,10 },          0,           "very tense" },
    { "Locrian #2",            7, { 0,2,3,5,6,8,10 },          0,           "dark, half-diminished" },
    { "Mixolydian b6",         7, { 0,2,4,5,7,8,10 },          0,           "epic, bittersweet" },
    { "Dorian b2",             7, { 0,1,3,5,7,9,10 },          0,           "dark with a lift" },
    { "Neapolitan Minor",      7, { 0,1,3,5,7,8,11 },          0,           "dramatic, horror" },
    { "Neapolitan Major",      7, { 0,1,3,5,7,9,11 },          0,           "odd, cinematic" },
    { "Persian",               7, { 0,1,4,5,6,8,11 },          0,           "exotic, harsh" },
    { "Enigmatic",             7, { 0,1,4,6,8,10,11 },         0,           "weird, experimental" },
    { "Minor Pentatonic",      5, { 0,3,5,7,10 },              T|O,         "simple, always works" },
    { "Major Pentatonic",      5, { 0,2,4,7,9 },               0,           "simple, bright" },
    { "Blues",                 6, { 0,3,5,6,7,10 },            0,           "gritty" },
    { "Major Blues",           6, { 0,2,3,4,7,9 },             0,           "playful" },
    { "Egyptian",              5, { 0,2,5,7,10 },              0,           "open, hypnotic" },
    { "Hirajoshi",             5, { 0,2,3,7,8 },               0,           "japanese, dark" },
    { "In Sen",                5, { 0,1,5,7,10 },              0,           "japanese, tense" },
    { "Iwato",                 5, { 0,1,5,6,10 },              0,           "japanese, dissonant" },
    { "Kumoi",                 5, { 0,2,3,7,9 },               0,           "japanese, gentle" },
    { "Pelog",                 5, { 0,1,3,7,8 },               0,           "gamelan, dark" },
    { "Yo",                    5, { 0,2,5,7,9 },               0,           "folk, open" },
    { "Whole Tone",            6, { 0,2,4,6,8,10 },            0,           "dreamlike, no center" },
    { "Augmented",             6, { 0,3,4,7,8,11 },            0,           "unsettling" },
    { "Prometheus",            6, { 0,2,4,6,9,10 },            0,           "mystic" },
    { "Tritone",               6, { 0,1,4,6,7,10 },            0,           "harsh, industrial" },
    { "Diminished (W-H)",      8, { 0,2,3,5,6,8,9,11 },        0,           "tense, symmetric" },
    { "Diminished (H-W)",      8, { 0,1,3,4,6,7,9,10 },        0,           "tense, symmetric" },
    { "Bebop Dominant",        8, { 0,2,4,5,7,9,10,11 },       0,           "jazzy" },
    { "Bebop Major",           8, { 0,2,4,5,7,8,9,11 },        0,           "jazzy" },
    { "Spanish 8-Tone",        8, { 0,1,3,4,5,6,8,10 },        0,           "flamenco, dark" },
    { "Chromatic",            12, { 0,1,2,3,4,5,6,7,8,9,10,11 }, 0,         "all notes, no help" },
};
inline constexpr int numScales = (int) (sizeof (scales) / sizeof (scales[0]));
inline const char* const noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

inline int floorDiv (int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }

inline int degreeToSemis (int scale, int degree)
{
    const auto& s = scales[juce::jlimit (0, numScales - 1, scale)];
    int oct = floorDiv (degree, s.n);
    return s.iv[degree - oct * s.n] + 12 * oct;
}

inline int semisToDegree (int scale, int semis)
{
    const auto& s = scales[juce::jlimit (0, numScales - 1, scale)];
    int oct = floorDiv (semis, 12), pc = semis - 12 * oct, best = 0, bestDist = 99;
    for (int i = 0; i <= s.n; ++i)
    {
        int v = i == s.n ? 12 : s.iv[i];
        if (std::abs (v - pc) < bestDist) { bestDist = std::abs (v - pc); best = i; }
    }
    return oct * s.n + best;
}

inline bool inScale (int scale, int root, int note)
{
    const auto& s = scales[juce::jlimit (0, numScales - 1, scale)];
    int pc = ((note - root) % 12 + 12) % 12;
    for (int i = 0; i < s.n; ++i) if (s.iv[i] == pc) return true;
    return false;
}

inline juce::String chordName (int scale, int root, int degree)
{
    int r = degreeToSemis (scale, degree);
    int third = degreeToSemis (scale, degree + 2) - r, fifth = degreeToSemis (scale, degree + 4) - r;
    juce::String n = noteNames[((root + r) % 12 + 12) % 12];
    if (scales[scale].n != 7) return n;
    if (third == 3 && fifth == 6) return n + "dim";
    if (third == 4 && fifth == 8) return n + "aug";
    if (third == 3 && fifth == 7) return n + "m";
    if (third == 4 && fifth == 7) return n;
    return n + "*";
}

inline juce::String romanName (int scale, int degree)
{
    static const char* up[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lo[] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    if (scales[scale].n != 7) return juce::String (degree + 1);
    int third = degreeToSemis (scale, degree + 2) - degreeToSemis (scale, degree);
    return third == 3 ? lo[degree % 7] : up[degree % 7];
}

inline const char* const sectionNames[] = { "Intro", "Build", "Drop", "Break", "Drop 2", "Outro", "Verse", "Bridge", "Fill", "Climax" };
inline constexpr int numSectionNames = 10;

struct ProgPreset { const char* name; int len; int deg[8]; };
inline const ProgPreset progPresets[] = {
    { "Static (one chord)",     1, { 0,0,0,0,0,0,0,0 } },
    { "1 - 6",                  2, { 0,5,0,5,0,5,0,5 } },
    { "1 - 1 - 6 - 7",          4, { 0,0,5,6,0,0,5,6 } },
    { "1 - 6 - 3 - 7",          4, { 0,5,2,6,0,5,2,6 } },
    { "1 - 7 - 6 - 7",          4, { 0,6,5,6,0,6,5,6 } },
    { "1 - 4 - 6 - 5",          4, { 0,3,5,4,0,3,5,4 } },
    { "1 - 1 - 2 - 1 (dark)",   4, { 0,0,1,0,0,0,1,0 } },
    { "1 - 6 - 4 - 5",          4, { 0,5,3,4,0,5,3,4 } },
    { "8 bars: 1 1 6 6 3 3 7 7",8, { 0,0,5,5,2,2,6,6 } },
};
inline constexpr int numProgPresets = (int) (sizeof (progPresets) / sizeof (progPresets[0]));

// One per process. Every Taktwerk instance in the project reads and writes it.
struct Session
{
    enum { maxSections = 8, maxTracks = 8 };
    std::atomic<int> root { 9 }, scale { 1 }, style { Techno }, progLen { 4 }, follow { 0 }, revision { 0 };
    std::atomic<float> swing { 0.0f };
    std::atomic<int> prog[8];
    std::atomic<int> secName[maxSections], secBars[maxSections];
    std::atomic<int> cell[maxTracks][maxSections];
    std::atomic<int> trackType[maxTracks];
    std::atomic<int> hist[12];

    Session()
    {
        static const int p[8] = { 0,0,5,6,0,0,5,6 }, names[8] = { 0,1,2,3,4,5,0,0 }, bars[8] = { 8,8,16,8,16,8,0,0 };
        for (int i = 0; i < 8; ++i) { prog[i] = p[i]; secName[i] = names[i]; secBars[i] = bars[i]; }
        for (auto& row : cell) for (auto& c : row) c = 1;
        for (auto& t : trackType) t = 0;
        for (auto& h : hist) h = 0;
    }

    void bump() { revision.fetch_add (1); }
    int totalBars() const { int t = 0; for (auto& b : secBars) t += b.load(); return t; }

    // Section that contains the bar (arrangement loops). -1 if there are no sections.
    int sectionForBar (int bar) const
    {
        int total = totalBars();
        if (total <= 0) return -1;
        bar %= total;
        for (int i = 0; i < maxSections; ++i) { int b = secBars[i].load(); if (bar < b) return i; bar -= b; }
        return -1;
    }

    void toXml (juce::XmlElement& x) const
    {
        x.setAttribute ("root", root.load()); x.setAttribute ("scale", juce::String (scales[scale.load()].name));
        x.setAttribute ("style", style.load()); x.setAttribute ("progLen", progLen.load());
        x.setAttribute ("follow", follow.load()); x.setAttribute ("swing", (double) swing.load());
        juce::String p, sn, sb, cl;
        for (int i = 0; i < 8; ++i) { p << prog[i].load() << " "; sn << secName[i].load() << " "; sb << secBars[i].load() << " "; }
        for (auto& row : cell) for (auto& c : row) cl << c.load() << " ";
        x.setAttribute ("prog", p); x.setAttribute ("secName", sn); x.setAttribute ("secBars", sb); x.setAttribute ("cells", cl);
    }

    void fromXml (const juce::XmlElement& x)
    {
        root = juce::jlimit (0, 11, x.getIntAttribute ("root", 9));
        auto sc = x.getStringAttribute ("scale");
        for (int i = 0; i < numScales; ++i) if (sc == scales[i].name) scale = i;
        style = juce::jlimit (0, numStyles - 1, x.getIntAttribute ("style", 0));
        progLen = juce::jlimit (1, 8, x.getIntAttribute ("progLen", 4));
        follow = x.getIntAttribute ("follow", 0);
        swing = (float) x.getDoubleAttribute ("swing", 0.0);
        auto p = juce::StringArray::fromTokens (x.getStringAttribute ("prog"), " ", "");
        auto sn = juce::StringArray::fromTokens (x.getStringAttribute ("secName"), " ", "");
        auto sb = juce::StringArray::fromTokens (x.getStringAttribute ("secBars"), " ", "");
        auto cl = juce::StringArray::fromTokens (x.getStringAttribute ("cells"), " ", "");
        for (int i = 0; i < 8; ++i)
        {
            if (i < p.size())  prog[i] = juce::jlimit (0, 11, p[i].getIntValue());
            if (i < sn.size()) secName[i] = juce::jlimit (0, numSectionNames - 1, sn[i].getIntValue());
            if (i < sb.size()) secBars[i] = juce::jlimit (0, 64, sb[i].getIntValue());
        }
        for (int t = 0; t < maxTracks; ++t) for (int s = 0; s < maxSections; ++s)
            if (t * maxSections + s < cl.size()) cell[t][s] = juce::jlimit (0, 4, cl[t * maxSections + s].getIntValue());
        bump();
    }
};
}
