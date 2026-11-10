#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Music.h"

struct StepData { bool on = false; int vel = 100; int degree = 0; int len = 1; };

class TaktwerkProcessor : public juce::AudioProcessor
{
public:
    enum { numSlots = 4, numLanes = 6, numSteps = 16, degMin = -7, degMax = 9 };

    TaktwerkProcessor();
    ~TaktwerkProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Taktwerk"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    std::unique_ptr<juce::XmlElement> toXml (bool withSession) const;
    void fromXml (const juce::XmlElement&, bool withSession);

    // Pattern access (lock-free, usable from both threads)
    StepData getStep (int slot, int lane, int step) const;
    void setStep (int slot, int lane, int step, const StepData&);
    void clearSlot (int slot);
    void generate (int slot);
    void vary (int from, int to);
    void setTypeFromUser (int type);

    int type() const  { return (int) pType->load(); }
    int track() const { return (int) pTrack->load(); }
    int slotForBar (int bar) const;

    // Notes one step produces. Returns the note count (0 if the step is silent).
    int notesForStep (int slot, int lane, int step, int bar, int* notes, int& vel, double& lenSteps) const;
    juce::File writeMidiFile (bool wholeSong) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<tw::Session> session;
    std::atomic<int> curSlot { 0 }, learn { 0 }, patternRev { 0 }, uiStep { -1 }, uiBar { 0 };
    std::atomic<int> laneNote[numLanes];

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void flushNotes (juce::MidiBuffer&, int sample);
    void noteOn (juce::MidiBuffer&, int sample, int note, int vel, double offPpq);
    void learnNote (const juce::MidiMessage&, double ppq);

    std::atomic<uint32_t> steps[numSlots][numLanes][numSteps];
    std::atomic<float> *pType, *pTrack, *pDensity, *pComplexity, *pHumanize, *pGate, *pOctave, *pChord, *pChannel;

    struct Pending { int note = -1; int ch = 1; double off = 0; };
    Pending pending[64];
    struct Held { long k = -1; int lane = 0; } held[128];
    double sampleRate = 44100.0, expectedPpq = 0.0;
    bool wasPlaying = false;
    uint32_t rng = 0x12345u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaktwerkProcessor)
};
