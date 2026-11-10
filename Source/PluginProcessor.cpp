#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tw;

namespace
{
uint32_t pack (const StepData& s)
{
    return (s.on ? 1u : 0u) | ((uint32_t) juce::jlimit (1, 127, s.vel) << 1)
         | ((uint32_t) (juce::jlimit (-64, 63, s.degree) + 64) << 8) | ((uint32_t) juce::jlimit (1, 16, s.len) << 16);
}
StepData unpack (uint32_t v)
{
    StepData s; s.on = (v & 1u) != 0; s.vel = (int) ((v >> 1) & 127u); s.degree = (int) ((v >> 8) & 255u) - 64; s.len = juce::jmax (1, (int) ((v >> 16) & 255u));
    return s;
}

// X = always, x = depends on DENSITY, o = depends on COMPLEXITY, - = never
const char* const drumTpl[numStyles][6] = {
    { "X---X---X---X---", "----X-------X---", "x-oxx-oxx-oxx-ox", "--X---X---X---X-", "---o--x---o--x-o", "--o---o---o---o-" },
    { "X---X---X---X--o", "----X-------X--o", "xxoxxxoxxxoxxxox", "--X---X---X---X-", "o--x--o--x-o--x-", "x-x-x-x-x-x-x-x-" },
    { "X---X---X---X---", "----X-------X---", "xxoxxxoxxxoxxxox", "--X---X---X---X-", "-o-x-o---o-x--o-", "----------------" },
    { "X---X---X---X---", "----X-------X---", "o-x-o-x-o-x-o-x-", "--X---X---X---X-", "---o---o---o-ooo", "x-x-x-x-x-x-x-x-" },
    { "X-o-X---X-o-X-oo", "----X-------X---", "--x---x---x---x-", "--X---X---X---X-", "---o---o---o--oo", "x-x-x-x-x-x-x-x-" },
    { "X---X---X---X---", "----X-------X---", "o-x-o-x-o-x-o-xo", "--X---X---X---X-", "---x--o---x--o--", "o-o-o-o-o-o-o-o-" },
    { "X---X---X---X---", "----X-------X---", "x-x-x-x-x-x-x-xo", "--X---X---X---X-", "-o-x--o--x-o--x-", "----------------" },
};
const char* const bassTpl[numStyles] = {
    "--Xo--X---Xo--Xo", "-xXx-xXx-xXx-xXx", "-XXX-XXX-XXX-XXX", "--X---X---X---Xo", "-xXx-xXx-xXx-xXx", "-xXx-xXx-xXx-xXx", "X--x--X-o-x-X-o-" };
const char* const synthTpl[numStyles] = {
    "X--x--x-o--x--xo", "X--X--X-X--X--Xo", "XxxXxxXxXxxXxxXx", "X-x-X-x-X-xxX-x-", "X-x-X-x-X-x-XoXo", "XxxXxxXxXxxXxxXx", "X--x--x---x-x-o-" };

bool roll (juce::Random& r, char c, float density, float complexity)
{
    float p = c == 'X' ? 1.0f : c == 'x' ? 0.25f + 0.75f * density : c == 'o' ? complexity * 0.7f : 0.0f;
    return r.nextFloat() < p;
}
int tplVel (juce::Random& r, char c) { return (c == 'X' ? 110 : c == 'x' ? 90 : 66) + r.nextInt (9) - 4; }
int clampDeg (int d) { return juce::jlimit ((int) TaktwerkProcessor::degMin, (int) TaktwerkProcessor::degMax, d); }
}

juce::AudioProcessorValueTreeState::ParameterLayout TaktwerkProcessor::layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "track", 1 }, "Track", StringArray { "1", "2", "3", "4", "5", "6", "7", "8" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Type", StringArray { "Drums", "Bass", "Synth", "Pad" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "density", 1 }, "Density", 0.0f, 1.0f, 0.6f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "complexity", 1 }, "Complexity", 0.0f, 1.0f, 0.3f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize", 0.0f, 1.0f, 0.15f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Note Length", 0.1f, 1.0f, 0.8f));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chord", 1 }, "Chord", StringArray { "Single note", "Power (1+5)", "Triad", "7th chord" }, 0));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 1));
    return l;
}

TaktwerkProcessor::TaktwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", layout())
{
    pType = apvts.getRawParameterValue ("type");       pTrack = apvts.getRawParameterValue ("track");
    pDensity = apvts.getRawParameterValue ("density"); pComplexity = apvts.getRawParameterValue ("complexity");
    pHumanize = apvts.getRawParameterValue ("humanize"); pGate = apvts.getRawParameterValue ("gate");
    pOctave = apvts.getRawParameterValue ("octave");   pChord = apvts.getRawParameterValue ("chord");
    pChannel = apvts.getRawParameterValue ("channel");
    static const int gm[numLanes] = { 36, 39, 42, 46, 37, 51 };
    for (int i = 0; i < numLanes; ++i) laneNote[i] = gm[i];
    for (auto& a : steps) for (auto& b : a) for (auto& c : b) c = pack ({});
    for (int s = 0; s < numSlots; ++s) generate (s);
}

TaktwerkProcessor::~TaktwerkProcessor() { session->trackType[track()] = 0; session->bump(); }

void TaktwerkProcessor::prepareToPlay (double sr, int) { sampleRate = sr; wasPlaying = false; for (auto& p : pending) p.note = -1; for (auto& h : held) h.k = -1; }

StepData TaktwerkProcessor::getStep (int slot, int lane, int step) const { return unpack (steps[slot][lane][step].load (std::memory_order_relaxed)); }
void TaktwerkProcessor::setStep (int slot, int lane, int step, const StepData& s) { steps[slot][lane][step].store (pack (s), std::memory_order_relaxed); patternRev.fetch_add (1); }
void TaktwerkProcessor::clearSlot (int slot) { for (auto& l : steps[slot]) for (auto& s : l) s = pack ({}); patternRev.fetch_add (1); }

int TaktwerkProcessor::slotForBar (int bar) const
{
    if (session->follow.load() == 0) return curSlot.load();
    int sec = session->sectionForBar (bar);
    if (sec < 0) return curSlot.load();
    return session->cell[track()][sec].load() - 1;
}

static int baseNote (int type, int root, int octave)
{
    int base = type == Bass ? 36 : type == Pad ? 48 : 60;
    return base + (root > 6 ? root - 12 : root) + 12 * octave;
}

int TaktwerkProcessor::notesForStep (int slot, int lane, int step, int bar, int* notes, int& vel, double& lenSteps) const
{
    auto sd = getStep (slot, lane, step);
    if (! sd.on) return 0;
    vel = sd.vel;
    const int t = type();
    if (t == Drums) { notes[0] = laneNote[lane].load(); lenSteps = 0.5; return 1; }
    if (lane != 0) return 0;

    int len = juce::jmin (sd.len, (int) numSteps - step);
    for (int i = 1; i < len; ++i) if (getStep (slot, 0, step + i).on) { len = i; break; }
    lenSteps = len * (double) pGate->load();

    const int scale = session->scale.load(), root = session->root.load();
    const int chordDeg = session->prog[bar % juce::jmax (1, session->progLen.load())].load();
    static const int offs[4][4] = { { 0, 0, 0, 0 }, { 0, 4, 0, 0 }, { 0, 2, 4, 0 }, { 0, 2, 4, 6 } };
    const int kind = juce::jlimit (0, 3, (int) pChord->load()), count = kind + 1;
    const int base = baseNote (t, root, (int) pOctave->load());
    for (int i = 0; i < count; ++i)
        notes[i] = juce::jlimit (0, 127, base + degreeToSemis (scale, chordDeg + sd.degree + offs[kind][i]));
    return count;
}

void TaktwerkProcessor::flushNotes (juce::MidiBuffer& midi, int sample)
{
    for (auto& p : pending) if (p.note >= 0) { midi.addEvent (juce::MidiMessage::noteOff (p.ch, p.note), sample); p.note = -1; }
}

void TaktwerkProcessor::noteOn (juce::MidiBuffer& midi, int sample, int note, int vel, double offPpq)
{
    const int ch = (int) pChannel->load();
    Pending* slot = nullptr;
    for (auto& p : pending)
    {
        if (p.note == note) { midi.addEvent (juce::MidiMessage::noteOff (p.ch, p.note), sample); p.note = -1; }
        if (p.note < 0 && slot == nullptr) slot = &p;
    }
    if (slot == nullptr) return;
    midi.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, vel)), sample);
    *slot = { note, ch, offPpq };
}

void TaktwerkProcessor::learnNote (const juce::MidiMessage& m, double ppq)
{
    const int n = m.getNoteNumber(), t = type(), slot = curSlot.load();
    const long k = std::lround (ppq * 4.0);
    if (m.isNoteOn())
    {
        if (k < 0) return;
        const int step = (int) (k % numSteps), bar = (int) (k / numSteps);
        StepData sd; sd.on = true; sd.vel = m.getVelocity(); sd.len = 1;
        int lane = 0;
        if (t == Drums)
        {
            int best = 999;
            for (int l = 0; l < numLanes; ++l) { int d = std::abs (laneNote[l].load() - n); if (d < best) { best = d; lane = l; } }
        }
        else
        {
            const int chordDeg = session->prog[bar % juce::jmax (1, session->progLen.load())].load();
            const int semis = n - baseNote (t, session->root.load(), (int) pOctave->load());
            sd.degree = clampDeg (semisToDegree (session->scale.load(), semis) - chordDeg);
        }
        setStep (slot, lane, step, sd);
        held[n] = { k, lane };
    }
    else if (m.isNoteOff() && held[n].k >= 0)
    {
        if (t != Drums)
        {
            const int step = (int) (held[n].k % numSteps);
            auto sd = getStep (slot, 0, step);
            if (sd.on) { sd.len = (int) juce::jlimit (1L, 16L, k - held[n].k); setStep (slot, 0, step, sd); }
        }
        held[n].k = -1;
    }
}

void TaktwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples(), t = type(), tr = track();
    auto& s = *session;
    if (s.trackType[tr].load() != t + 1) { s.trackType[tr] = t + 1; s.bump(); }

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm().orFallback (120.0);
            ppq = pos->getPpqPosition().orFallback (0.0);
            playing = pos->getIsPlaying();
        }
    const double ppqPerSample = bpm / (60.0 * sampleRate);
    const double ppqEnd = ppq + numSamples * ppqPerSample;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && t != Drums) { s.hist[m.getNoteNumber() % 12].fetch_add (1); s.bump(); }
        if (learn.load() != 0 && playing && (m.isNoteOn() || m.isNoteOff())) learnNote (m, ppq + meta.samplePosition * ppqPerSample);
    }
    midi.clear();

    if (! playing)
    {
        if (wasPlaying) flushNotes (midi, 0);
        wasPlaying = false; uiStep = -1;
        return;
    }
    if (wasPlaying && std::abs (ppq - expectedPpq) > 0.02) flushNotes (midi, 0);
    wasPlaying = true; expectedPpq = ppqEnd;

    const double swingOff = (double) s.swing.load() * 0.25 / 3.0;
    const float humanize = pHumanize->load();
    const long k0 = (long) std::floor (ppq * 4.0) - 1, k1 = (long) std::floor (ppqEnd * 4.0) + 1;
    for (long k = juce::jmax (0L, k0); k <= k1; ++k)
    {
        const double tk = k * 0.25 + ((k & 1) ? swingOff : 0.0);
        if (tk < ppq || tk >= ppqEnd) continue;
        const int bar = (int) (k / numSteps), step = (int) (k % numSteps), slot = slotForBar (bar);
        if (slot < 0) continue;
        const int sample = juce::jlimit (0, numSamples - 1, (int) ((tk - ppq) / ppqPerSample));
        for (int lane = 0; lane < (t == Drums ? (int) numLanes : 1); ++lane)
        {
            int notes[4], vel = 100; double len = 1.0;
            const int n = notesForStep (slot, lane, step, bar, notes, vel, len);
            if (n == 0) continue;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            vel += (int) (((rng & 0xffff) / 65535.0f - 0.5f) * 60.0f * humanize);
            for (int i = 0; i < n; ++i) noteOn (midi, sample, notes[i], vel, tk + juce::jmax (0.03, len * 0.25));
        }
    }
    for (auto& p : pending)
        if (p.note >= 0 && p.off < ppqEnd)
        {
            midi.addEvent (juce::MidiMessage::noteOff (p.ch, p.note), juce::jlimit (0, numSamples - 1, (int) ((p.off - ppq) / ppqPerSample)));
            p.note = -1;
        }

    const long kNow = (long) std::floor (juce::jmax (0.0, ppq) * 4.0);
    uiStep = (int) (kNow % numSteps); uiBar = (int) (kNow / numSteps);
}

//==============================================================================
void TaktwerkProcessor::generate (int slot)
{
    juce::Random r (juce::Random::getSystemRandom().nextInt64());
    const int t = type(), style = juce::jlimit (0, numStyles - 1, session->style.load());
    const float density = pDensity->load(), complexity = pComplexity->load();
    const int n = scales[session->scale.load()].n;
    clearSlot (slot);
    auto put = [&] (int lane, int step, int vel, int degree, int len) { StepData sd; sd.on = true; sd.vel = vel; sd.degree = clampDeg (degree); sd.len = len; setStep (slot, lane, step, sd); };

    if (t == Drums)
    {
        for (int lane = 0; lane < numLanes; ++lane)
            for (int i = 0; i < numSteps; ++i)
            {
                char c = drumTpl[style][lane][i];
                if (roll (r, c, density, complexity)) put (lane, i, tplVel (r, c), 0, 1);
            }
    }
    else if (t == Bass)
    {
        const int alt[] = { n, 4, 2, -1, 3 };
        for (int i = 0; i < numSteps; ++i)
        {
            char c = bassTpl[style][i];
            if (! roll (r, c, density, complexity)) continue;
            int deg = (r.nextFloat() < complexity * 0.45f) ? alt[r.nextInt (5)] : 0;
            put (0, i, tplVel (r, c), deg, style == House ? 2 : 1);
        }
    }
    else if (t == Synth)
    {
        const int chordTones[] = { 0, 2, 4, n, 0, 4 }, passing[] = { 1, 3, 5, 6, -1, -3 };
        int firstHalf[8];
        for (int i = 0; i < numSteps; ++i)
        {
            char c = synthTpl[style][i];
            int deg = (r.nextFloat() < complexity * 0.6f) ? passing[r.nextInt (6)] : chordTones[r.nextInt (6)];
            if (i == 0) deg = 0;
            if (i < 8) firstHalf[i] = deg;
            else if (r.nextFloat() > 0.2f + complexity * 0.5f) deg = firstHalf[i - 8];
            if (roll (r, c, density, complexity)) put (0, i, tplVel (r, c), deg, 1);
        }
    }
    else
    {
        if (density > 0.75f) { for (int i : { 0, 3, 6, 8, 11, 14 }) put (0, i, 88, 0, 2); }
        else if (complexity > 0.6f) { put (0, 0, 88, 0, 8); put (0, 8, 84, 0, 8); }
        else put (0, 0, 88, 0, 16);
    }
}

void TaktwerkProcessor::vary (int from, int to)
{
    if (from == to) return;
    juce::Random r (juce::Random::getSystemRandom().nextInt64());
    for (int l = 0; l < numLanes; ++l) for (int i = 0; i < numSteps; ++i) steps[to][l][i] = steps[from][l][i].load();
    const int t = type(), changes = 2 + (int) (pComplexity->load() * 6.0f);
    for (int c = 0; c < changes; ++c)
    {
        if (t == Drums)
        {
            int lane = 1 + r.nextInt (numLanes - 1), step = c < 2 ? 12 + r.nextInt (4) : r.nextInt (numSteps);
            auto sd = getStep (to, lane, step); sd.on = ! sd.on; sd.vel = 78 + r.nextInt (20); setStep (to, lane, step, sd);
        }
        else
        {
            int step = r.nextInt (numSteps);
            auto sd = getStep (to, 0, step);
            if (sd.on && r.nextFloat() < 0.7f) sd.degree = clampDeg (sd.degree + (r.nextBool() ? 1 : -1) * (1 + r.nextInt (2)));
            else if (t != Pad) { sd.on = ! sd.on; sd.vel = 85; sd.len = 1; sd.degree = r.nextBool() ? 0 : 4; }
            setStep (to, 0, step, sd);
        }
    }
    patternRev.fetch_add (1);
}

void TaktwerkProcessor::setTypeFromUser (int newType)
{
    auto set = [this] (const char* id, int v) { auto* p = apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 ((float) v)); };
    session->trackType[track()] = 0;
    set ("type", newType);
    set ("chord", newType == Pad ? 2 : 0);
    for (int s = 0; s < numSlots; ++s) generate (s);
}

juce::File TaktwerkProcessor::writeMidiFile (bool wholeSong) const
{
    juce::MidiMessageSequence seq;
    const int ch = (int) pChannel->load(), t = type();
    const int bars = wholeSong ? session->totalBars() : (t == Drums ? 1 : session->progLen.load());
    const double swingOff = (double) session->swing.load() * 0.25 / 3.0;
    for (int bar = 0; bar < bars; ++bar)
    {
        int slot = curSlot.load();
        if (wholeSong) { int sec = session->sectionForBar (bar); slot = sec < 0 ? -1 : session->cell[track()][sec].load() - 1; }
        if (slot < 0) continue;
        for (int step = 0; step < numSteps; ++step)
            for (int lane = 0; lane < numLanes; ++lane)
            {
                int notes[4], vel = 100; double len = 1.0;
                const int n = notesForStep (slot, lane, step, bar, notes, vel, len);
                const double start = (bar * numSteps + step) * 0.25 + ((step & 1) ? swingOff : 0.0);
                for (int i = 0; i < n; ++i)
                {
                    seq.addEvent (juce::MidiMessage::noteOn (ch, notes[i], (juce::uint8) vel), start * 960.0);
                    seq.addEvent (juce::MidiMessage::noteOff (ch, notes[i]), (start + len * 0.25) * 960.0 - 1.0);
                }
            }
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf; mf.setTicksPerQuarterNote (960); mf.addTrack (seq);
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Taktwerk");
    dir.createDirectory();
    auto name = juce::String ("Taktwerk T") + juce::String (track() + 1) + " " + typeNames[t] + (wholeSong ? " Song" : juce::String (" ") + juce::String::charToString ((juce::juce_wchar) ('A' + curSlot.load())));
    auto file = dir.getChildFile (name + ".mid");
    file.deleteFile();
    if (auto os = file.createOutputStream()) mf.writeTo (*os);
    return file;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> TaktwerkProcessor::toXml (bool withSession) const
{
    auto xml = std::make_unique<juce::XmlElement> ("TAKTWERK");
    if (auto p = const_cast<juce::AudioProcessorValueTreeState&> (apvts).copyState().createXml()) xml->addChildElement (p.release());
    auto* pat = xml->createNewChildElement ("PATTERNS");
    pat->setAttribute ("slot", curSlot.load());
    juce::String ln; for (auto& l : laneNote) ln << l.load() << " ";
    pat->setAttribute ("laneNotes", ln);
    for (int s = 0; s < numSlots; ++s) for (int l = 0; l < numLanes; ++l)
    {
        juce::String v; for (int i = 0; i < numSteps; ++i) v << juce::String::toHexString ((int) steps[s][l][i].load()) << " ";
        pat->setAttribute ("p" + juce::String (s) + "_" + juce::String (l), v);
    }
    if (withSession) session->toXml (*xml->createNewChildElement ("SESSION"));
    return xml;
}

void TaktwerkProcessor::fromXml (const juce::XmlElement& xml, bool withSession)
{
    if (! xml.hasTagName ("TAKTWERK")) return;
    session->trackType[track()] = 0;
    if (auto* p = xml.getChildByName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*p));
    if (auto* pat = xml.getChildByName ("PATTERNS"))
    {
        curSlot = juce::jlimit (0, numSlots - 1, pat->getIntAttribute ("slot", 0));
        auto ln = juce::StringArray::fromTokens (pat->getStringAttribute ("laneNotes"), " ", "");
        for (int l = 0; l < numLanes && l < ln.size(); ++l) laneNote[l] = juce::jlimit (0, 127, ln[l].getIntValue());
        for (int s = 0; s < numSlots; ++s) for (int l = 0; l < numLanes; ++l)
        {
            auto v = juce::StringArray::fromTokens (pat->getStringAttribute ("p" + juce::String (s) + "_" + juce::String (l)), " ", "");
            for (int i = 0; i < numSteps; ++i) steps[s][l][i] = i < v.size() ? (uint32_t) v[i].getHexValue32() : pack ({});
        }
    }
    if (withSession) if (auto* se = xml.getChildByName ("SESSION")) session->fromXml (*se);
    session->trackType[track()] = type() + 1;
    session->bump(); patternRev.fetch_add (1);
}

void TaktwerkProcessor::getStateInformation (juce::MemoryBlock& dest) { copyXmlToBinary (*toXml (true), dest); }
void TaktwerkProcessor::setStateInformation (const void* data, int size) { if (auto xml = getXmlFromBinary (data, size)) fromXml (*xml, true); }

juce::AudioProcessorEditor* TaktwerkProcessor::createEditor() { return new TaktwerkEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TaktwerkProcessor(); }
