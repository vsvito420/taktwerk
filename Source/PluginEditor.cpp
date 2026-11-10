#include "PluginEditor.h"

using namespace tw;
using juce::String;

namespace
{
const juce::Colour bg (0xff14161a), panel (0xff1d2026), cellCol (0xff272b33), cellAlt (0xff2f343e), line (0xff3a404b),
                   text (0xffe6e8ec), dim (0xff8a909c), accent (0xffff7a1a);
juce::Colour typeColour (int t)
{
    static const juce::Colour c[] = { juce::Colour (0xffff7a1a), juce::Colour (0xff3ddc97), juce::Colour (0xff4db8ff), juce::Colour (0xffc77dff) };
    return c[juce::jlimit (0, 3, t)];
}
juce::Font font (float size, bool bold = false) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }
String midiName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 5); }
const String star = String::fromUTF8 ("\xe2\x98\x85 ");
const char* const laneNames[6] = { "Kick", "Clap", "Closed Hat", "Open Hat", "Perc", "Ride" };
}

//==============================================================================
struct TaktwerkEditor::Grid : juce::Component, juce::SettableTooltipClient
{
    static constexpr int labelW = 96;
    TaktwerkProcessor& p;
    int dragStep = -1, dragRow = -1; bool paintOn = false;

    explicit Grid (TaktwerkProcessor& pr) : p (pr) { setOpaque (true); }
    bool drums() const { return p.type() == Drums; }
    int rows() const { return drums() ? 6 : (TaktwerkProcessor::degMax - TaktwerkProcessor::degMin + 1); }
    float rowH() const { return (float) getHeight() / (float) rows(); }
    float stepW() const { return (float) (getWidth() - labelW) / 16.0f; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (panel);
        const int slot = p.curSlot.load(), nRows = rows(), type = p.type();
        const float rh = rowH(), sw = stepW();
        const auto col = typeColour (type);
        const int scale = p.session->scale.load(), root = p.session->root.load(), n = scales[scale].n;
        const int playStep = p.uiStep.load();
        const bool slotPlaying = playStep >= 0 && p.slotForBar (p.uiBar.load()) == slot;

        for (int r = 0; r < nRows; ++r)
        {
            const float y = r * rh;
            const int degree = TaktwerkProcessor::degMax - r;
            const int rel = ((degree % n) + n) % n;
            const bool isRoot = ! drums() && rel == 0, chordTone = ! drums() && (rel == 2 || rel == 4);
            for (int s = 0; s < 16; ++s)
            {
                auto c = ((s / 4) % 2 == 0) ? cellAlt : cellCol;
                if (isRoot) c = c.brighter (0.18f); else if (chordTone) c = c.brighter (0.07f);
                if (slotPlaying && s == playStep) c = c.brighter (0.25f);
                g.setColour (c);
                g.fillRect (labelW + s * sw + 0.5f, y + 0.5f, sw - 1.0f, rh - 1.0f);
            }
            g.setFont (font (drums() ? 12.0f : 10.5f, isRoot));
            if (drums())
            {
                g.setColour (text); g.drawText (laneNames[r], 6, (int) y, labelW - 40, (int) rh, juce::Justification::centredLeft);
                g.setColour (dim);  g.drawText (midiName (p.laneNote[r].load()), labelW - 38, (int) y, 34, (int) rh, juce::Justification::centredRight);
            }
            else
            {
                g.setColour (isRoot ? col : chordTone ? text : dim);
                String nm = noteNames[((root + degreeToSemis (scale, degree)) % 12 + 12) % 12];
                g.drawText (nm + (isRoot ? "  root" : chordTone ? String::fromUTF8 ("  \xe2\x80\xa2 chord") : ""), 6, (int) y, labelW - 8, (int) rh, juce::Justification::centredLeft);
            }
        }

        for (int lane = 0; lane < (drums() ? 6 : 1); ++lane)
            for (int s = 0; s < 16; ++s)
            {
                auto sd = p.getStep (slot, lane, s);
                if (! sd.on) continue;
                int row = lane, len = 1;
                if (! drums())
                {
                    row = TaktwerkProcessor::degMax - sd.degree;
                    len = juce::jmin (sd.len, 16 - s);
                    for (int i = 1; i < len; ++i) if (p.getStep (slot, 0, s + i).on) { len = i; break; }
                }
                g.setColour (col.withAlpha (0.45f + 0.55f * (float) sd.vel / 127.0f));
                g.fillRoundedRectangle (labelW + s * sw + 2.0f, row * rh + 2.0f, len * sw - 4.0f, rh - 4.0f, 3.0f);
            }
    }

    void apply (const juce::MouseEvent& e, bool isDown)
    {
        const int slot = p.curSlot.load();
        const int step = juce::jlimit (0, 15, (int) ((e.position.x - labelW) / stepW()));
        const int row = juce::jlimit (0, rows() - 1, (int) (e.position.y / rowH()));
        if (drums())
        {
            auto sd = p.getStep (slot, row, step);
            if (isDown) paintOn = ! sd.on;
            if (sd.on != paintOn) { sd.on = paintOn; sd.vel = 100; sd.len = 1; p.setStep (slot, row, step, sd); }
        }
        else if (isDown)
        {
            const int degree = TaktwerkProcessor::degMax - row;
            auto sd = p.getStep (slot, 0, step);
            dragStep = -1;
            if (sd.on && sd.degree == degree) sd.on = false;
            else { sd.on = true; sd.degree = degree; sd.len = 1; sd.vel = 100; dragStep = step; }
            p.setStep (slot, 0, step, sd);
        }
        else if (dragStep >= 0)
        {
            auto sd = p.getStep (slot, 0, dragStep);
            const int len = juce::jmax (1, step - dragStep + 1);
            if (sd.len != len) { sd.len = len; p.setStep (slot, 0, dragStep, sd); }
        }
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < labelW)
        {
            if (! drums()) return;
            const int lane = juce::jlimit (0, 5, (int) (e.position.y / rowH()));
            juce::PopupMenu m;
            m.addSectionHeader (String ("MIDI note for ") + laneNames[lane]);
            for (int n = 24; n <= 84; ++n) m.addItem (n + 1, midiName (n) + "  (" + String (n) + ")", true, n == p.laneNote[lane].load());
            juce::Component::SafePointer<Grid> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMaximumNumColumns (4), [safe, lane] (int r)
            { if (safe != nullptr && r > 0) { safe->p.laneNote[lane] = r - 1; safe->repaint(); } });
            return;
        }
        apply (e, true);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (e.getMouseDownX() >= labelW) apply (e, false); }
};

//==============================================================================
struct TaktwerkEditor::Prog : juce::Component, juce::SettableTooltipClient
{
    TaktwerkProcessor& p;
    explicit Prog (TaktwerkProcessor& pr) : p (pr) {}
    juce::Rectangle<float> box (int i) const { float w = (float) getWidth() / 8.0f; return { i * w + 2.0f, 0.0f, w - 4.0f, (float) getHeight() }; }

    void paint (juce::Graphics& g) override
    {
        auto& s = *p.session;
        const int scale = s.scale.load(), root = s.root.load(), len = s.progLen.load();
        const int playing = p.uiStep.load() >= 0 ? p.uiBar.load() % juce::jmax (1, len) : -1;
        for (int i = 0; i < 8; ++i)
        {
            auto b = box (i); const bool active = i < len; const int d = s.prog[i].load();
            g.setColour (active ? (i == playing ? cellAlt.brighter (0.3f) : cellAlt) : cellCol.withAlpha (0.5f));
            g.fillRoundedRectangle (b, 5.0f);
            if (i == playing) { g.setColour (accent); g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.5f); }
            g.setColour (dim.withAlpha (active ? 1.0f : 0.4f)); g.setFont (font (10.0f));
            g.drawText ("BAR " + String (i + 1), b.withHeight (16.0f).translated (0.0f, 3.0f), juce::Justification::centred);
            g.setColour (text.withAlpha (active ? 1.0f : 0.35f)); g.setFont (font (17.0f, true));
            g.drawText (chordName (scale, root, d), b.withTrimmedTop (18.0f).withTrimmedBottom (18.0f), juce::Justification::centred);
            g.setColour (accent.withAlpha (active ? 1.0f : 0.35f)); g.setFont (font (11.0f));
            g.drawText (romanName (scale, d), b.withTop (b.getBottom() - 19.0f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, 7, (int) (e.position.x / ((float) getWidth() / 8.0f)));
        auto& s = *p.session; const int scale = s.scale.load(), root = s.root.load();
        juce::PopupMenu m; m.addSectionHeader ("Chord for bar " + String (i + 1));
        for (int d = 0; d < scales[scale].n; ++d)
            m.addItem (d + 1, romanName (scale, d) + "   " + chordName (scale, root, d), true, d == s.prog[i].load());
        juce::Component::SafePointer<Prog> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, i] (int r)
        { if (safe != nullptr && r > 0) { safe->p.session->prog[i] = r - 1; safe->p.session->bump(); } });
    }
};

//==============================================================================
struct TaktwerkEditor::Guide : juce::Component, juce::SettableTooltipClient
{
    struct Sug { int root, scale; float fit, score; bool rec; };
    TaktwerkProcessor& p;
    std::vector<Sug> sugs; bool hasInput = false;
    static constexpr int rowsShown = 6, headH = 30, rowH = 16, stripH = 24;
    explicit Guide (TaktwerkProcessor& pr) : p (pr) {}

    void compute()
    {
        auto& s = *p.session; sugs.clear();
        int hist[12], total = 0; for (int i = 0; i < 12; ++i) { hist[i] = s.hist[i].load(); total += hist[i]; }
        const unsigned styleBit = 1u << s.style.load(); const int root = s.root.load();
        hasInput = total > 0;
        if (! hasInput)
        {
            for (int i = 0; i < numScales; ++i) if (scales[i].styles & styleBit) sugs.push_back ({ root, i, 1.0f, 0.0f, true });
            return;
        }
        for (int sc = 0; sc < numScales - 1; ++sc)
            for (int r = 0; r < 12; ++r)
            {
                int in = 0; for (int k = 0; k < scales[sc].n; ++k) in += hist[(r + scales[sc].iv[k]) % 12];
                const float fit = (float) in / (float) total; const bool rec = (scales[sc].styles & styleBit) != 0;
                float score = fit * 100.0f - (float) juce::jmax (0, scales[sc].n - 7) * 4.0f + (rec ? 5.0f : 0.0f)
                            + (r == root ? 3.0f : 0.0f) + 12.0f * (float) hist[r] / (float) total + (scales[sc].n == 7 ? 2.0f : 0.0f);
                sugs.push_back ({ r, sc, fit, score, rec });
            }
        std::sort (sugs.begin(), sugs.end(), [] (const Sug& a, const Sug& b) { return a.score > b.score; });
        if (sugs.size() > rowsShown) sugs.resize (rowsShown);
    }

    void paint (juce::Graphics& g) override
    {
        compute();
        auto& s = *p.session; const int root = s.root.load(), scale = s.scale.load();
        g.setColour (dim); g.setFont (font (10.5f));
        g.drawFittedText (hasInput ? "Best matches for the notes you played into Taktwerk. Click one to use it."
                                   : String ("Good scales for ") + styleNames[s.style.load()] + ". Click one to use it - or play your melody into a Bass/Synth/Pad track to get matches.",
                          0, 0, getWidth(), headH - 2, juce::Justification::topLeft, 2);
        for (int i = 0; i < (int) sugs.size() && i < rowsShown; ++i)
        {
            const auto& u = sugs[(size_t) i]; const int y = headH + i * rowH;
            const bool current = u.root == root && u.scale == scale;
            if (current) { g.setColour (accent.withAlpha (0.18f)); g.fillRoundedRectangle (0.0f, (float) y, (float) getWidth(), (float) rowH - 1.0f, 3.0f); }
            g.setColour (current ? accent : text); g.setFont (font (12.0f, current));
            g.drawText ((u.rec ? star : String ("   ")) + noteNames[u.root] + " " + scales[u.scale].name, 4, y, 178, rowH, juce::Justification::centredLeft);
            if (hasInput)
            {
                g.setColour (cellAlt); g.fillRect (186, y + 5, 90, 6);
                g.setColour (u.fit > 0.99f ? juce::Colour (0xff3ddc97) : accent); g.fillRect (186, y + 5, (int) (90.0f * u.fit), 6);
                g.setColour (dim); g.setFont (font (11.0f)); g.drawText (String (juce::roundToInt (u.fit * 100.0f)) + "%", 280, y, 40, rowH, juce::Justification::centredLeft);
            }
            else { g.setColour (dim); g.setFont (font (10.5f)); g.drawText (scales[u.scale].mood, 186, y, getWidth() - 186, rowH, juce::Justification::centredLeft); }
        }
        const float kw = (float) getWidth() / 12.0f, ky = (float) (getHeight() - stripH);
        for (int i = 0; i < 12; ++i)
        {
            const bool in = inScale (scale, root, i), isRoot = i == root;
            g.setColour (isRoot ? accent : in ? cellAlt.brighter (0.35f) : cellCol.darker (0.3f));
            g.fillRoundedRectangle (i * kw + 1.0f, ky, kw - 2.0f, (float) stripH, 3.0f);
            g.setColour (isRoot ? juce::Colours::black : in ? text : dim.withAlpha (0.5f)); g.setFont (font (10.5f, in));
            g.drawText (noteNames[i], (int) (i * kw), (int) ky, (int) kw, stripH, juce::Justification::centred);
            if (s.hist[i].load() > 0) { g.setColour (in ? juce::Colour (0xff3ddc97) : juce::Colour (0xffff4d4d)); g.fillEllipse (i * kw + kw - 8.0f, ky + 2.0f, 5.0f, 5.0f); }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = (e.y - headH) / rowH;
        if (e.y < headH || i < 0 || i >= (int) sugs.size()) return;
        p.session->root = sugs[(size_t) i].root; p.session->scale = sugs[(size_t) i].scale; p.session->bump();
    }
};

//==============================================================================
struct TaktwerkEditor::Arrange : juce::Component, juce::SettableTooltipClient
{
    static constexpr int labelW = 96, headH = 30;
    TaktwerkProcessor& p;
    explicit Arrange (TaktwerkProcessor& pr) : p (pr) {}
    float colW() const { return (float) (getWidth() - labelW) / 8.0f; }
    float rowH() const { return (float) (getHeight() - headH) / 8.0f; }

    void paint (juce::Graphics& g) override
    {
        auto& s = *p.session;
        const float cw = colW(), rh = rowH();
        const int playSec = (s.follow.load() != 0 && p.uiStep.load() >= 0) ? s.sectionForBar (p.uiBar.load()) : -1;
        int bar = 1;
        for (int c = 0; c < 8; ++c)
        {
            const int bars = s.secBars[c].load(); const bool used = bars > 0;
            juce::Rectangle<float> h (labelW + c * cw + 1.0f, 0.0f, cw - 2.0f, (float) headH - 2.0f);
            g.setColour (used ? (c == playSec ? accent.withAlpha (0.5f) : cellAlt) : cellCol.withAlpha (0.5f)); g.fillRoundedRectangle (h, 4.0f);
            g.setColour (used ? text : dim); g.setFont (font (12.0f, true));
            g.drawText (used ? String (sectionNames[s.secName[c].load()]) : String ("+ add"), h.withHeight (15.0f).translated (0.0f, 1.0f), juce::Justification::centred);
            if (used)
            {
                g.setColour (dim); g.setFont (font (10.0f));
                g.drawText (String (bars) + " bars  |  bar " + String (bar), h.withTop (14.0f), juce::Justification::centred);
                bar += bars;
            }
        }
        for (int t = 0; t < 8; ++t)
        {
            const float y = headH + t * rh; const int type = s.trackType[t].load() - 1; const bool mine = t == p.track();
            g.setColour (type >= 0 ? (mine ? typeColour (type) : text) : dim.withAlpha (0.5f)); g.setFont (font (11.5f, mine));
            g.drawText (String (t + 1) + "  " + (type >= 0 ? typeNames[type] : "-") + (mine ? "  (this)" : ""), 4, (int) y, labelW - 6, (int) rh, juce::Justification::centredLeft);
            for (int c = 0; c < 8; ++c)
            {
                if (s.secBars[c].load() <= 0) continue;
                const int v = s.cell[t][c].load();
                juce::Rectangle<float> r (labelW + c * cw + 1.0f, y + 1.0f, cw - 2.0f, rh - 2.0f);
                g.setColour (v > 0 && type >= 0 ? typeColour (type).withAlpha (0.35f + 0.13f * (float) v) : cellCol); g.fillRoundedRectangle (r, 3.0f);
                if (v > 0 && type >= 0) { g.setColour (juce::Colours::white); g.setFont (font (11.0f, true)); g.drawText (String::charToString ((juce::juce_wchar) ('A' + v - 1)), r, juce::Justification::centred); }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < labelW) return;
        auto& s = *p.session;
        const int c = juce::jlimit (0, 7, (int) ((e.position.x - labelW) / colW()));
        if (e.y >= headH)
        {
            if (s.secBars[c].load() <= 0) return;
            const int t = juce::jlimit (0, 7, (int) ((e.position.y - headH) / rowH()));
            s.cell[t][c] = (s.cell[t][c].load() + (e.mods.isRightButtonDown() ? 4 : 1)) % 5; s.bump();
            return;
        }
        juce::PopupMenu m, names, lens;
        for (int i = 0; i < numSectionNames; ++i) names.addItem (1 + i, sectionNames[i], true, i == s.secName[c].load());
        for (int b : { 4, 8, 16, 32 }) lens.addItem (100 + b, String (b) + " bars", true, b == s.secBars[c].load());
        m.addSubMenu ("Name", names); m.addSubMenu ("Length", lens); m.addSeparator(); m.addItem (100, "Remove this part");
        juce::Component::SafePointer<Arrange> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, c] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& ss = *safe->p.session;
            if (r >= 100) ss.secBars[c] = r - 100; else { ss.secName[c] = r - 1; if (ss.secBars[c].load() <= 0) ss.secBars[c] = 8; }
            ss.bump();
        });
    }
};

//==============================================================================
struct TaktwerkEditor::DragTile : juce::Component, juce::SettableTooltipClient
{
    TaktwerkProcessor& p; bool song, dragging = false;
    DragTile (TaktwerkProcessor& pr, bool wholeSong) : p (pr), song (wholeSong) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (cellAlt); g.fillRoundedRectangle (b, 4.0f);
        g.setColour (typeColour (p.type())); g.drawRoundedRectangle (b, 4.0f, 1.0f);
        g.setColour (text); g.setFont (font (11.0f, true));
        g.drawText (song ? "DRAG SONG MIDI" : "DRAG PATTERN MIDI", getLocalBounds(), juce::Justification::centred);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 5) return;
        dragging = true;
        auto f = p.writeMidiFile (song);
        juce::Component::SafePointer<DragTile> safe (this);
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [safe] { if (safe != nullptr) safe->dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
};

//==============================================================================
struct TaktwerkEditor::Help : juce::Component
{
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.82f));
        auto b = getLocalBounds().reduced (70, 40);
        g.setColour (panel); g.fillRoundedRectangle (b.toFloat(), 8.0f);
        g.setColour (accent); g.drawRoundedRectangle (b.toFloat(), 8.0f, 1.0f);
        b.reduce (24, 18);
        g.setFont (font (20.0f, true)); g.drawText ("QUICK START", b.removeFromTop (30), juce::Justification::centredLeft);
        g.setColour (text); g.setFont (font (13.5f));
        g.drawFittedText (
            "Taktwerk makes no sound. It plays notes into your other instruments, in time with FL Studio.\n\n"
            "1.  Add one Taktwerk for each part of your track (drums, bass, synth, pad). Give each its own TRACK number and pick its TYPE.\n\n"
            "2.  Connect it to an instrument: click the gear icon at the top left of the Taktwerk window, open the plug settings tab and set "
            "MIDI 'Output port' to a number (for example 1). Open the same settings on your instrument (Serum, Sylenth, a drum plugin ...) "
            "and set its MIDI 'Input port' to the same number. Use a different port for every Taktwerk.\n\n"
            "3.  Press Play in FL Studio. Taktwerk only plays while FL is playing.\n\n"
            "4.  SESSION (key, scale, style, chords, swing) and ARRANGEMENT are shared: change them in any Taktwerk and all others follow, "
            "so everything stays in the same key. The SCALE GUIDE shows which scales fit your style (star) or the notes you played.\n\n"
            "5.  GENERATE makes a new pattern for the chosen style. VARY copies it to the next slot with small changes. "
            "Click in the grid to edit. Each track has four patterns: A, B, C, D.\n\n"
            "6.  LEARN: switch it on, press Play and send notes to Taktwerk (from its piano roll or your keyboard). "
            "They are written into the pattern and moved onto the scale.\n\n"
            "7.  ARRANGEMENT: click a cell to choose which pattern (A-D) a track plays in each part of the song, or nothing. "
            "Switch FOLLOW SONG on and play FL in Song mode.\n\n"
            "8.  DRAG PATTERN MIDI / DRAG SONG MIDI: drag the box into FL's piano roll or playlist to get the notes as normal MIDI.\n\n"
            "Patcher: put Taktwerk and your instruments in one Patcher, set Taktwerk's MIDI Output port, then right-click Taktwerk > "
            "Outputs > Events and switch the MIDI port on. Drag a cable from it to the instrument.",
            b.getX(), b.getY(), b.getWidth(), b.getHeight() - 24, juce::Justification::topLeft, 60, 0.9f);
        g.setColour (dim); g.setFont (font (12.0f));
        g.drawText ("Click anywhere to close", b.removeFromBottom (20), juce::Justification::centredRight);
    }
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};

//==============================================================================
void TaktwerkEditor::label (const String& t, juce::Component& target)
{
    auto* l = labels.add (new juce::Label ({}, t));
    l->setFont (font (10.0f, true)); l->setColour (juce::Label::textColourId, dim);
    l->setBorderSize ({}); l->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (l); labelTargets.push_back ({ l, &target });
}

void TaktwerkEditor::setupBar (juce::Slider& s, const String& name, const String& tip, const char* paramId)
{
    s.setSliderStyle (juce::Slider::LinearBar); s.setTooltip (tip);
    addAndMakeVisible (s); label (name, s);
    if (paramId != nullptr) sliderAtts.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.apvts, paramId, s));
}

TaktwerkEditor::TaktwerkEditor (TaktwerkProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    laf.setColour (juce::ComboBox::backgroundColourId, cellCol); laf.setColour (juce::ComboBox::outlineColourId, line);
    laf.setColour (juce::ComboBox::textColourId, text); laf.setColour (juce::PopupMenu::backgroundColourId, panel);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, accent); laf.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    laf.setColour (juce::TextButton::buttonColourId, cellAlt); laf.setColour (juce::TextButton::textColourOffId, text);
    laf.setColour (juce::Slider::trackColourId, accent.withAlpha (0.55f)); laf.setColour (juce::Slider::backgroundColourId, cellCol);
    laf.setColour (juce::Slider::textBoxTextColourId, text); laf.setColour (juce::Slider::textBoxOutlineColourId, line);
    setLookAndFeel (&laf);

    auto& s = *proc.session;
    auto combo = [this] (juce::ComboBox& b, const String& name, const String& tip) { b.setTooltip (tip); addAndMakeVisible (b); label (name, b); };

    combo (trackBox, "TRACK", "Which row of the arrangement this Taktwerk is. Give every Taktwerk its own number.");
    for (int i = 0; i < 8; ++i) trackBox.addItem (String (i + 1), i + 1);
    trackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "track", trackBox);

    combo (typeBox, "TYPE", "What this Taktwerk plays: drums, a bass line, a synth melody or pad chords. Changing it makes new patterns.");
    for (int i = 0; i < numTypes; ++i) typeBox.addItem (typeNames[i], i + 1);
    typeBox.onChange = [this] { int t = typeBox.getSelectedId() - 1; if (t >= 0 && t != proc.type()) proc.setTypeFromUser (t); };

    combo (keyBox, "KEY", "The root note of your track. Shared by all Taktwerk tracks.");
    for (int i = 0; i < 12; ++i) keyBox.addItem (noteNames[i], i + 1);
    keyBox.onChange = [this] { proc.session->root = keyBox.getSelectedId() - 1; proc.session->bump(); };

    combo (scaleBox, "SCALE", "The set of notes everything is locked to. A star marks scales that work well for the chosen style.");
    scaleBox.onChange = [this] { if (scaleBox.getSelectedId() > 0) { proc.session->scale = scaleBox.getSelectedId() - 1; proc.session->bump(); } };

    combo (styleBox, "STYLE", "The genre GENERATE aims for. It also decides which scales get a star.");
    for (int i = 0; i < numStyles; ++i) styleBox.addItem (styleNames[i], i + 1);
    styleBox.onChange = [this] { proc.session->style = styleBox.getSelectedId() - 1; proc.session->bump(); };

    setupBar (swing, "SWING", "Delays every second 16th note for a shuffled groove. 0 = straight. Shared by all tracks.", nullptr);
    swing.setRange (0.0, 1.0, 0.01);
    swing.textFromValueFunction = [] (double v) { return String (juce::roundToInt (v * 100.0)) + " %"; };
    swing.updateText();
    swing.onValueChange = [this] { proc.session->swing = (float) swing.getValue(); };

    combo (progPresetBox, "CHORD PRESET", "Ready-made chord orders. The numbers are steps of the scale (1 = the key).");
    for (int i = 0; i < numProgPresets; ++i) progPresetBox.addItem (progPresets[i].name, i + 1);
    progPresetBox.setTextWhenNothingSelected ("Choose...");
    progPresetBox.onChange = [this]
    {
        int i = progPresetBox.getSelectedId() - 1; if (i < 0) return;
        for (int b = 0; b < 8; ++b) proc.session->prog[b] = progPresets[i].deg[b];
        proc.session->progLen = progPresets[i].len; proc.session->bump();
    };
    combo (progLenBox, "CHORD LOOP LENGTH", "After how many bars the chords start again.");
    for (int b : { 1, 2, 4, 8 }) progLenBox.addItem (String (b) + (b == 1 ? " bar" : " bars"), b);
    progLenBox.onChange = [this] { proc.session->progLen = progLenBox.getSelectedId(); proc.session->bump(); };

    prog = std::make_unique<Prog> (proc); addAndMakeVisible (*prog);
    prog->setTooltip ("The chord for each bar. Bass, synth and pad follow it automatically. Click a bar to change its chord.");
    label ("CHORDS PER BAR", *prog);

    guide = std::make_unique<Guide> (proc); addAndMakeVisible (*guide);
    guide->setTooltip ("Scale suggestions. Bottom row: notes of the current scale. Green dot = a note you played that fits, red dot = one that does not.");
    clearInputBtn.setTooltip ("Forget the notes you played, so the guide shows style suggestions again.");
    clearInputBtn.onClick = [this] { for (auto& h : proc.session->hist) h = 0; proc.session->bump(); };
    addAndMakeVisible (clearInputBtn);

    for (int i = 0; i < 4; ++i)
    {
        slotBtn[i].setButtonText (String::charToString ((juce::juce_wchar) ('A' + i)));
        slotBtn[i].setTooltip ("Pattern " + slotBtn[i].getButtonText() + ". Each track has four patterns to use in different parts of the song.");
        slotBtn[i].onClick = [this, i] { proc.curSlot = i; };
        addAndMakeVisible (slotBtn[i]);
    }
    genBtn.setTooltip ("Make a new pattern in the chosen style. Click again for another idea.");
    genBtn.onClick = [this] { proc.generate (proc.curSlot.load()); };
    varyBtn.setTooltip ("Copy this pattern to the next slot and change it a little. Good for fills and build-ups.");
    varyBtn.onClick = [this] { int from = proc.curSlot.load(), to = (from + 1) % 4; proc.vary (from, to); proc.curSlot = to; };
    clearBtn.setTooltip ("Empty this pattern.");
    clearBtn.onClick = [this] { proc.clearSlot (proc.curSlot.load()); };
    learnBtn.setTooltip ("Record notes you send to Taktwerk into this pattern while FL plays. Starts with an empty pattern. Click again to stop.");
    learnBtn.onClick = [this] { bool on = proc.learn.load() == 0; if (on) proc.clearSlot (proc.curSlot.load()); proc.learn = on ? 1 : 0; };
    for (auto* b : { &genBtn, &varyBtn, &clearBtn, &learnBtn }) addAndMakeVisible (*b);

    dragPattern = std::make_unique<DragTile> (proc, false); dragSong = std::make_unique<DragTile> (proc, true);
    dragPattern->setTooltip ("Drag this into FL's piano roll or playlist to get the current pattern as MIDI notes.");
    dragSong->setTooltip ("Drag this into FL to get this track's whole arrangement as MIDI notes.");
    addAndMakeVisible (*dragPattern); addAndMakeVisible (*dragSong);

    grid = std::make_unique<Grid> (proc); addAndMakeVisible (*grid);
    grid->setTooltip ("Click to add or remove notes. Drums: click a name on the left to change its MIDI note. "
                      "Bass/Synth/Pad: rows are scale notes that move with the chord of each bar; drag right to make a note longer.");

    setupBar (density, "DENSITY", "How many notes GENERATE makes. Low = sparse, high = busy.", "density");
    setupBar (complexity, "COMPLEXITY", "How many surprises GENERATE and VARY add: extra hits and notes away from the chord.", "complexity");
    setupBar (humanize, "HUMANIZE", "Random loudness changes so it sounds less like a machine.", "humanize");
    setupBar (gate, "NOTE LENGTH", "How long each note is held. Low = short and tight, high = long and smooth. Not used for drums.", "gate");
    setupBar (octave, "OCTAVE", "Moves bass, synth or pad notes up or down by octaves.", "octave");
    setupBar (channel, "MIDI CHANNEL", "The MIDI channel of the notes Taktwerk sends. Leave on 1 unless your instrument needs another.", "channel");
    combo (chordBox, "NOTES PER STEP", "Play one note or a whole chord for every step. Pads usually use Triad.");
    chordBox.addItemList ({ "Single note", "Power (1+5)", "Triad", "7th chord" }, 1);
    chordAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "chord", chordBox);

    arrange = std::make_unique<Arrange> (proc); addAndMakeVisible (*arrange);
    arrange->setTooltip ("Your song plan. Columns are song parts, rows are Taktwerk tracks. Click a cell to pick pattern A-D or nothing. Click a part's name to rename, resize or remove it.");
    followBtn.setTooltip ("ON: every track plays what the arrangement says for the current song position (use FL's Song mode). OFF: each track loops the pattern you have selected.");
    followBtn.onClick = [this] { proc.session->follow = proc.session->follow.load() == 0 ? 1 : 0; proc.session->bump(); };
    addAndMakeVisible (followBtn);

    saveBtn.setTooltip ("Save this track's patterns and settings as a preset file.");
    loadBtn.setTooltip ("Load a preset file into this track.");
    helpBtn.setTooltip ("Quick start: how to connect Taktwerk to your instruments.");
    auto presetDir = [] { auto d = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets/Taktwerk"); d.createDirectory(); return d; };
    saveBtn.onClick = [this, presetDir]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save Taktwerk preset", presetDir(), "*.taktwerk");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) f.withFileExtension ("taktwerk").replaceWithText (proc.toXml (false)->toString()); });
    };
    loadBtn.onClick = [this, presetDir]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load Taktwerk preset", presetDir(), "*.taktwerk");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc) { if (auto xml = juce::parseXML (fc.getResult())) proc.fromXml (*xml, false); });
    };
    help = std::make_unique<Help>();
    helpBtn.onClick = [this] { help->setVisible (true); help->toFront (false); };
    for (auto* b : { &saveBtn, &loadBtn, &helpBtn }) addAndMakeVisible (*b);
    addChildComponent (*help);

    juce::ignoreUnused (s);
    setSize (1000, 724);
    syncFromSession();
    timerCallback();
    startTimerHz (30);
}

TaktwerkEditor::~TaktwerkEditor() { stopTimer(); setLookAndFeel (nullptr); }

void TaktwerkEditor::rebuildScaleBox()
{
    const unsigned bit = 1u << proc.session->style.load();
    scaleBox.clear (juce::dontSendNotification);
    for (int i = 0; i < numScales; ++i) scaleBox.addItem (((scales[i].styles & bit) ? star : String()) + scales[i].name, i + 1);
}

void TaktwerkEditor::syncFromSession()
{
    auto& s = *proc.session;
    if (s.style.load() != lastStyle) { lastStyle = s.style.load(); rebuildScaleBox(); }
    keyBox.setSelectedId (s.root.load() + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (s.scale.load() + 1, juce::dontSendNotification);
    styleBox.setSelectedId (s.style.load() + 1, juce::dontSendNotification);
    progLenBox.setSelectedId (s.progLen.load(), juce::dontSendNotification);
    progPresetBox.setSelectedId (0, juce::dontSendNotification);
    for (int i = 0; i < numProgPresets; ++i)
    {
        bool same = progPresets[i].len == s.progLen.load();
        for (int b = 0; b < progPresets[i].len && same; ++b) same = progPresets[i].deg[b] == s.prog[b].load();
        if (same) { progPresetBox.setSelectedId (i + 1, juce::dontSendNotification); break; }
    }
    if (! swing.isMouseButtonDown()) swing.setValue (s.swing.load(), juce::dontSendNotification);
    followBtn.setButtonText (s.follow.load() != 0 ? "FOLLOW SONG: ON" : "FOLLOW SONG: OFF");
    followBtn.setColour (juce::TextButton::buttonColourId, s.follow.load() != 0 ? accent.darker (0.2f) : cellAlt);
}

void TaktwerkEditor::timerCallback()
{
    auto& s = *proc.session;
    const int type = proc.type(), track = proc.track(), slot = proc.curSlot.load();
    if (track != lastTrack) { if (lastTrack >= 0) s.trackType[lastTrack] = 0; lastTrack = track; s.bump(); }
    if (s.trackType[track].load() != type + 1) { s.trackType[track] = type + 1; s.bump(); }

    const int sig = s.revision.load() * 31 + proc.patternRev.load() * 17 + proc.uiStep.load() * 7 + proc.uiBar.load() * 131
                  + type * 3 + track * 5 + slot * 11 + proc.learn.load() * 13;
    if (sig == lastSig) return;
    lastSig = sig;
    if (s.revision.load() != lastSessionRev) { lastSessionRev = s.revision.load(); syncFromSession(); }

    typeBox.setSelectedId (type + 1, juce::dontSendNotification);
    const auto col = typeColour (type);
    for (int i = 0; i < 4; ++i)
    {
        slotBtn[i].setColour (juce::TextButton::buttonColourId, i == slot ? col : cellAlt);
        slotBtn[i].setColour (juce::TextButton::textColourOffId, i == slot ? juce::Colours::black : text);
    }
    varyBtn.setButtonText ("VARY > " + String::charToString ((juce::juce_wchar) ('A' + (slot + 1) % 4)));
    learnBtn.setButtonText (proc.learn.load() != 0 ? "LEARNING..." : "LEARN");
    learnBtn.setColour (juce::TextButton::buttonColourId, proc.learn.load() != 0 ? juce::Colour (0xffd93636) : cellAlt);
    const bool melodic = type != Drums;
    gate.setEnabled (melodic); octave.setEnabled (melodic); chordBox.setEnabled (melodic);
    repaint();
}

void TaktwerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    const auto col = typeColour (proc.type());
    g.setColour (col); g.setFont (font (24.0f, true)); g.drawText ("TAKTWERK", 14, 8, 150, 30, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f)); g.drawText ("v" TAKTWERK_VERSION "   groove & track planner", 150, 16, 220, 18, juce::Justification::centredLeft);

    auto box = [&g] (juce::Rectangle<int> r, const String& title, const String& sub)
    {
        g.setColour (panel); g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (text); g.setFont (font (12.0f, true)); g.drawText (title, r.getX() + 12, r.getY() + 6, 200, 16, juce::Justification::centredLeft);
        g.setColour (dim); g.setFont (font (11.0f)); g.drawText (sub, r.getX() + 12 + juce::GlyphArrangement::getStringWidthInt (font (12.0f, true), title) + 10, r.getY() + 6, 360, 16, juce::Justification::centredLeft);
    };
    box ({ 10, 50, 630, 186 }, "SESSION", "shared by all Taktwerk tracks");
    box ({ 650, 50, 340, 186 }, "SCALE GUIDE", "");
    box ({ 10, 244, 980, 276 }, "", "");
    box ({ 10, 528, 980, 186 }, "ARRANGEMENT", "shared - which pattern each track plays in each part of the song");
}

void TaktwerkEditor::resized()
{
    trackBox.setBounds (560, 17, 60, 22); typeBox.setBounds (630, 17, 100, 22);
    saveBtn.setBounds (800, 17, 60, 22); loadBtn.setBounds (866, 17, 60, 22); helpBtn.setBounds (958, 17, 30, 22);

    keyBox.setBounds (22, 92, 70, 24); scaleBox.setBounds (100, 92, 210, 24); styleBox.setBounds (318, 92, 120, 24); swing.setBounds (446, 92, 182, 24);
    prog->setBounds (22, 142, 414, 84); progPresetBox.setBounds (446, 142, 182, 24); progLenBox.setBounds (446, 190, 182, 24);

    clearInputBtn.setBounds (880, 55, 100, 18);
    guide->setBounds (662, 78, 316, 152);

    for (int i = 0; i < 4; ++i) slotBtn[i].setBounds (22 + i * 36, 254, 32, 24);
    genBtn.setBounds (176, 254, 92, 24); varyBtn.setBounds (274, 254, 86, 24); clearBtn.setBounds (366, 254, 64, 24); learnBtn.setBounds (436, 254, 92, 24);
    dragPattern->setBounds (540, 254, 132, 24); dragSong->setBounds (678, 254, 116, 24);
    grid->setBounds (22, 286, 772, 226);

    int y = 271;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &density, &complexity, &humanize, &gate, &octave, &chordBox, &channel })
    { c->setBounds (808, y, 170, 20); y += 35; }

    followBtn.setBounds (838, 533, 140, 20);
    arrange->setBounds (22, 558, 956, 150);
    help->setBounds (getLocalBounds());

    for (auto& lt : labelTargets) lt.first->setBounds (lt.second->getX(), lt.second->getY() - 13, juce::jmax (140, lt.second->getWidth()), 12);
    // TRACK and TYPE sit next to each other, so their labels must not be wider than the boxes
    for (auto& lt : labelTargets) if (lt.second == &trackBox) lt.first->setSize (60, 12);
}
