#pragma once

#include "../dsp/AmpHead.h"
#include "../dsp/CabinetIRLoader.h"
#include "../dsp/ChromaticTuner.h"
#include "../dsp/CompressorStomp.h"
#include "../dsp/GraphicEq.h"
#include "../dsp/LooperRecorder.h"
#include "../dsp/NoiseGate.h"
#include "../dsp/OverdriveStomp.h"
#include "../dsp/ReverbUnit.h"
#include "../dsp/StereoChorus.h"
#include "../dsp/StereoDelay.h"
#include "../dsp/WahStomp.h"
#include "ModuleId.h"
#include "PedalSlot.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

struct ParallelMixerParams {
    RoutingMode mode = RoutingMode::Serial;
    float pathALevelDb = 0.0f;
    float pathAPan = 0.0f;      // -1..+1
    bool pathAMute = false;
    float pathBLevelDb = -4.0f;
    float pathBPan = 0.0f;      // -1..+1
    bool pathBMute = false;
    bool pathBPhaseInvert = false;
    float pathCLevelDb = -6.0f;
    float pathCPan = 0.0f;      // -1..+1
    bool pathCMute = false;
    bool pathCPhaseInvert = false;
};

enum class InputChannelMode : int {
    Stereo = 0,     // L -> L, R -> R
    MonoLeft = 1,   // Ch 1 (Left) -> L + R (standard single-cable guitar interface)
    MonoRight = 2,  // Ch 2 (Right) -> L + R
    SumMono = 3     // 0.5*(L+R) -> L + R
};

struct ChainParams {
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    bool tunerMute = false;

    InputChannelMode inputMode = InputChannelMode::Stereo;
    bool inputPhaseInvert = false;
    float inputRumbleHpfHz = 0.0f; // 0 = OFF, or 40 / 60 / 80 Hz
    bool outputSafetyLimiter = true;
    float tempoBpm = 120.0f;

    float looperLevelDb = 0.0f;
    bool looperPreFx = false;

    ParallelMixerParams mixer;

    NoiseGate::Params gate;
    CompressorStomp::Params compressor;
    WahStomp::Params wah;
    OverdriveStomp::Params overdrive;
    AmpHead::Params ampA;
    AmpHead::Params ampB;
    AmpHead::Params ampC;
    CabinetIRLoader::Params cabA;
    CabinetIRLoader::Params cabB;
    CabinetIRLoader::Params cabC;
    GraphicEq::Params eq;
    StereoChorus::Params chorus;
    StereoDelay::Params delay;
    ReverbUnit::Params reverb;

    bool useDynamicChain = false;
    int numChainNodes = 0;
    std::array<ChainNode, 16> chainNodes {};
    std::array<PedalSlotSnapshot, kMaxPedalSlots> pedalSlots {};
    std::array<PedalSlotSnapshot, 3> rackSlots {};
};

// Modular real-time safe DSP engine with:
// - 10 reorderable modules + Dual Parallel A/B routing
// - Independent Amp A / NAM A + Cab A (4x12) on Path A AND Amp B / NAM B + Cab B (2x12) on Path B
// - Pre-FX (DI) or Post-FX Stereo Looper + 24-bit Background WAV Recorder
// - Smoothed I/O & Parallel Mixer gains (zero zipper noise)
class SignalChain {
public:
    SignalChain() noexcept
        : cabinetIrA(CabinetIRLoader::BuiltInVoicing::Closed4x12),
          cabinetIrB(CabinetIRLoader::BuiltInVoicing::Open2x12),
          cabinetIrC(CabinetIRLoader::BuiltInVoicing::Open2x12) {
        packedOrder.store(packChainOrder(defaultChainOrder()), std::memory_order_release);
        packedBuses.store(packBusAssignments(defaultBusAssignments()), std::memory_order_release);
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        const double sr = std::max(8000.0, spec.sampleRate);
        engineSampleRate = sr;
        inputHpfZ1.fill(0.0f);
        inputHpfPrevX.fill(0.0f);
        tuner.prepare(sr);
        noiseGate.prepare(spec);
        compressor.prepare(spec);
        wah.prepare(spec);
        overdrive.prepare(spec);
        ampHeadA.prepare(spec);
        ampHeadB.prepare(spec);
        ampHeadC.prepare(spec);
        cabinetIrA.prepare(spec);
        cabinetIrB.prepare(spec);
        cabinetIrC.prepare(spec);
        graphicEq.prepare(spec);
        stereoChorus.prepare(spec);
        stereoDelay.prepare(spec);
        reverb.prepare(spec);
        for (auto& s : customSlotDsps) {
            s.prepare(spec);
        }
        looper.prepare(sr);
        recorder.prepare(sr);

        smoothedInGain.reset(sr, 0.015);
        smoothedOutGain.reset(sr, 0.015);
        smoothedPathAL.reset(sr, 0.015);
        smoothedPathAR.reset(sr, 0.015);
        smoothedPathBL.reset(sr, 0.015);
        smoothedPathBR.reset(sr, 0.015);
        smoothedPathCL.reset(sr, 0.015);
        smoothedPathCR.reset(sr, 0.015);

        pathBBuffer.setSize(
            std::max(2, static_cast<int>(spec.numChannels)),
            std::max(256, static_cast<int>(spec.maximumBlockSize)),
            false, true, true
        );
        pathCBuffer.setSize(
            std::max(2, static_cast<int>(spec.numChannels)),
            std::max(256, static_cast<int>(spec.maximumBlockSize)),
            false, true, true
        );
        resetMeters();
    }

    void reset() noexcept {
        inputHpfZ1.fill(0.0f);
        inputHpfPrevX.fill(0.0f);
        noiseGate.reset();
        compressor.reset();
        wah.reset();
        overdrive.reset();
        ampHeadA.reset();
        ampHeadB.reset();
        ampHeadC.reset();
        cabinetIrA.reset();
        cabinetIrB.reset();
        cabinetIrC.reset();
        graphicEq.reset();
        stereoChorus.reset();
        stereoDelay.reset();
        reverb.reset();
        for (auto& s : customSlotDsps) {
            s.reset();
        }
        resetMeters();
    }

    void setChainOrder(const ChainOrder& order) noexcept {
        packedOrder.store(packChainOrder(order), std::memory_order_release);
    }

    ChainOrder getChainOrder() const noexcept {
        return unpackChainOrder(packedOrder.load(std::memory_order_acquire));
    }

    void setBusAssignments(const BusAssignments& buses) noexcept {
        packedBuses.store(packBusAssignments(buses), std::memory_order_release);
    }

    BusAssignments getBusAssignments() const noexcept {
        return unpackBusAssignments(packedBuses.load(std::memory_order_acquire));
    }

    void process(juce::AudioBuffer<float>& buffer, const ChainParams& p) noexcept {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) {
            return;
        }

        // Guitar Input Channel Routing (Mono Ch 1 / Ch 2 / Sum Mono / Stereo)
        if (numChannels >= 2) {
            if (p.inputMode == InputChannelMode::MonoLeft) {
                buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
            } else if (p.inputMode == InputChannelMode::MonoRight) {
                buffer.copyFrom(0, 0, buffer, 1, 0, numSamples);
            } else if (p.inputMode == InputChannelMode::SumMono) {
                float* chL = buffer.getWritePointer(0);
                float* chR = buffer.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i) {
                    const float m = 0.5f * (chL[i] + chR[i]);
                    chL[i] = m;
                    chR[i] = m;
                }
            }
        }

        // Smoothed input trim + optional input polarity invert
        const float polarity = p.inputPhaseInvert ? -1.0f : 1.0f;
        smoothedInGain.setTargetValue(juce::Decibels::decibelsToGain(p.inputGainDb) * polarity);
        for (int i = 0; i < numSamples; ++i) {
            const float g = smoothedInGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch) {
                buffer.getWritePointer(ch)[i] *= g;
            }
        }

        // Optional Global Input Sub-Rumble / DC High-Pass Filter (40 / 60 / 80 Hz)
        if (p.inputRumbleHpfHz > 1.0f) {
            const float cutHz = std::clamp(p.inputRumbleHpfHz, 15.0f, 160.0f);
            const float rc = 1.0f / (2.0f * std::numbers::pi_v<float> * cutHz);
            const float dt = 1.0f / static_cast<float>(std::max(8000.0, engineSampleRate));
            const float alpha = rc / (rc + dt);
            const int hpfChannels = std::min(numChannels, 2);
            for (int ch = 0; ch < hpfChannels; ++ch) {
                float* ptr = buffer.getWritePointer(ch);
                float z1 = inputHpfZ1[static_cast<size_t>(ch)];
                float prevX = inputHpfPrevX[static_cast<size_t>(ch)];
                for (int i = 0; i < numSamples; ++i) {
                    const float x = ptr[i];
                    const float y = alpha * (z1 + x - prevX);
                    prevX = x;
                    z1 = y;
                    ptr[i] = y;
                }
                inputHpfZ1[static_cast<size_t>(ch)] = z1;
                inputHpfPrevX[static_cast<size_t>(ch)] = prevX;
            }
        }

        // Feed lock-free chromatic tuner & pre-FX meters
        tuner.pushBlock(buffer);
        updatePeakAtomics(buffer, inputPeakL, inputPeakR);

        if (p.tunerMute) {
            buffer.clear();
            outputPeakL.store(0.0f, std::memory_order_relaxed);
            outputPeakR.store(0.0f, std::memory_order_relaxed);
            return;
        }

        looper.setLoopLevelDb(p.looperLevelDb);
        looper.setPreFxMode(p.looperPreFx);

        // Optional Pre-FX DI Looper placement (great for auditioning tones hands-free!)
        if (p.looperPreFx) {
            looper.process(buffer);
        }

        if (p.useDynamicChain && p.numChainNodes > 0) {
            const int nodeCount = std::min(p.numChainNodes, static_cast<int>(p.chainNodes.size()));
            const auto mode = p.mixer.mode;

            auto mixPathAAndB = [&](juce::AudioBuffer<float>& bufA, juce::AudioBuffer<float>& bufB, int activeCh) {
                const float gainA = p.mixer.pathAMute ? 0.0f : juce::Decibels::decibelsToGain(p.mixer.pathALevelDb);
                const float gainB = p.mixer.pathBMute ? 0.0f
                                                      : (p.mixer.pathBPhaseInvert ? -1.0f : 1.0f)
                                                        * juce::Decibels::decibelsToGain(p.mixer.pathBLevelDb);

                const float panA = (std::clamp(p.mixer.pathAPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
                const float panB = (std::clamp(p.mixer.pathBPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
                smoothedPathAL.setTargetValue(gainA * std::cos(panA) * 1.41421356f);
                smoothedPathAR.setTargetValue(gainA * std::sin(panA) * 1.41421356f);
                smoothedPathBL.setTargetValue(gainB * std::cos(panB) * 1.41421356f);
                smoothedPathBR.setTargetValue(gainB * std::sin(panB) * 1.41421356f);

                float* dstL = bufA.getWritePointer(0);
                const float* srcBL = bufB.getReadPointer(0);
                if (activeCh >= 2) {
                    float* dstR = bufA.getWritePointer(1);
                    const float* srcBR = bufB.getReadPointer(1);
                    for (int i = 0; i < numSamples; ++i) {
                        const float aL = smoothedPathAL.getNextValue();
                        const float aR = smoothedPathAR.getNextValue();
                        const float bL = smoothedPathBL.getNextValue();
                        const float bR = smoothedPathBR.getNextValue();
                        dstL[i] = dstL[i] * aL + srcBL[i] * bL;
                        dstR[i] = dstR[i] * aR + srcBR[i] * bR;
                    }
                } else {
                    for (int i = 0; i < numSamples; ++i) {
                        const float aL = smoothedPathAL.getNextValue();
                        const float bL = smoothedPathBL.getNextValue();
                        (void)smoothedPathAR.getNextValue();
                        (void)smoothedPathBR.getNextValue();
                        dstL[i] = dstL[i] * aL + srcBL[i] * bL;
                    }
                }
            };

            auto mixPathsABC = [&](juce::AudioBuffer<float>& bufA,
                                   juce::AudioBuffer<float>& bufB,
                                   juce::AudioBuffer<float>& bufC,
                                   int activeCh) {
                const float gainA = p.mixer.pathAMute ? 0.0f : juce::Decibels::decibelsToGain(p.mixer.pathALevelDb);
                const float gainB = p.mixer.pathBMute ? 0.0f
                                                      : (p.mixer.pathBPhaseInvert ? -1.0f : 1.0f)
                                                        * juce::Decibels::decibelsToGain(p.mixer.pathBLevelDb);
                const float gainC = p.mixer.pathCMute ? 0.0f
                                                      : (p.mixer.pathCPhaseInvert ? -1.0f : 1.0f)
                                                        * juce::Decibels::decibelsToGain(p.mixer.pathCLevelDb);

                const float panA = (std::clamp(p.mixer.pathAPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
                const float panB = (std::clamp(p.mixer.pathBPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
                const float panC = (std::clamp(p.mixer.pathCPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
                smoothedPathAL.setTargetValue(gainA * std::cos(panA) * 1.41421356f);
                smoothedPathAR.setTargetValue(gainA * std::sin(panA) * 1.41421356f);
                smoothedPathBL.setTargetValue(gainB * std::cos(panB) * 1.41421356f);
                smoothedPathBR.setTargetValue(gainB * std::sin(panB) * 1.41421356f);
                smoothedPathCL.setTargetValue(gainC * std::cos(panC) * 1.41421356f);
                smoothedPathCR.setTargetValue(gainC * std::sin(panC) * 1.41421356f);

                float* dstL = bufA.getWritePointer(0);
                const float* srcBL = bufB.getReadPointer(0);
                const float* srcCL = bufC.getReadPointer(0);
                if (activeCh >= 2) {
                    float* dstR = bufA.getWritePointer(1);
                    const float* srcBR = bufB.getReadPointer(1);
                    const float* srcCR = bufC.getReadPointer(1);
                    for (int i = 0; i < numSamples; ++i) {
                        const float aL = smoothedPathAL.getNextValue();
                        const float aR = smoothedPathAR.getNextValue();
                        const float bL = smoothedPathBL.getNextValue();
                        const float bR = smoothedPathBR.getNextValue();
                        const float cL = smoothedPathCL.getNextValue();
                        const float cR = smoothedPathCR.getNextValue();
                        dstL[i] = dstL[i] * aL + srcBL[i] * bL + srcCL[i] * cL;
                        dstR[i] = dstR[i] * aR + srcBR[i] * bR + srcCR[i] * cR;
                    }
                } else {
                    for (int i = 0; i < numSamples; ++i) {
                        const float aL = smoothedPathAL.getNextValue();
                        const float bL = smoothedPathBL.getNextValue();
                        const float cL = smoothedPathCL.getNextValue();
                        (void)smoothedPathAR.getNextValue();
                        (void)smoothedPathBR.getNextValue();
                        (void)smoothedPathCR.getNextValue();
                        dstL[i] = dstL[i] * aL + srcBL[i] * bL + srcCL[i] * cL;
                    }
                }
            };

            if (mode == RoutingMode::Serial || pathBBuffer.getNumSamples() < numSamples) {
                // Mode 0: Single Serial Chain (IN -> all nodes -> MAIN mixer strip -> OUT)
                for (int i = 0; i < nodeCount; ++i) {
                    processChainNode(p.chainNodes[static_cast<size_t>(i)], buffer, p, false);
                }

                const float gainA = p.mixer.pathAMute ? 0.0f : juce::Decibels::decibelsToGain(p.mixer.pathALevelDb);
                const float panA = (std::clamp(p.mixer.pathAPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
                smoothedPathAL.setTargetValue(gainA * std::cos(panA) * 1.41421356f);
                smoothedPathAR.setTargetValue(gainA * std::sin(panA) * 1.41421356f);

                float* dstL = buffer.getWritePointer(0);
                if (numChannels >= 2) {
                    float* dstR = buffer.getWritePointer(1);
                    for (int i = 0; i < numSamples; ++i) {
                        dstL[i] *= smoothedPathAL.getNextValue();
                        dstR[i] *= smoothedPathAR.getNextValue();
                    }
                } else {
                    for (int i = 0; i < numSamples; ++i) {
                        dstL[i] *= smoothedPathAL.getNextValue();
                        (void)smoothedPathAR.getNextValue();
                    }
                }
            } else if (mode == RoutingMode::DualIndependent) {
                // Mode 3: Dual Independent Parallel Chains with True Pre-Amp & Post-Amp Separation on BOTH Chains!
                const int activeCh = std::min(numChannels, pathBBuffer.getNumChannels());
                for (int ch = 0; ch < activeCh; ++ch) {
                    pathBBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                }
                juce::AudioBuffer<float> pathBView(pathBBuffer.getArrayOfWritePointers(), activeCh, numSamples);

                // Chain A (Top): [Pre A pedals] -> Amp A -> Cab A -> [Post A pedals] -> RackMod
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::Pre || nd.bus == BusLane::PathA)) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
                ampHeadA.process(buffer, p.ampA);
                cabinetIrA.process(buffer, p.cabA);
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::PostA || nd.bus == BusLane::PathC)) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::RackMod) {
                        processChainNode(nd, buffer, p, false);
                    }
                }

                // Chain B (Bottom): [Pre B pedals] -> Amp B -> Cab B -> [Post B pedals] -> RackDelay -> RackReverb
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::PathB) {
                        processChainNode(nd, pathBView, p, true);
                    }
                }
                ampHeadB.process(pathBView, p.ampB);
                cabinetIrB.process(pathBView, p.cabB);
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::Post) {
                        processChainNode(nd, pathBView, p, true);
                    }
                }
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::RackDelay || nd.kind == ChainNodeKind::RackReverb) {
                        processChainNode(nd, pathBView, p, true);
                    }
                }

                mixPathAAndB(buffer, pathBView, activeCh);
            } else if (mode == RoutingMode::TripleParallel && pathCBuffer.getNumSamples() >= numSamples) {
                // Mode 2: True Triple Parallel (Shared Pre -> Path A || Path B || Path C -> 3-Ch Mix -> Shared Post -> Out)
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::Pre) {
                        processChainNode(nd, buffer, p, false);
                    }
                }

                const int activeCh = std::min(numChannels, std::min(pathBBuffer.getNumChannels(), pathCBuffer.getNumChannels()));
                for (int ch = 0; ch < activeCh; ++ch) {
                    pathBBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                    pathCBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                }
                juce::AudioBuffer<float> pathBView(pathBBuffer.getArrayOfWritePointers(), activeCh, numSamples);
                juce::AudioBuffer<float> pathCView(pathCBuffer.getArrayOfWritePointers(), activeCh, numSamples);

                // Branch 1 (Top - Path A): PathA pedals -> Amp A -> Cab A
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::PathA) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
                ampHeadA.process(buffer, p.ampA);
                cabinetIrA.process(buffer, p.cabA);

                // Branch 2 (Middle - Path B): PathB pedals -> Amp B -> Cab B
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::PathB) {
                        processChainNode(nd, pathBView, p, true);
                    }
                }
                ampHeadB.process(pathBView, p.ampB);
                cabinetIrB.process(pathBView, p.cabB);

                // Branch 3 (Bottom - Path C): PathC pedals -> Amp C -> Cab C
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::PathC) {
                        processChainNode(nd, pathCView, p, false);
                    }
                }
                ampHeadC.process(pathCView, p.ampC);
                cabinetIrC.process(pathCView, p.cabC);

                // 3-Channel Smoothed Console Mixer (Path A + Path B + Path C)
                mixPathsABC(buffer, pathBView, pathCView, activeCh);

                // Shared Post-Mixer Master FX Bus (Post pedals + Studio Rack MOD / DLY / RVB)
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::Post || nd.bus == BusLane::PostA)) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::RackMod ||
                        nd.kind == ChainNodeKind::RackDelay ||
                        nd.kind == ChainNodeKind::RackReverb) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
            } else {
                // Mode 1: Dual Split (Shared Pre -> Path A || Path B -> Mix -> Shared Post)
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::Pre) {
                        processChainNode(nd, buffer, p, false);
                    }
                }

                const int activeCh = std::min(numChannels, pathBBuffer.getNumChannels());
                for (int ch = 0; ch < activeCh; ++ch) {
                    pathBBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                }
                juce::AudioBuffer<float> pathBView(pathBBuffer.getArrayOfWritePointers(), activeCh, numSamples);

                // Path A (Top): PathA pedals -> Amp A -> Cab A
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::PathA) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
                ampHeadA.process(buffer, p.ampA);
                cabinetIrA.process(buffer, p.cabA);

                // Path B (Bottom): PathB (+ fallback PathC) pedals -> Amp B -> Cab B
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::PathB || nd.bus == BusLane::PathC)) {
                        processChainNode(nd, pathBView, p, true);
                    }
                }
                ampHeadB.process(pathBView, p.ampB);
                cabinetIrB.process(pathBView, p.cabB);

                mixPathAAndB(buffer, pathBView, activeCh);

                // Shared Post-Mixer Master FX bus (Post pedals + Studio Rack MOD / DLY / RVB)
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::Post || nd.bus == BusLane::PostA)) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
                for (int i = 0; i < nodeCount; ++i) {
                    const auto& nd = p.chainNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::RackMod ||
                        nd.kind == ChainNodeKind::RackDelay ||
                        nd.kind == ChainNodeKind::RackReverb) {
                        processChainNode(nd, buffer, p, false);
                    }
                }
            }
        } else {
            const ChainOrder order = getChainOrder();
            if (p.mixer.mode == RoutingMode::Serial || pathBBuffer.getNumSamples() < numSamples) {
            for (const ModuleId mod : order) {
                processSingleModule(mod, buffer, p, false);
            }
        } else {
            const BusAssignments buses = getBusAssignments();

            // 1. Pre-Split bus
            for (const ModuleId mod : order) {
                if (buses[static_cast<size_t>(mod)] == BusLane::Pre) {
                    processSingleModule(mod, buffer, p, false);
                }
            }

            // 2. Split signal into Path B buffer (pre-allocated)
            const int activeCh = std::min(numChannels, pathBBuffer.getNumChannels());
            for (int ch = 0; ch < activeCh; ++ch) {
                pathBBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }
            juce::AudioBuffer<float> pathBView(pathBBuffer.getArrayOfWritePointers(), activeCh, numSamples);

            // 3. Process Path A (uses Amp A + Cab A)
            for (const ModuleId mod : order) {
                if (buses[static_cast<size_t>(mod)] == BusLane::PathA) {
                    processSingleModule(mod, buffer, p, false);
                }
            }

            // 4. Process Path B (always has independent Amp B -> Cab B available, plus any modules assigned to Path B!)
            const bool ampBAssignedToB = (buses[static_cast<size_t>(ModuleId::AmpHead)] == BusLane::PathB);
            const bool cabBAssignedToB = (buses[static_cast<size_t>(ModuleId::CabinetIR)] == BusLane::PathB);
            if (!ampBAssignedToB) {
                ampHeadB.process(pathBView, p.ampB);
            }
            if (!cabBAssignedToB) {
                cabinetIrB.process(pathBView, p.cabB);
            }
            for (const ModuleId mod : order) {
                if (buses[static_cast<size_t>(mod)] == BusLane::PathB) {
                    processSingleModule(mod, pathBView, p, true);
                }
            }

            // 5. Sum Path A + Path B via smoothed Dual-Path Mixer
            const float gainA = p.mixer.pathAMute ? 0.0f : juce::Decibels::decibelsToGain(p.mixer.pathALevelDb);
            const float gainB = p.mixer.pathBMute ? 0.0f
                                                  : (p.mixer.pathBPhaseInvert ? -1.0f : 1.0f)
                                                    * juce::Decibels::decibelsToGain(p.mixer.pathBLevelDb);

            const float panA = (std::clamp(p.mixer.pathAPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
            const float panB = (std::clamp(p.mixer.pathBPan, -1.0f, 1.0f) + 1.0f) * 0.25f * std::numbers::pi_v<float>;
            smoothedPathAL.setTargetValue(gainA * std::cos(panA) * 1.41421356f);
            smoothedPathAR.setTargetValue(gainA * std::sin(panA) * 1.41421356f);
            smoothedPathBL.setTargetValue(gainB * std::cos(panB) * 1.41421356f);
            smoothedPathBR.setTargetValue(gainB * std::sin(panB) * 1.41421356f);

            float* dstL = buffer.getWritePointer(0);
            const float* srcBL = pathBView.getReadPointer(0);
            if (activeCh >= 2) {
                float* dstR = buffer.getWritePointer(1);
                const float* srcBR = pathBView.getReadPointer(1);
                for (int i = 0; i < numSamples; ++i) {
                    const float aL = smoothedPathAL.getNextValue();
                    const float aR = smoothedPathAR.getNextValue();
                    const float bL = smoothedPathBL.getNextValue();
                    const float bR = smoothedPathBR.getNextValue();
                    dstL[i] = dstL[i] * aL + srcBL[i] * bL;
                    dstR[i] = dstR[i] * aR + srcBR[i] * bR;
                }
            } else {
                for (int i = 0; i < numSamples; ++i) {
                    const float aL = smoothedPathAL.getNextValue();
                    const float bL = smoothedPathBL.getNextValue();
                    (void)smoothedPathAR.getNextValue();
                    (void)smoothedPathBR.getNextValue();
                    dstL[i] = dstL[i] * aL + srcBL[i] * bL;
                }
            }

            // 6. Post-Mixer Master bus
            for (const ModuleId mod : order) {
                if (buses[static_cast<size_t>(mod)] == BusLane::Post) {
                    processSingleModule(mod, buffer, p, false);
                }
            }
        }
        }

        // Standard Post-FX Looper placement (captures processed guitar tone)
        if (!p.looperPreFx) {
            looper.process(buffer);
        }

        // Smoothed Master Output Trim + Optional Studio Safety Soft-Limiter Ceiling
        smoothedOutGain.setTargetValue(juce::Decibels::decibelsToGain(p.outputGainDb));
        const bool useLimiter = p.outputSafetyLimiter;
        for (int i = 0; i < numSamples; ++i) {
            const float og = smoothedOutGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch) {
                float* ptr = buffer.getWritePointer(ch);
                float v = ptr[i] * og;
                if (useLimiter) {
                    const float av = std::abs(v);
                    if (av > 0.90f) {
                        const float sign = (v >= 0.0f) ? 1.0f : -1.0f;
                        v = sign * (0.90f + 0.095f * std::tanh((av - 0.90f) / 0.095f));
                    }
                    v = std::clamp(v, -0.995f, 0.995f);
                }
                ptr[i] = v;
            }
        }

        // Stream stereo master output to background WAV recorder when recording
        recorder.pushBlock(buffer);
        updatePeakAtomics(buffer, outputPeakL, outputPeakR);
    }

    ChromaticTuner& getTuner() noexcept { return tuner; }
    const ChromaticTuner& getTuner() const noexcept { return tuner; }

    CompressorStomp& getCompressor() noexcept { return compressor; }
    const CompressorStomp& getCompressor() const noexcept { return compressor; }

    float getSlotGainReductionDb(int slotId) const noexcept {
        if (slotId >= 0 && slotId < kMaxPedalSlots) {
            return customSlotDsps[static_cast<size_t>(slotId)].getGainReductionDb();
        }
        return compressor.getGainReductionDb();
    }

    NamModelRunner& getSlotNamRunner(int slotId) noexcept {
        return customSlotDsps[static_cast<size_t>(std::clamp(slotId, 0, kMaxPedalSlots - 1))].getNamRunner();
    }
    const NamModelRunner& getSlotNamRunner(int slotId) const noexcept {
        return customSlotDsps[static_cast<size_t>(std::clamp(slotId, 0, kMaxPedalSlots - 1))].getNamRunner();
    }

    bool isSlotUsingPackCapture(int slotId) const noexcept {
        if (slotId < 0 || slotId >= kMaxPedalSlots) return false;
        return customSlotDsps[static_cast<size_t>(slotId)].isUsingPackCapture();
    }

    juce::String getSlotPackCaptureName(int slotId) const {
        if (slotId < 0 || slotId >= kMaxPedalSlots) return {};
        return customSlotDsps[static_cast<size_t>(slotId)].getPackCaptureName();
    }

    void setSlotPreampColorEnabled(int slotId, bool enabled) {
        if (slotId >= 0 && slotId < kMaxPedalSlots) {
            customSlotDsps[static_cast<size_t>(slotId)].setPreampColorEnabled(enabled);
        }
    }

    bool isSlotPreampColorEnabled(int slotId) const noexcept {
        if (slotId < 0 || slotId >= kMaxPedalSlots) return false;
        return customSlotDsps[static_cast<size_t>(slotId)].isPreampColorEnabled();
    }

    AmpHead& getAmpHead(int rigIndex = 0) noexcept { return rigIndex == 2 ? ampHeadC : (rigIndex == 1 ? ampHeadB : ampHeadA); }
    const AmpHead& getAmpHead(int rigIndex = 0) const noexcept { return rigIndex == 2 ? ampHeadC : (rigIndex == 1 ? ampHeadB : ampHeadA); }

    CabinetIRLoader& getCabinet(int rigIndex = 0) noexcept { return rigIndex == 2 ? cabinetIrC : (rigIndex == 1 ? cabinetIrB : cabinetIrA); }
    const CabinetIRLoader& getCabinet(int rigIndex = 0) const noexcept { return rigIndex == 2 ? cabinetIrC : (rigIndex == 1 ? cabinetIrB : cabinetIrA); }

    StereoLooper& getLooper() noexcept { return looper; }
    const StereoLooper& getLooper() const noexcept { return looper; }

    WavAudioRecorder& getRecorder() noexcept { return recorder; }
    const WavAudioRecorder& getRecorder() const noexcept { return recorder; }

    float getInputPeakL() const noexcept { return inputPeakL.load(std::memory_order_relaxed); }
    float getInputPeakR() const noexcept { return inputPeakR.load(std::memory_order_relaxed); }
    float getOutputPeakL() const noexcept { return outputPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const noexcept { return outputPeakR.load(std::memory_order_relaxed); }

    void setNamLoudnessNormalization(bool enabled) noexcept {
        ampHeadA.getNamRunner().setLoudnessNormalizationEnabled(enabled);
        ampHeadB.getNamRunner().setLoudnessNormalizationEnabled(enabled);
        ampHeadC.getNamRunner().setLoudnessNormalizationEnabled(enabled);
        ampHeadA.getPackNamRunner().setLoudnessNormalizationEnabled(enabled);
        ampHeadB.getPackNamRunner().setLoudnessNormalizationEnabled(enabled);
        ampHeadC.getPackNamRunner().setLoudnessNormalizationEnabled(enabled);
        for (auto& s : customSlotDsps) {
            s.getNamRunner().setLoudnessNormalizationEnabled(enabled);
            s.getPackNamRunner().setLoudnessNormalizationEnabled(enabled);
        }
    }

    void reclaimAllRetiredNamModels() {
        ampHeadA.getNamRunner().reclaimRetired();
        ampHeadB.getNamRunner().reclaimRetired();
        ampHeadC.getNamRunner().reclaimRetired();
        ampHeadA.getPackNamRunner().reclaimRetired();
        ampHeadB.getPackNamRunner().reclaimRetired();
        ampHeadC.getPackNamRunner().reclaimRetired();
        for (auto& s : customSlotDsps) {
            s.getNamRunner().reclaimRetired();
            s.getPackNamRunner().reclaimRetired();
        }
    }

    void refreshAllModelPacks() {
        ModelPackRegistry::instance().rescan();
        ampHeadA.requestPackRefresh();
        ampHeadB.requestPackRefresh();
        ampHeadC.requestPackRefresh();
        cabinetIrA.requestPackRefresh();
        cabinetIrB.requestPackRefresh();
        cabinetIrC.requestPackRefresh();
        reverb.requestPackRefresh();
        for (auto& s : customSlotDsps) {
            s.requestPackRefresh();
        }
    }

    int getActiveNamModelCount() const noexcept {
        int count = 0;
        if (ampHeadA.getNamRunner().hasLoadedModel() || ampHeadA.isUsingPackNam()) ++count;
        if (ampHeadB.getNamRunner().hasLoadedModel() || ampHeadB.isUsingPackNam()) ++count;
        if (ampHeadC.getNamRunner().hasLoadedModel() || ampHeadC.isUsingPackNam()) ++count;
        for (const auto& s : customSlotDsps) {
            if (s.getNamRunner().hasLoadedModel() || s.getPackNamRunner().hasLoadedModel()) ++count;
        }
        return count;
    }

private:
    void processChainNode(const ChainNode& node, juce::AudioBuffer<float>& buf, const ChainParams& p, bool useRigB) noexcept {
        switch (node.kind) {
            case ChainNodeKind::PedalSlot:
                if (node.slotId >= 0 && node.slotId < kMaxPedalSlots) {
                    customSlotDsps[static_cast<size_t>(node.slotId)].process(buf, p.pedalSlots[static_cast<size_t>(node.slotId)], p.tempoBpm);
                }
                break;
            case ChainNodeKind::AmpHead:
                if (useRigB) ampHeadB.process(buf, p.ampB);
                else         ampHeadA.process(buf, p.ampA);
                break;
            case ChainNodeKind::CabinetIR:
                if (useRigB) cabinetIrB.process(buf, p.cabB);
                else         cabinetIrA.process(buf, p.cabA);
                break;
            case ChainNodeKind::RackMod:
                stereoChorus.process(buf, p.rackSlots[0].occupied ? buildChorusParamsFromSlot(p.rackSlots[0], p.tempoBpm) : p.chorus);
                break;
            case ChainNodeKind::RackDelay:
                stereoDelay.process(buf, p.rackSlots[1].occupied ? buildDelayParamsFromSlot(p.rackSlots[1], p.tempoBpm) : p.delay);
                break;
            case ChainNodeKind::RackReverb:
                reverb.process(buf, p.rackSlots[2].occupied ? buildReverbParamsFromSlot(p.rackSlots[2]) : p.reverb);
                break;
            default:
                break;
        }
    }

    void processSingleModule(ModuleId mod, juce::AudioBuffer<float>& buf, const ChainParams& p, bool useRigB) noexcept {
        switch (mod) {
            case ModuleId::NoiseGate:    noiseGate.process(buf, p.gate); break;
            case ModuleId::Compressor:   compressor.process(buf, p.compressor); break;
            case ModuleId::Wah:          wah.process(buf, p.wah); break;
            case ModuleId::Overdrive:    overdrive.process(buf, p.overdrive); break;
            case ModuleId::AmpHead:
                if (useRigB) ampHeadB.process(buf, p.ampB);
                else         ampHeadA.process(buf, p.ampA);
                break;
            case ModuleId::CabinetIR:
                if (useRigB) cabinetIrB.process(buf, p.cabB);
                else         cabinetIrA.process(buf, p.cabA);
                break;
            case ModuleId::GraphicEq:    graphicEq.process(buf, p.eq); break;
            case ModuleId::StereoChorus: stereoChorus.process(buf, p.chorus); break;
            case ModuleId::StereoDelay:  stereoDelay.process(buf, p.delay); break;
            case ModuleId::Reverb:       reverb.process(buf, p.reverb); break;
            default: break;
        }
    }

    void resetMeters() noexcept {
        inputPeakL.store(0.0f, std::memory_order_relaxed);
        inputPeakR.store(0.0f, std::memory_order_relaxed);
        outputPeakL.store(0.0f, std::memory_order_relaxed);
        outputPeakR.store(0.0f, std::memory_order_relaxed);
    }

    static void updatePeakAtomics(const juce::AudioBuffer<float>& buf,
                                  std::atomic<float>& atomL,
                                  std::atomic<float>& atomR) noexcept {
        const int n = buf.getNumSamples();
        const float pL = buf.getMagnitude(0, 0, n);
        const float pR = buf.getNumChannels() > 1 ? buf.getMagnitude(1, 0, n) : pL;
        atomL.store(pL, std::memory_order_relaxed);
        atomR.store(pR, std::memory_order_relaxed);
    }

    std::atomic<uint64_t> packedOrder { 0 };
    std::atomic<uint32_t> packedBuses { 0 };

    ChromaticTuner tuner;
    NoiseGate noiseGate;
    CompressorStomp compressor;
    WahStomp wah;
    OverdriveStomp overdrive;
    AmpHead ampHeadA;
    AmpHead ampHeadB;
    AmpHead ampHeadC;
    CabinetIRLoader cabinetIrA;
    CabinetIRLoader cabinetIrB;
    CabinetIRLoader cabinetIrC;
    GraphicEq graphicEq;
    StereoChorus stereoChorus;
    StereoDelay stereoDelay;
    ReverbUnit reverb;
    std::array<UniversalPedalSlotDsp, kMaxPedalSlots> customSlotDsps {};
    StereoLooper looper;
    WavAudioRecorder recorder;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedInGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPathAL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPathAR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPathBL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPathBR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPathCL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPathCR;

    juce::AudioBuffer<float> pathBBuffer;
    juce::AudioBuffer<float> pathCBuffer;

    std::atomic<float> inputPeakL { 0.0f };
    std::atomic<float> inputPeakR { 0.0f };
    std::atomic<float> outputPeakL { 0.0f };
    std::atomic<float> outputPeakR { 0.0f };

    double engineSampleRate = 48000.0;
    std::array<float, 2> inputHpfZ1 { 0.0f, 0.0f };
    std::array<float, 2> inputHpfPrevX { 0.0f, 0.0f };
};

} // namespace pedalboard
