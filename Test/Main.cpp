#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <set>

using namespace tw;

struct Head : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing); return p; }
};

struct Ev { juce::MidiMessage m; long sample; };
static int fails = 0;
static void check (bool ok, const juce::String& what) { std::cout << (ok ? "ok    " : "FAIL  ") << what << std::endl; if (! ok) ++fails; }

static std::vector<Ev> run (TaktwerkProcessor& p, Head& h, double bars, const std::vector<std::pair<double, juce::MidiMessage>>& input = {})
{
    const double sr = 44100.0; const int bs = 512;
    std::vector<Ev> out; juce::AudioBuffer<float> buf (2, bs);
    const double ppqPerSample = h.bpm / (60.0 * sr);
    const long total = (long) (bars * 4.0 / ppqPerSample);
    for (long pos = 0; pos < total; pos += bs)
    {
        juce::MidiBuffer mb;
        for (auto& in : input) { long s = (long) (in.first / ppqPerSample); if (s >= pos && s < pos + bs) mb.addEvent (in.second, (int) (s - pos)); }
        h.ppq = pos * ppqPerSample;
        p.processBlock (buf, mb);
        for (const auto meta : mb) out.push_back ({ meta.getMessage(), pos + meta.samplePosition });
    }
    h.playing = false; juce::MidiBuffer mb; p.processBlock (buf, mb);
    for (const auto meta : mb) out.push_back ({ meta.getMessage(), total });
    h.playing = true;
    return out;
}

static void prep (TaktwerkProcessor& p, Head& h) { p.setPlayHead (&h); p.setRateAndBufferSizeDetails (44100.0, 512); p.prepareToPlay (44100.0, 512); }
static void setParam (TaktwerkProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static int count (const std::vector<Ev>& e, bool on) { int n = 0; for (auto& x : e) if (on ? x.m.isNoteOn() : x.m.isNoteOff()) ++n; return n; }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File shots (argc > 1 ? argv[1] : "screenshots"); shots.createDirectory();
    Head h;

    TaktwerkProcessor drums; prep (drums, h);
    setParam (drums, "humanize", 0.0f);
    auto e = run (drums, h, 2.0);
    int kicks = 0; bool onGrid = true;
    for (auto& x : e) if (x.m.isNoteOn() && x.m.getNoteNumber() == 36 && x.m.getVelocity() > 100) { ++kicks; if (std::abs ((double) (x.sample % 22050)) > 2 && x.sample % 22050 < 22048) onGrid = false; }
    check (kicks >= 8, "drums: four-on-the-floor kick, " + juce::String (kicks) + " strong kicks in 2 bars");
    check (onGrid, "drums: kicks land on the beat");
    check (count (e, true) == count (e, false) && count (e, true) > 16, "drums: every note-on has a note-off (" + juce::String (count (e, true)) + ")");

    drums.session->swing = 1.0f;
    drums.clearSlot (0); StepData sd; sd.on = true; drums.setStep (0, 2, 1, sd);
    e = run (drums, h, 1.0);
    long firstOn = -1; for (auto& x : e) if (x.m.isNoteOn()) { firstOn = x.sample; break; }
    check (std::abs (firstOn - (long) (5512.5 + 1837.5)) <= 2, "swing: off-16th delayed to triplet position (sample " + juce::String (firstOn) + ")");
    drums.session->swing = 0.0f; drums.generate (0);

    TaktwerkProcessor bass; prep (bass, h);
    setParam (bass, "track", 1.0f); bass.setTypeFromUser (Bass);
    check (bass.session.operator->() == drums.session.operator->(), "session: shared between instances");
    bool allIn = true; int total = 0;
    for (int sc : { 3, 7, 9, 24, 35 })
    {
        bass.session->scale = sc; bass.session->root = 4; setParam (bass, "complexity", 1.0f); bass.generate (0);
        auto b = run (bass, h, 4.0);
        for (auto& x : b) if (x.m.isNoteOn()) { ++total; if (! inScale (sc, 4, x.m.getNoteNumber())) allIn = false; }
    }
    check (allIn && total > 40, "bass: all " + juce::String (total) + " notes stay in the chosen scale (5 scales, chord changes)");

    TaktwerkProcessor pad; prep (pad, h);
    setParam (pad, "track", 3.0f); pad.setTypeFromUser (Pad);
    pad.session->scale = 1; pad.session->root = 9;
    auto pe = run (pad, h, 4.0);
    std::set<int> firstChord; for (auto& x : pe) if (x.m.isNoteOn() && x.sample == 0) firstChord.insert (x.m.getNoteNumber() % 12);
    check (firstChord == std::set<int> { 9, 0, 4 }, "pad: bar 1 plays an A minor triad");
    std::set<int> bar3; for (auto& x : pe) if (x.m.isNoteOn() && x.sample >= 176400 - 2 && x.sample < 176400 + 600) bar3.insert (x.m.getNoteNumber() % 12);
    check (bar3 == std::set<int> { 5, 9, 0 }, "pad: bar 3 follows the progression to F major");
    check (count (pe, true) == count (pe, false), "pad: no hanging notes");

    drums.session->follow = 1; drums.session->cell[0][0] = 0;
    check (count (run (drums, h, 2.0), true) == 0, "arrangement: track is silent where its cell is empty");
    drums.session->cell[0][0] = 2; drums.clearSlot (1);
    check (count (run (drums, h, 1.0), true) == 0, "arrangement: cell B plays pattern B (empty here)");
    drums.session->cell[0][0] = 1; drums.session->follow = 0;

    TaktwerkProcessor synth; prep (synth, h);
    setParam (synth, "track", 2.0f); synth.setTypeFromUser (Synth);
    synth.clearSlot (0); synth.learn = 1;
    run (synth, h, 1.0, { { 1.0, juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100) }, { 1.5, juce::MidiMessage::noteOff (1, 69) },
                           { 2.0, juce::MidiMessage::noteOn (1, 73, (juce::uint8) 100) }, { 2.25, juce::MidiMessage::noteOff (1, 73) } });
    synth.learn = 0;
    auto s4 = synth.getStep (0, 0, 4), s8 = synth.getStep (0, 0, 8);
    check (s4.on && s4.degree == 7 && s4.len == 2, "learn: A5 recorded on step 5 as the root an octave up, 2 steps long");
    check (s8.on && s8.degree == 9, "learn: C#6 (not in A minor) snapped onto the scale");
    check (synth.session->hist[9].load() > 0, "scale guide: played notes are counted");

    juce::MemoryBlock mb; bass.getStateInformation (mb);
    TaktwerkProcessor copy; copy.setStateInformation (mb.getData(), (int) mb.getSize());
    bool same = copy.type() == Bass && copy.track() == 1;
    for (int i = 0; i < 16; ++i) { auto a = bass.getStep (0, 0, i), b = copy.getStep (0, 0, i); if (a.on != b.on || a.degree != b.degree || a.len != b.len) same = false; }
    check (same, "state: save and reload keeps type, track and pattern");

    auto f = bass.writeMidiFile (false);
    juce::MidiFile mf; juce::FileInputStream in (f);
    check (f.existsAsFile() && mf.readFrom (in) && mf.getNumTracks() == 1 && mf.getTrack (0)->getNumEvents() > 8, "midi export: file written with notes (" + f.getFileName() + ")");
    check (drums.writeMidiFile (true).getSize() > 200, "midi export: whole song");

    for (auto& h12 : synth.session->hist) h12 = 0;
    int i = 0;
    for (auto* p : { &drums, &bass, &synth, &pad })
    {
        if (i == 2) for (int n : { 9, 9, 9, 0, 4, 5, 7, 10 }) p->session->hist[n].fetch_add (2);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto file = shots.getChildFile (juce::String ("ui_") + typeNames[i++] + ".png"); file.deleteFile();
        juce::FileOutputStream os (file); juce::PNGImageFormat().writeImageToStream (img, os);
    }
    std::cout << (fails == 0 ? "ALL OK" : "FAILED: " + juce::String (fails)) << std::endl;
    return fails;
}
