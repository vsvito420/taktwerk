#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class TaktwerkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TaktwerkEditor (TaktwerkProcessor&);
    ~TaktwerkEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    struct Grid; struct Prog; struct Guide; struct Arrange; struct DragTile; struct Help;

private:
    void timerCallback() override;
    void syncFromSession();
    void rebuildScaleBox();
    void label (const juce::String& text, juce::Component& target);
    void setupBar (juce::Slider&, const juce::String& name, const juce::String& tip, const char* paramId);

    TaktwerkProcessor& proc;
    juce::LookAndFeel_V4 laf;
    juce::TooltipWindow tooltips { this, 450 };

    juce::ComboBox trackBox, typeBox, keyBox, scaleBox, styleBox, progPresetBox, progLenBox, chordBox;
    juce::Slider swing, density, complexity, humanize, gate, octave, channel;
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD" }, helpBtn { "?" }, genBtn { "GENERATE" }, varyBtn { "VARY" },
                     clearBtn { "CLEAR" }, learnBtn { "LEARN" }, clearInputBtn { "RESET INPUT" }, followBtn, slotBtn[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> trackAtt, chordAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtts;
    juce::OwnedArray<juce::Label> labels;
    std::vector<std::pair<juce::Label*, juce::Component*>> labelTargets;

    std::unique_ptr<Grid> grid;
    std::unique_ptr<Prog> prog;
    std::unique_ptr<Guide> guide;
    std::unique_ptr<Arrange> arrange;
    std::unique_ptr<DragTile> dragPattern, dragSong;
    std::unique_ptr<Help> help;
    std::unique_ptr<juce::FileChooser> chooser;

    int lastSig = -1, lastSessionRev = -1, lastStyle = -1, lastTrack = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaktwerkEditor)
};
