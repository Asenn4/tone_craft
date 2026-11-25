#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pedalboard {

PedalBoardAudioProcessor::PedalBoardAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", params::createParameterLayout()) {
    using namespace params;

    midiLearnManager.bindToAPVTS(apvts);

    pInputGain     = apvts.getRawParameterValue(kInputGain);
    pOutputGain    = apvts.getRawParameterValue(kOutputGain);
    pTunerMute     = apvts.getRawParameterValue(kTunerMute);
    pLooperLevel   = apvts.getRawParameterValue(kLooperLevel);
    pLooperPreFx   = apvts.getRawParameterValue(kLooperPreFx);

    pRoutingMode   = apvts.getRawParameterValue(kRoutingMode);
    pPathALevel    = apvts.getRawParameterValue(kPathALevel);
    pPathAPan      = apvts.getRawParameterValue(kPathAPan);
    pPathAMute     = apvts.getRawParameterValue(kPathAMute);
    pPathBLevel    = apvts.getRawParameterValue(kPathBLevel);
    pPathBPan      = apvts.getRawParameterValue(kPathBPan);
    pPathBMute     = apvts.getRawParameterValue(kPathBMute);
    pPathBPhase    = apvts.getRawParameterValue(kPathBPhase);
    pPathCLevel    = apvts.getRawParameterValue(kPathCLevel);
    pPathCPan      = apvts.getRawParameterValue(kPathCPan);
    pPathCMute     = apvts.getRawParameterValue(kPathCMute);
    pPathCPhase    = apvts.getRawParameterValue(kPathCPhase);

    pGateBypass    = apvts.getRawParameterValue(kGateBypass);
    pGateModel     = apvts.getRawParameterValue(kGateModel);
    pGateThreshold = apvts.getRawParameterValue(kGateThreshold);
    pGateRelease   = apvts.getRawParameterValue(kGateRelease);
    pGateDepth     = apvts.getRawParameterValue(kGateDepth);

    pCompBypass    = apvts.getRawParameterValue(kCompBypass);
    pCompModel     = apvts.getRawParameterValue(kCompModel);
    pCompThreshold = apvts.getRawParameterValue(kCompThreshold);
    pCompRatio     = apvts.getRawParameterValue(kCompRatio);
    pCompAttack    = apvts.getRawParameterValue(kCompAttack);
    pCompRelease   = apvts.getRawParameterValue(kCompRelease);
    pCompMakeup    = apvts.getRawParameterValue(kCompMakeup);

    pWahBypass     = apvts.getRawParameterValue(kWahBypass);
    pWahModel      = apvts.getRawParameterValue(kWahModel);
    pWahPedal      = apvts.getRawParameterValue(kWahPedal);
    pWahQ          = apvts.getRawParameterValue(kWahQ);
    pWahSensitivity= apvts.getRawParameterValue(kWahSensitivity);
    pWahMix        = apvts.getRawParameterValue(kWahMix);
    pWahAuto       = apvts.getRawParameterValue(kWahAuto);

    pOdBypass      = apvts.getRawParameterValue(kOdBypass);
    pOdModel       = apvts.getRawParameterValue(kOdModel);
    pOdDrive       = apvts.getRawParameterValue(kOdDrive);
    pOdTone        = apvts.getRawParameterValue(kOdTone);
    pOdLevel       = apvts.getRawParameterValue(kOdLevel);

    pAmpBypass     = apvts.getRawParameterValue(kAmpBypass);
    pAmpChannel    = apvts.getRawParameterValue(kAmpChannel);
    pAmpModel      = apvts.getRawParameterValue(kAmpModel);
    pAmpGain       = apvts.getRawParameterValue(kAmpGain);
    pAmpBass       = apvts.getRawParameterValue(kAmpBass);
    pAmpMid        = apvts.getRawParameterValue(kAmpMid);
    pAmpTreble     = apvts.getRawParameterValue(kAmpTreble);
    pAmpPresence   = apvts.getRawParameterValue(kAmpPresence);
    pAmpMaster     = apvts.getRawParameterValue(kAmpMaster);
    pAmpExtra      = apvts.getRawParameterValue(kAmpExtra);
    pAmpSwitch     = apvts.getRawParameterValue(kAmpSwitch);

    pAmpBBypass    = apvts.getRawParameterValue(kAmpBBypass);
    pAmpBChannel   = apvts.getRawParameterValue(kAmpBChannel);
    pAmpBModel     = apvts.getRawParameterValue(kAmpBModel);
    pAmpBGain      = apvts.getRawParameterValue(kAmpBGain);
    pAmpBBass      = apvts.getRawParameterValue(kAmpBBass);
    pAmpBMid       = apvts.getRawParameterValue(kAmpBMid);
    pAmpBTreble    = apvts.getRawParameterValue(kAmpBTreble);
    pAmpBPresence  = apvts.getRawParameterValue(kAmpBPresence);
    pAmpBMaster    = apvts.getRawParameterValue(kAmpBMaster);
    pAmpBExtra     = apvts.getRawParameterValue(kAmpBExtra);
    pAmpBSwitch    = apvts.getRawParameterValue(kAmpBSwitch);

    pAmpCBypass    = apvts.getRawParameterValue(kAmpCBypass);
    pAmpCChannel   = apvts.getRawParameterValue(kAmpCChannel);
    pAmpCModel     = apvts.getRawParameterValue(kAmpCModel);
    pAmpCGain      = apvts.getRawParameterValue(kAmpCGain);
    pAmpCBass      = apvts.getRawParameterValue(kAmpCBass);
    pAmpCMid       = apvts.getRawParameterValue(kAmpCMid);
    pAmpCTreble    = apvts.getRawParameterValue(kAmpCTreble);
    pAmpCPresence  = apvts.getRawParameterValue(kAmpCPresence);
    pAmpCMaster    = apvts.getRawParameterValue(kAmpCMaster);
    pAmpCExtra     = apvts.getRawParameterValue(kAmpCExtra);
    pAmpCSwitch    = apvts.getRawParameterValue(kAmpCSwitch);

    pCabBypass     = apvts.getRawParameterValue(kCabBypass);
    pCabModel      = apvts.getRawParameterValue(kCabModel);
    pCabLowCut     = apvts.getRawParameterValue(kCabLowCut);
    pCabHighCut    = apvts.getRawParameterValue(kCabHighCut);
    pCabLevel      = apvts.getRawParameterValue(kCabLevel);

    pCabBBypass    = apvts.getRawParameterValue(kCabBBypass);
    pCabBModel     = apvts.getRawParameterValue(kCabBModel);
    pCabBLowCut    = apvts.getRawParameterValue(kCabBLowCut);
    pCabBHighCut   = apvts.getRawParameterValue(kCabBHighCut);
    pCabBLevel     = apvts.getRawParameterValue(kCabBLevel);

    pCabCBypass    = apvts.getRawParameterValue(kCabCBypass);
    pCabCModel     = apvts.getRawParameterValue(kCabCModel);
    pCabCLowCut    = apvts.getRawParameterValue(kCabCLowCut);
    pCabCHighCut   = apvts.getRawParameterValue(kCabCHighCut);
    pCabCLevel     = apvts.getRawParameterValue(kCabCLevel);

    pEqBypass      = apvts.getRawParameterValue(kEqBypass);
    pEqModel       = apvts.getRawParameterValue(kEqModel);
    pEqBands[0]    = apvts.getRawParameterValue(kEq100);
    pEqBands[1]    = apvts.getRawParameterValue(kEq200);
    pEqBands[2]    = apvts.getRawParameterValue(kEq400);
    pEqBands[3]    = apvts.getRawParameterValue(kEq800);
    pEqBands[4]    = apvts.getRawParameterValue(kEq1600);
    pEqBands[5]    = apvts.getRawParameterValue(kEq3200);
    pEqBands[6]    = apvts.getRawParameterValue(kEq6400);
    pEqLevel       = apvts.getRawParameterValue(kEqLevel);

    pChorusBypass  = apvts.getRawParameterValue(kChorusBypass);
    pChorusModel   = apvts.getRawParameterValue(kChorusModel);
    pChorusRate    = apvts.getRawParameterValue(kChorusRate);
    pChorusDepth   = apvts.getRawParameterValue(kChorusDepth);
    pChorusDelay   = apvts.getRawParameterValue(kChorusDelay);
    pChorusMix     = apvts.getRawParameterValue(kChorusMix);

    pDelayBypass   = apvts.getRawParameterValue(kDelayBypass);
    pDelayModel    = apvts.getRawParameterValue(kDelayModel);
    pDelayTime     = apvts.getRawParameterValue(kDelayTime);
    pDelayFeedback = apvts.getRawParameterValue(kDelayFeedback);
    pDelayMix      = apvts.getRawParameterValue(kDelayMix);
    pDelayPingPong = apvts.getRawParameterValue(kDelayPingPong);

    pReverbBypass  = apvts.getRawParameterValue(kReverbBypass);
    pReverbModel   = apvts.getRawParameterValue(kReverbModel);
    pReverbSize    = apvts.getRawParameterValue(kReverbSize);
    pReverbDamping = apvts.getRawParameterValue(kReverbDamping);
    pReverbMix     = apvts.getRawParameterValue(kReverbMix);

    setCurrentProgram(0); // Clean Init (0 effects loaded, Clean Amp + matching Clean Cab only)
    loadStudioSettings();
}

void PedalBoardAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentBlockSize.store(std::max(1, samplesPerBlock), std::memory_order_relaxed);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(std::max(1, samplesPerBlock));
    spec.numChannels = static_cast<juce::uint32>(std::max(1, getTotalNumOutputChannels()));
    signalChain.prepare(spec);
}

void PedalBoardAudioProcessor::releaseResources() {
    signalChain.getRecorder().stopRecording();
    signalChain.reset();
}

bool PedalBoardAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto outSet = layouts.getMainOutputChannelSet();
    const auto inSet = layouts.getMainInputChannelSet();
    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo()) {
        return false;
    }
    if (inSet != juce::AudioChannelSet::mono() && inSet != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return true;
}

RoutingMode PedalBoardAudioProcessor::getRoutingMode() const noexcept {
    if (pRoutingMode == nullptr) return RoutingMode::Serial;
    const int idx = std::clamp(static_cast<int>(std::lround(pRoutingMode->load(std::memory_order_relaxed))), 0, 3);
    return static_cast<RoutingMode>(idx);
}

juce::String PedalBoardAudioProcessor::getActivePresetName() const {
    std::lock_guard<std::mutex> lock(presetNameMutex);
    return activePresetName;
}

ChainParams PedalBoardAudioProcessor::readCurrentParams() const noexcept {
    ChainParams p;
    p.inputGainDb  = pInputGain->load(std::memory_order_relaxed);
    p.outputGainDb = pOutputGain->load(std::memory_order_relaxed);
    p.tunerMute    = pTunerMute->load(std::memory_order_relaxed) > 0.5f;
    p.inputMode           = getInputChannelMode();
    p.inputPhaseInvert    = inputPhaseInvert.load(std::memory_order_relaxed);
    p.inputRumbleHpfHz    = inputRumbleHpfHz.load(std::memory_order_relaxed);
    p.outputSafetyLimiter = outputSafetyLimiter.load(std::memory_order_relaxed);
    p.tempoBpm            = effectiveBpm.load(std::memory_order_relaxed);
    p.looperLevelDb = pLooperLevel->load(std::memory_order_relaxed);
    p.looperPreFx   = pLooperPreFx->load(std::memory_order_relaxed) > 0.5f;

    p.mixer.mode             = getRoutingMode();
    p.mixer.pathALevelDb     = pPathALevel->load(std::memory_order_relaxed);
    p.mixer.pathAPan         = pPathAPan->load(std::memory_order_relaxed);
    p.mixer.pathAMute        = pPathAMute->load(std::memory_order_relaxed) > 0.5f;
    p.mixer.pathBLevelDb     = pPathBLevel->load(std::memory_order_relaxed);
    p.mixer.pathBPan         = pPathBPan->load(std::memory_order_relaxed);
    p.mixer.pathBMute        = pPathBMute->load(std::memory_order_relaxed) > 0.5f;
    p.mixer.pathBPhaseInvert = pPathBPhase->load(std::memory_order_relaxed) > 0.5f;
    p.mixer.pathCLevelDb     = pPathCLevel->load(std::memory_order_relaxed);
    p.mixer.pathCPan         = pPathCPan->load(std::memory_order_relaxed);
    p.mixer.pathCMute        = pPathCMute->load(std::memory_order_relaxed) > 0.5f;
    p.mixer.pathCPhaseInvert = pPathCPhase->load(std::memory_order_relaxed) > 0.5f;

    p.gate.bypass      = pGateBypass->load(std::memory_order_relaxed) > 0.5f;
    p.gate.modelIndex  = static_cast<int>(std::lround(pGateModel->load(std::memory_order_relaxed)));
    p.gate.thresholdDb = pGateThreshold->load(std::memory_order_relaxed);
    p.gate.releaseMs   = pGateRelease->load(std::memory_order_relaxed);
    p.gate.depthDb     = pGateDepth->load(std::memory_order_relaxed);

    p.compressor.bypass      = pCompBypass->load(std::memory_order_relaxed) > 0.5f;
    p.compressor.modelIndex  = static_cast<int>(std::lround(pCompModel->load(std::memory_order_relaxed)));
    p.compressor.thresholdDb = pCompThreshold->load(std::memory_order_relaxed);
    p.compressor.ratio       = pCompRatio->load(std::memory_order_relaxed);
    p.compressor.attackMs    = pCompAttack->load(std::memory_order_relaxed);
    p.compressor.releaseMs   = pCompRelease->load(std::memory_order_relaxed);
    p.compressor.makeupDb    = pCompMakeup->load(std::memory_order_relaxed);
    p.compressor.tempoBpm    = p.tempoBpm;

    p.wah.bypass      = pWahBypass->load(std::memory_order_relaxed) > 0.5f;
    p.wah.modelIndex  = static_cast<int>(std::lround(pWahModel->load(std::memory_order_relaxed)));
    p.wah.pedal       = pWahPedal->load(std::memory_order_relaxed);
    p.wah.q           = pWahQ->load(std::memory_order_relaxed);
    p.wah.sensitivity = pWahSensitivity->load(std::memory_order_relaxed);
    p.wah.mix         = pWahMix->load(std::memory_order_relaxed);
    p.wah.autoWah     = pWahAuto->load(std::memory_order_relaxed) > 0.5f;

    p.overdrive.bypass     = pOdBypass->load(std::memory_order_relaxed) > 0.5f;
    p.overdrive.modelIndex = static_cast<int>(std::lround(pOdModel->load(std::memory_order_relaxed)));
    p.overdrive.drive      = pOdDrive->load(std::memory_order_relaxed);
    p.overdrive.tone       = pOdTone->load(std::memory_order_relaxed);
    p.overdrive.level      = pOdLevel->load(std::memory_order_relaxed);

    p.ampA.bypass     = pAmpBypass->load(std::memory_order_relaxed) > 0.5f;
    p.ampA.channel    = static_cast<int>(std::lround(pAmpChannel->load(std::memory_order_relaxed)));
    p.ampA.modelIndex = static_cast<int>(std::lround(pAmpModel->load(std::memory_order_relaxed)));
    p.ampA.gain       = pAmpGain->load(std::memory_order_relaxed);
    p.ampA.bass       = pAmpBass->load(std::memory_order_relaxed);
    p.ampA.mid        = pAmpMid->load(std::memory_order_relaxed);
    p.ampA.treble     = pAmpTreble->load(std::memory_order_relaxed);
    p.ampA.presence   = pAmpPresence->load(std::memory_order_relaxed);
    p.ampA.master     = pAmpMaster->load(std::memory_order_relaxed);
    p.ampA.extra      = pAmpExtra->load(std::memory_order_relaxed);
    p.ampA.switchVal  = static_cast<int>(std::lround(pAmpSwitch->load(std::memory_order_relaxed)));

    p.ampB.bypass     = pAmpBBypass->load(std::memory_order_relaxed) > 0.5f;
    p.ampB.channel    = static_cast<int>(std::lround(pAmpBChannel->load(std::memory_order_relaxed)));
    p.ampB.modelIndex = static_cast<int>(std::lround(pAmpBModel->load(std::memory_order_relaxed)));
    p.ampB.gain       = pAmpBGain->load(std::memory_order_relaxed);
    p.ampB.bass       = pAmpBBass->load(std::memory_order_relaxed);
    p.ampB.mid        = pAmpBMid->load(std::memory_order_relaxed);
    p.ampB.treble     = pAmpBTreble->load(std::memory_order_relaxed);
    p.ampB.presence   = pAmpBPresence->load(std::memory_order_relaxed);
    p.ampB.master     = pAmpBMaster->load(std::memory_order_relaxed);
    p.ampB.extra      = pAmpBExtra->load(std::memory_order_relaxed);
    p.ampB.switchVal  = static_cast<int>(std::lround(pAmpBSwitch->load(std::memory_order_relaxed)));

    p.ampC.bypass     = pAmpCBypass->load(std::memory_order_relaxed) > 0.5f;
    p.ampC.channel    = static_cast<int>(std::lround(pAmpCChannel->load(std::memory_order_relaxed)));
    p.ampC.modelIndex = static_cast<int>(std::lround(pAmpCModel->load(std::memory_order_relaxed)));
    p.ampC.gain       = pAmpCGain->load(std::memory_order_relaxed);
    p.ampC.bass       = pAmpCBass->load(std::memory_order_relaxed);
    p.ampC.mid        = pAmpCMid->load(std::memory_order_relaxed);
    p.ampC.treble     = pAmpCTreble->load(std::memory_order_relaxed);
    p.ampC.presence   = pAmpCPresence->load(std::memory_order_relaxed);
    p.ampC.master     = pAmpCMaster->load(std::memory_order_relaxed);
    p.ampC.extra      = pAmpCExtra->load(std::memory_order_relaxed);
    p.ampC.switchVal  = static_cast<int>(std::lround(pAmpCSwitch->load(std::memory_order_relaxed)));

    p.cabA.bypass     = pCabBypass->load(std::memory_order_relaxed) > 0.5f;
    p.cabA.modelIndex = static_cast<int>(std::lround(pCabModel->load(std::memory_order_relaxed)));
    p.cabA.lowCutHz   = pCabLowCut->load(std::memory_order_relaxed);
    p.cabA.highCutHz  = pCabHighCut->load(std::memory_order_relaxed);
    p.cabA.levelDb    = pCabLevel->load(std::memory_order_relaxed);

    p.cabB.bypass     = pCabBBypass->load(std::memory_order_relaxed) > 0.5f;
    p.cabB.modelIndex = static_cast<int>(std::lround(pCabBModel->load(std::memory_order_relaxed)));
    p.cabB.lowCutHz   = pCabBLowCut->load(std::memory_order_relaxed);
    p.cabB.highCutHz  = pCabBHighCut->load(std::memory_order_relaxed);
    p.cabB.levelDb    = pCabBLevel->load(std::memory_order_relaxed);

    p.cabC.bypass     = pCabCBypass->load(std::memory_order_relaxed) > 0.5f;
    p.cabC.modelIndex = static_cast<int>(std::lround(pCabCModel->load(std::memory_order_relaxed)));
    p.cabC.lowCutHz   = pCabCLowCut->load(std::memory_order_relaxed);
    p.cabC.highCutHz  = pCabCHighCut->load(std::memory_order_relaxed);
    p.cabC.levelDb    = pCabCLevel->load(std::memory_order_relaxed);

    p.eq.bypass     = pEqBypass->load(std::memory_order_relaxed) > 0.5f;
    p.eq.modelIndex = static_cast<int>(std::lround(pEqModel->load(std::memory_order_relaxed)));
    for (int b = 0; b < GraphicEq::kNumBands; ++b) {
        p.eq.bandsDb[static_cast<size_t>(b)] = pEqBands[static_cast<size_t>(b)]->load(std::memory_order_relaxed);
    }
    p.eq.levelDb = pEqLevel->load(std::memory_order_relaxed);

    p.chorus.bypass     = pChorusBypass->load(std::memory_order_relaxed) > 0.5f;
    p.chorus.modelIndex = static_cast<int>(std::lround(pChorusModel->load(std::memory_order_relaxed)));
    p.chorus.rateHz     = pChorusRate->load(std::memory_order_relaxed);
    p.chorus.depth      = pChorusDepth->load(std::memory_order_relaxed);
    p.chorus.delayMs    = pChorusDelay->load(std::memory_order_relaxed);
    p.chorus.mix        = pChorusMix->load(std::memory_order_relaxed);
    p.chorus.tempoBpm   = p.tempoBpm;

    p.delay.bypass     = pDelayBypass->load(std::memory_order_relaxed) > 0.5f;
    p.delay.modelIndex = static_cast<int>(std::lround(pDelayModel->load(std::memory_order_relaxed)));
    p.delay.timeMs     = pDelayTime->load(std::memory_order_relaxed);
    p.delay.feedback   = pDelayFeedback->load(std::memory_order_relaxed);
    p.delay.mix        = pDelayMix->load(std::memory_order_relaxed);
    p.delay.pingPong   = pDelayPingPong->load(std::memory_order_relaxed) > 0.5f;
    p.delay.tempoBpm   = p.tempoBpm;

    p.reverb.bypass     = pReverbBypass->load(std::memory_order_relaxed) > 0.5f;
    p.reverb.modelIndex = static_cast<int>(std::lround(pReverbModel->load(std::memory_order_relaxed)));
    p.reverb.size       = pReverbSize->load(std::memory_order_relaxed);
    p.reverb.damping    = pReverbDamping->load(std::memory_order_relaxed);
    p.reverb.mix        = pReverbMix->load(std::memory_order_relaxed);

    const int boardIdx = activeAudioBoardIdx.load(std::memory_order_acquire) & 1;
    const auto& bs = audioBoardBuffers[static_cast<size_t>(boardIdx)];
    if (bs.numNodes > 0) {
        p.useDynamicChain = true;
        p.numChainNodes   = bs.numNodes;
        p.chainNodes      = bs.nodes;
        p.pedalSlots      = bs.slots;
        p.rackSlots       = bs.rackSlots;
    }
    return p;
}

void PedalBoardAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const auto blockStartTicks = juce::Time::getHighResolutionTicks();

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2) {
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
    }
    for (int ch = std::max(totalNumInputChannels, 2); ch < totalNumOutputChannels; ++ch) {
        buffer.clear(ch, 0, buffer.getNumSamples());
    }

    midiLearnManager.processMidiBuffer(midiMessages);

    const bool allowProgramChange = midiProgramChangeEnabled.load(std::memory_order_relaxed);
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isController()) {
            const int ccNum = msg.getControllerNumber();
            const int ccVal = msg.getControllerValue();
            lastMidiType.store(1, std::memory_order_relaxed);
            lastMidiChannel.store(msg.getChannel(), std::memory_order_relaxed);
            lastMidiData1.store(ccNum, std::memory_order_relaxed);
            lastMidiData2.store(ccVal, std::memory_order_relaxed);
            lastMidiTimestampMs.store(static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()), std::memory_order_release);

            if (expressionLearnArmed.exchange(false, std::memory_order_acq_rel)) {
                expressionPedalCc.store(ccNum, std::memory_order_release);
                notifyUiStateChanged();
                juce::MessageManager::callAsync([safeThis = juce::WeakReference<PedalBoardAudioProcessor>(this)] {
                    if (safeThis != nullptr) {
                        safeThis->saveStudioSettings();
                    }
                });
            }
            if (ccNum == expressionPedalCc.load(std::memory_order_acquire)) {
                const float norm = std::clamp(static_cast<float>(ccVal) / 127.0f, 0.0f, 1.0f);
                juce::MessageManager::callAsync([safeThis = juce::WeakReference<PedalBoardAudioProcessor>(this), norm] {
                    if (safeThis != nullptr) {
                        safeThis->setExpressionPedalPosition(norm);
                    }
                });
            }
        } else if (msg.isProgramChange()) {
            const int prog = msg.getProgramChangeNumber();
            lastMidiType.store(2, std::memory_order_relaxed);
            lastMidiChannel.store(msg.getChannel(), std::memory_order_relaxed);
            lastMidiData1.store(prog, std::memory_order_relaxed);
            lastMidiData2.store(0, std::memory_order_relaxed);
            lastMidiTimestampMs.store(static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()), std::memory_order_release);

            if (allowProgramChange && prog >= 0 && prog < getNumPrograms()) {
                juce::MessageManager::callAsync([safeThis = juce::WeakReference<PedalBoardAudioProcessor>(this), prog] {
                    if (safeThis != nullptr) {
                        safeThis->setCurrentProgram(prog);
                    }
                });
            }
        } else if (msg.isNoteOn()) {
            lastMidiType.store(3, std::memory_order_relaxed);
            lastMidiChannel.store(msg.getChannel(), std::memory_order_relaxed);
            lastMidiData1.store(msg.getNoteNumber(), std::memory_order_relaxed);
            lastMidiData2.store(static_cast<int>(msg.getVelocity()), std::memory_order_relaxed);
            lastMidiTimestampMs.store(static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()), std::memory_order_release);
        }
    }

    // Read live tempo BPM from Host DAW PlayHead in VST3, falling back to Master/Tap BPM in Standalone
    float activeBpm = manualBpm.load(std::memory_order_relaxed);
    bool fromHost = false;
    if (auto* ph = getPlayHead()) {
        if (const auto pos = ph->getPosition()) {
            if (const auto hostBpm = pos->getBpm()) {
                if (*hostBpm >= 30.0 && *hostBpm <= 300.0) {
                    activeBpm = static_cast<float>(*hostBpm);
                    fromHost = true;
                }
            }
        }
    }
    effectiveBpm.store(activeBpm, std::memory_order_relaxed);
    hostTempoSynced.store(fromHost, std::memory_order_relaxed);

    const auto chainParams = readCurrentParams();
    signalChain.process(buffer, chainParams);

    const double sr = getSampleRate();
    const int nSamp = buffer.getNumSamples();
    if (sr > 1000.0 && nSamp > 0) {
        const double elapsedSec = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - blockStartTicks);
        const double blockSec = static_cast<double>(nSamp) / sr;
        const float instantPct = static_cast<float>(std::clamp((elapsedSec / blockSec) * 100.0, 0.0, 100.0));
        const float prevPct = dspCpuLoadPercent.load(std::memory_order_relaxed);
        dspCpuLoadPercent.store(prevPct * 0.85f + instantPct * 0.15f, std::memory_order_relaxed);
    }
}

int PedalBoardAudioProcessor::getNumPrograms() {
    return static_cast<int>(presetManager.getFactoryPresets().size());
}

int PedalBoardAudioProcessor::getCurrentProgram() {
    return currentProgramIndex.load(std::memory_order_relaxed);
}

void PedalBoardAudioProcessor::setCurrentProgram(int index) {
    const auto& presets = presetManager.getFactoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        return;
    }
    currentProgramIndex.store(index, std::memory_order_relaxed);
    applyPresetData(presets[static_cast<size_t>(index)]);
}

const juce::String PedalBoardAudioProcessor::getProgramName(int index) {
    const auto& presets = presetManager.getFactoryPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
        return presets[static_cast<size_t>(index)].name;
    }
    return {};
}

void PedalBoardAudioProcessor::changeProgramName(int index, const juce::String& newName) {
    juce::ignoreUnused(index, newName);
}

void PedalBoardAudioProcessor::setChainOrder(const ChainOrder& order) {
    signalChain.setChainOrder(order);
    syncDynamicBoardFromApvts();
    notifyUiStateChanged();
}

ChainOrder PedalBoardAudioProcessor::getChainOrder() const noexcept {
    return signalChain.getChainOrder();
}

void PedalBoardAudioProcessor::moveModule(ModuleId id, int delta) {
    auto order = signalChain.getChainOrder();
    int idx = -1;
    for (int i = 0; i < kNumModules; ++i) {
        if (order[static_cast<size_t>(i)] == id) {
            idx = i;
            break;
        }
    }
    const int target = idx + delta;
    if (idx < 0 || target < 0 || target >= kNumModules) {
        return;
    }
    std::swap(order[static_cast<size_t>(idx)], order[static_cast<size_t>(target)]);
    setChainOrder(order);
}

void PedalBoardAudioProcessor::moveModuleToSlot(ModuleId dragged, int targetSlotIndex) {
    if (targetSlotIndex < 0 || targetSlotIndex >= kNumModules) {
        return;
    }
    auto order = signalChain.getChainOrder();
    int fromIdx = -1;
    for (int i = 0; i < kNumModules; ++i) {
        if (order[static_cast<size_t>(i)] == dragged) {
            fromIdx = i;
            break;
        }
    }
    if (fromIdx < 0 || fromIdx == targetSlotIndex) {
        return;
    }
    if (fromIdx < targetSlotIndex) {
        for (int i = fromIdx; i < targetSlotIndex; ++i) {
            order[static_cast<size_t>(i)] = order[static_cast<size_t>(i + 1)];
        }
    } else {
        for (int i = fromIdx; i > targetSlotIndex; --i) {
            order[static_cast<size_t>(i)] = order[static_cast<size_t>(i - 1)];
        }
    }
    order[static_cast<size_t>(targetSlotIndex)] = dragged;
    setChainOrder(order);
}

void PedalBoardAudioProcessor::setBusAssignments(const BusAssignments& buses) {
    signalChain.setBusAssignments(buses);
    notifyUiStateChanged();
}

BusAssignments PedalBoardAudioProcessor::getBusAssignments() const noexcept {
    return signalChain.getBusAssignments();
}

void PedalBoardAudioProcessor::setModuleBusLane(ModuleId id, BusLane lane) {
    auto buses = signalChain.getBusAssignments();
    buses[static_cast<size_t>(id)] = lane;
    setBusAssignments(buses);
}

void PedalBoardAudioProcessor::cycleModuleBusLane(ModuleId id) {
    auto buses = signalChain.getBusAssignments();
    const uint8_t next = static_cast<uint8_t>((static_cast<uint8_t>(buses[static_cast<size_t>(id)]) + 1u) % 4u);
    buses[static_cast<size_t>(id)] = static_cast<BusLane>(next);
    setBusAssignments(buses);
}

void PedalBoardAudioProcessor::publishBoardStateLocked() noexcept {
    const int nextIdx = 1 - (activeAudioBoardIdx.load(std::memory_order_relaxed) & 1);
    audioBoardBuffers[static_cast<size_t>(nextIdx)] = workingBoardState;
    activeAudioBoardIdx.store(nextIdx, std::memory_order_release);
    notifyUiStateChanged();
}

DynamicBoardState PedalBoardAudioProcessor::getDynamicBoardState() const noexcept {
    const int idx = activeAudioBoardIdx.load(std::memory_order_acquire) & 1;
    return audioBoardBuffers[static_cast<size_t>(idx)];
}

std::vector<ChainNode> PedalBoardAudioProcessor::getDynamicChainNodes() const {
    const auto bs = getDynamicBoardState();
    std::vector<ChainNode> out;
    const int n = std::min(bs.numNodes, static_cast<int>(bs.nodes.size()));
    out.reserve(static_cast<size_t>(std::max(0, n)));
    for (int i = 0; i < n; ++i) {
        const auto& nd = bs.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot) {
            if (nd.slotId < 0 || nd.slotId >= kMaxPedalSlots || !bs.slots[static_cast<size_t>(nd.slotId)].occupied) {
                continue;
            }
        }
        out.push_back(nd);
    }
    return out;
}

std::vector<int> PedalBoardAudioProcessor::getActivePedalSlotIds() const {
    const auto bs = getDynamicBoardState();
    std::vector<int> ids;
    const int n = std::min(bs.numNodes, static_cast<int>(bs.nodes.size()));
    for (int i = 0; i < n; ++i) {
        const auto& nd = bs.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot &&
            nd.slotId >= 0 && nd.slotId < kMaxPedalSlots &&
            bs.slots[static_cast<size_t>(nd.slotId)].occupied) {
            ids.push_back(nd.slotId);
        }
    }
    return ids;
}

PedalSlotSnapshot PedalBoardAudioProcessor::getPedalSlot(int slotId) const noexcept {
    if (slotId < 0 || slotId >= kMaxPedalSlots) {
        return {};
    }
    const auto bs = getDynamicBoardState();
    return bs.slots[static_cast<size_t>(slotId)];
}

void PedalBoardAudioProcessor::syncPrimaryApvtsFromSlot(const PedalSlotSnapshot& s) {
    auto setRawParam = [this](const char* pid, float realVal) {
        if (auto* p = apvts.getParameter(pid)) {
            p->setValueNotifyingHost(p->convertTo0to1(realVal));
        }
    };
    switch (s.category) {
        case ModuleId::NoiseGate:
            setRawParam(params::kGateBypass,    s.bypassed ? 1.0f : 0.0f);
            setRawParam(params::kGateModel,     static_cast<float>(s.modelIndex));
            setRawParam(params::kGateThreshold, s.params[0]);
            break;
        case ModuleId::Compressor:
            setRawParam(params::kCompBypass, s.bypassed ? 1.0f : 0.0f);
            setRawParam(params::kCompModel,  static_cast<float>(s.modelIndex));
            break;
        case ModuleId::Wah:
            setRawParam(params::kWahBypass, s.bypassed ? 1.0f : 0.0f);
            setRawParam(params::kWahModel,  static_cast<float>(s.modelIndex));
            setRawParam(params::kWahPedal,  s.params[0]);
            break;
        case ModuleId::Overdrive:
            setRawParam(params::kOdBypass, s.bypassed ? 1.0f : 0.0f);
            setRawParam(params::kOdModel,  static_cast<float>(s.modelIndex));
            break;
        case ModuleId::GraphicEq:
            setRawParam(params::kEqBypass, s.bypassed ? 1.0f : 0.0f);
            setRawParam(params::kEqModel,  static_cast<float>(s.modelIndex));
            break;
        default:
            break;
    }
}

void PedalBoardAudioProcessor::syncDynamicBoardFromApvts(bool emptyStompboard) {
    std::lock_guard<std::mutex> lock(boardStateMutex);
    DynamicBoardState bs {};

    if (!emptyStompboard) {
        // Slot 0: NoiseGate
        const int gateModel = static_cast<int>(std::lround(pGateModel->load(std::memory_order_relaxed)));
        bs.slots[0].occupied   = true;
        bs.slots[0].bypassed   = pGateBypass->load(std::memory_order_relaxed) > 0.5f;
        bs.slots[0].category   = ModuleId::NoiseGate;
        bs.slots[0].modelIndex = gateModel;
        bs.slots[0].params     = defaultParamsForCategory(ModuleId::NoiseGate, gateModel);
        bs.slots[0].params[0]  = pGateThreshold->load(std::memory_order_relaxed);
        if (gateModel >= 1) {
            bs.slots[0].params[2] = pGateRelease->load(std::memory_order_relaxed);
        }

        // Slot 1: Compressor / PRE
        const int preModel = static_cast<int>(std::lround(pCompModel->load(std::memory_order_relaxed)));
        bs.slots[1].occupied   = true;
        bs.slots[1].bypassed   = pCompBypass->load(std::memory_order_relaxed) > 0.5f;
        bs.slots[1].category   = ModuleId::Compressor;
        bs.slots[1].modelIndex = preModel;
        bs.slots[1].params     = defaultParamsForCategory(ModuleId::Compressor, preModel);
        if (preModel == 2) { // S-Comp maps directly from legacy compressor APVTS
            bs.slots[1].params[0] = pCompThreshold->load(std::memory_order_relaxed);
            bs.slots[1].params[1] = pCompRatio->load(std::memory_order_relaxed);
            bs.slots[1].params[2] = pCompMakeup->load(std::memory_order_relaxed);
            bs.slots[1].params[3] = pCompAttack->load(std::memory_order_relaxed);
            bs.slots[1].params[4] = pCompRelease->load(std::memory_order_relaxed);
        }

        // Slot 2: Wah
        const int wahModel = static_cast<int>(std::lround(pWahModel->load(std::memory_order_relaxed)));
        bs.slots[2].occupied   = true;
        bs.slots[2].bypassed   = pWahBypass->load(std::memory_order_relaxed) > 0.5f;
        bs.slots[2].category   = ModuleId::Wah;
        bs.slots[2].modelIndex = wahModel;
        bs.slots[2].params     = defaultParamsForCategory(ModuleId::Wah, wahModel);
        bs.slots[2].params[0]  = pWahPedal->load(std::memory_order_relaxed);
        bs.slots[2].params[2]  = pWahQ->load(std::memory_order_relaxed);

        // Slot 3: Overdrive / DST
        const int dstModel = static_cast<int>(std::lround(pOdModel->load(std::memory_order_relaxed)));
        bs.slots[3].occupied   = true;
        bs.slots[3].bypassed   = pOdBypass->load(std::memory_order_relaxed) > 0.5f;
        bs.slots[3].category   = ModuleId::Overdrive;
        bs.slots[3].modelIndex = dstModel;
        bs.slots[3].params     = defaultParamsForCategory(ModuleId::Overdrive, dstModel);
        const auto dstSpecs = getSlotParamSpecs(ModuleId::Overdrive, dstModel);
        if (dstSpecs.size() >= 3 && std::string(dstSpecs[0].label) == "GAIN" && std::string(dstSpecs[1].label) == "TONE") {
            bs.slots[3].params[0] = pOdDrive->load(std::memory_order_relaxed);
            bs.slots[3].params[1] = pOdTone->load(std::memory_order_relaxed);
            bs.slots[3].params[2] = pOdLevel->load(std::memory_order_relaxed);
        }

        // Slot 4: GraphicEq
        const int eqModel = static_cast<int>(std::lround(pEqModel->load(std::memory_order_relaxed)));
        bs.slots[4].occupied   = true;
        bs.slots[4].bypassed   = pEqBypass->load(std::memory_order_relaxed) > 0.5f;
        bs.slots[4].category   = ModuleId::GraphicEq;
        bs.slots[4].modelIndex = eqModel;
        bs.slots[4].params     = defaultParamsForCategory(ModuleId::GraphicEq, eqModel);
        const int eqBands = GraphicEq::bandCountForModel(eqModel);
        for (int b = 0; b < std::min(eqBands, GraphicEq::kNumBands); ++b) {
            bs.slots[4].params[static_cast<size_t>(b)] = pEqBands[static_cast<size_t>(b)]->load(std::memory_order_relaxed);
        }
        if (GraphicEq::hasVolumeForModel(eqModel)) {
            bs.slots[4].params[static_cast<size_t>(eqBands)] = pEqLevel->load(std::memory_order_relaxed);
        }
    }

    // Studio Rack Slots (0 = MOD, 1 = DLY, 2 = RVB) with authentic per-model Excel params
    const int modModel = static_cast<int>(std::lround(pChorusModel->load(std::memory_order_relaxed)));
    bs.rackSlots[0].occupied   = true;
    bs.rackSlots[0].bypassed   = pChorusBypass->load(std::memory_order_relaxed) > 0.5f;
    bs.rackSlots[0].category   = ModuleId::StereoChorus;
    bs.rackSlots[0].modelIndex = modModel;
    bs.rackSlots[0].params     = defaultParamsForCategory(ModuleId::StereoChorus, modModel);

    const int dlyModel = static_cast<int>(std::lround(pDelayModel->load(std::memory_order_relaxed)));
    bs.rackSlots[1].occupied   = true;
    bs.rackSlots[1].bypassed   = pDelayBypass->load(std::memory_order_relaxed) > 0.5f;
    bs.rackSlots[1].category   = ModuleId::StereoDelay;
    bs.rackSlots[1].modelIndex = dlyModel;
    bs.rackSlots[1].params     = defaultParamsForCategory(ModuleId::StereoDelay, dlyModel);

    const int rvbModel = static_cast<int>(std::lround(pReverbModel->load(std::memory_order_relaxed)));
    bs.rackSlots[2].occupied   = true;
    bs.rackSlots[2].bypassed   = pReverbBypass->load(std::memory_order_relaxed) > 0.5f;
    bs.rackSlots[2].category   = ModuleId::Reverb;
    bs.rackSlots[2].modelIndex = rvbModel;
    bs.rackSlots[2].params     = defaultParamsForCategory(ModuleId::Reverb, rvbModel);

    const auto order = signalChain.getChainOrder();
    const auto buses = signalChain.getBusAssignments();
    int nodeIdx = 0;
    for (const auto mod : order) {
        if (nodeIdx >= static_cast<int>(bs.nodes.size())) break;
        const auto bus = buses[static_cast<size_t>(mod)];
        switch (mod) {
            case ModuleId::NoiseGate:    if (!emptyStompboard) bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::PedalSlot, 0, bus }; break;
            case ModuleId::Compressor:   if (!emptyStompboard) bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::PedalSlot, 1, bus }; break;
            case ModuleId::Wah:          if (!emptyStompboard) bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::PedalSlot, 2, bus }; break;
            case ModuleId::Overdrive:    if (!emptyStompboard) bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::PedalSlot, 3, bus }; break;
            case ModuleId::GraphicEq:    if (!emptyStompboard) bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::PedalSlot, 4, bus }; break;
            case ModuleId::AmpHead:      bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::AmpHead,   0, bus }; break;
            case ModuleId::CabinetIR:    bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::CabinetIR, 0, bus }; break;
            case ModuleId::StereoChorus: bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::RackMod,   0, bus }; break;
            case ModuleId::StereoDelay:  bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::RackDelay, 0, bus }; break;
            case ModuleId::Reverb:       bs.nodes[static_cast<size_t>(nodeIdx++)] = { ChainNodeKind::RackReverb,0, bus }; break;
            default: break;
        }
    }
    bs.numNodes = nodeIdx;
    workingBoardState = bs;
    publishBoardStateLocked();
}

int PedalBoardAudioProcessor::addPedalSlot(ModuleId category, int modelIndex, int insertAtPedalOrderIdx, int explicitLane) {
    std::lock_guard<std::mutex> lock(boardStateMutex);

    int freeSlot = -1;
    for (int i = 0; i < kMaxPedalSlots; ++i) {
        if (!workingBoardState.slots[static_cast<size_t>(i)].occupied) {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot < 0 || workingBoardState.numNodes >= static_cast<int>(workingBoardState.nodes.size())) {
        return -1;
    }

    auto& sl = workingBoardState.slots[static_cast<size_t>(freeSlot)];
    sl.occupied   = true;
    sl.bypassed   = false;
    sl.category   = category;
    sl.modelIndex = std::clamp(modelIndex, 0, std::max(0, static_cast<int>(gp200::modelsForModule(category).size()) - 1));
    sl.params     = defaultParamsForCategory(category, sl.modelIndex);

    // Find target chain insertion index based on insertAtPedalOrderIdx
    int insertChainIdx = workingBoardState.numNodes;
    int seenPedals = 0;
    int lastPedalChainIdx = -1;
    int ampChainIdx = -1;

    for (int i = 0; i < workingBoardState.numNodes; ++i) {
        const auto& nd = workingBoardState.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::AmpHead && ampChainIdx < 0) {
            ampChainIdx = i;
        }
        if (nd.kind == ChainNodeKind::PedalSlot) {
            if (insertAtPedalOrderIdx >= 0 && seenPedals == insertAtPedalOrderIdx) {
                insertChainIdx = i;
                break;
            }
            lastPedalChainIdx = i;
            ++seenPedals;
        }
    }

    if (insertAtPedalOrderIdx < 0 || insertChainIdx == workingBoardState.numNodes) {
        if (lastPedalChainIdx >= 0) {
            insertChainIdx = lastPedalChainIdx + 1;
        } else if (ampChainIdx >= 0) {
            insertChainIdx = ampChainIdx;
        } else {
            insertChainIdx = 0;
        }
    }

    insertChainIdx = std::clamp(insertChainIdx, 0, workingBoardState.numNodes);
    for (int i = workingBoardState.numNodes; i > insertChainIdx; --i) {
        workingBoardState.nodes[static_cast<size_t>(i)] = workingBoardState.nodes[static_cast<size_t>(i - 1)];
    }
    const BusLane chosenBus = (explicitLane >= 0 && explicitLane <= static_cast<int>(BusLane::PostA))
                                  ? static_cast<BusLane>(explicitLane)
                                  : ((category == ModuleId::StereoDelay || category == ModuleId::Reverb)
                                         ? BusLane::Post
                                         : BusLane::Pre);
    workingBoardState.nodes[static_cast<size_t>(insertChainIdx)] = { ChainNodeKind::PedalSlot, freeSlot, chosenBus };
    workingBoardState.numNodes++;

    publishBoardStateLocked();
    return freeSlot;
}

int PedalBoardAudioProcessor::addPedalSlotAtChainIndex(ModuleId category, int modelIndex, int targetChainIdx, BusLane lane) {
    std::lock_guard<std::mutex> lock(boardStateMutex);

    int freeSlot = -1;
    for (int i = 0; i < kMaxPedalSlots; ++i) {
        if (!workingBoardState.slots[static_cast<size_t>(i)].occupied) {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot < 0 || workingBoardState.numNodes >= static_cast<int>(workingBoardState.nodes.size())) {
        return -1;
    }

    auto& sl = workingBoardState.slots[static_cast<size_t>(freeSlot)];
    sl.occupied   = true;
    sl.bypassed   = false;
    sl.category   = category;
    sl.modelIndex = std::clamp(modelIndex, 0, std::max(0, static_cast<int>(gp200::modelsForModule(category).size()) - 1));
    sl.params     = defaultParamsForCategory(category, sl.modelIndex);

    const int ins = std::clamp(targetChainIdx, 0, workingBoardState.numNodes);
    for (int i = workingBoardState.numNodes; i > ins; --i) {
        workingBoardState.nodes[static_cast<size_t>(i)] = workingBoardState.nodes[static_cast<size_t>(i - 1)];
    }
    workingBoardState.nodes[static_cast<size_t>(ins)] = { ChainNodeKind::PedalSlot, freeSlot, lane };
    workingBoardState.numNodes++;

    publishBoardStateLocked();
    return freeSlot;
}

void PedalBoardAudioProcessor::removePedalSlot(int slotId) {
    if (slotId < 0 || slotId >= kMaxPedalSlots) return;
    signalChain.getSlotNamRunner(slotId).clearModel();
    std::lock_guard<std::mutex> lock(boardStateMutex);

    workingBoardState.slots[static_cast<size_t>(slotId)].occupied = false;
    int writeIdx = 0;
    for (int i = 0; i < workingBoardState.numNodes; ++i) {
        const auto& nd = workingBoardState.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot && nd.slotId == slotId) {
            continue;
        }
        workingBoardState.nodes[static_cast<size_t>(writeIdx++)] = nd;
    }
    workingBoardState.numNodes = writeIdx;
    publishBoardStateLocked();
}

void PedalBoardAudioProcessor::replacePedalSlot(int slotId, ModuleId newCategory, int newModelIndex) {
    if (slotId < 0 || slotId >= kMaxPedalSlots) return;
    if (!isNamPedalSlotModel(newCategory, newModelIndex)) {
        signalChain.getSlotNamRunner(slotId).clearModel();
    }
    PedalSlotSnapshot snapCopy {};
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        auto& sl = workingBoardState.slots[static_cast<size_t>(slotId)];
        const int clampedModel = std::clamp(newModelIndex, 0, std::max(0, static_cast<int>(gp200::modelsForModule(newCategory).size()) - 1));
        if (!sl.occupied || sl.category != newCategory || sl.modelIndex != clampedModel) {
            sl.params = defaultParamsForCategory(newCategory, clampedModel);
        }
        sl.occupied   = true;
        sl.bypassed   = false;
        sl.category   = newCategory;
        sl.modelIndex = clampedModel;
        snapCopy = sl;
        publishBoardStateLocked();
    }
    syncPrimaryApvtsFromSlot(snapCopy);
}

void PedalBoardAudioProcessor::setPedalSlotModel(int slotId, int newModelIndex) {
    if (slotId < 0 || slotId >= kMaxPedalSlots) return;
    PedalSlotSnapshot snapCopy {};
    bool clearNam = false;
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        auto& sl = workingBoardState.slots[static_cast<size_t>(slotId)];
        if (!sl.occupied) return;
        const int clampedModel = std::clamp(newModelIndex, 0, std::max(0, static_cast<int>(gp200::modelsForModule(sl.category).size()) - 1));
        if (sl.modelIndex != clampedModel) {
            clearNam = isNamPedalSlotModel(sl.category, sl.modelIndex) && !isNamPedalSlotModel(sl.category, clampedModel);
            sl.params = defaultParamsForCategory(sl.category, clampedModel);
        }
        sl.modelIndex = clampedModel;
        snapCopy = sl;
        publishBoardStateLocked();
    }
    if (clearNam) {
        signalChain.getSlotNamRunner(slotId).clearModel();
    }
    syncPrimaryApvtsFromSlot(snapCopy);
}

void PedalBoardAudioProcessor::setPedalSlotBypass(int slotId, bool bypassed) {
    if (slotId < 0 || slotId >= kMaxPedalSlots) return;
    PedalSlotSnapshot snapCopy {};
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        auto& sl = workingBoardState.slots[static_cast<size_t>(slotId)];
        if (!sl.occupied) return;
        sl.bypassed = bypassed;
        snapCopy = sl;
        publishBoardStateLocked();
    }
    syncPrimaryApvtsFromSlot(snapCopy);
}

void PedalBoardAudioProcessor::setPedalSlotParam(int slotId, int paramIdx, float value) {
    if (slotId < 0 || slotId >= kMaxPedalSlots || paramIdx < 0 || paramIdx >= kMaxSlotParams) return;
    PedalSlotSnapshot snapCopy {};
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        auto& sl = workingBoardState.slots[static_cast<size_t>(slotId)];
        if (!sl.occupied) return;
        sl.params[static_cast<size_t>(paramIdx)] = value;
        snapCopy = sl;
        publishBoardStateLocked();
    }
    syncPrimaryApvtsFromSlot(snapCopy);
}

PedalSlotSnapshot PedalBoardAudioProcessor::getRackSlot(int rackIdx) const noexcept {
    if (rackIdx < 0 || rackIdx >= 3) return {};
    const auto bs = getDynamicBoardState();
    return bs.rackSlots[static_cast<size_t>(rackIdx)];
}

void PedalBoardAudioProcessor::setRackSlotModel(int rackIdx, int newModelIndex) {
    if (rackIdx < 0 || rackIdx >= 3) return;
    static constexpr ModuleId kMods[3] = { ModuleId::StereoChorus, ModuleId::StereoDelay, ModuleId::Reverb };
    static constexpr const char* kModelIds[3] = { params::kChorusModel, params::kDelayModel, params::kReverbModel };
    const ModuleId mod = kMods[rackIdx];
    const int clampedModel = std::clamp(newModelIndex, 0, std::max(0, static_cast<int>(gp200::modelsForModule(mod).size()) - 1));
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        auto& rs = workingBoardState.rackSlots[static_cast<size_t>(rackIdx)];
        rs.occupied = true;
        rs.category = mod;
        if (rs.modelIndex != clampedModel) {
            rs.params = defaultParamsForCategory(mod, clampedModel);
        }
        rs.modelIndex = clampedModel;
        publishBoardStateLocked();
    }
    if (auto* p = apvts.getParameter(kModelIds[rackIdx])) {
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(clampedModel)));
    }
}

void PedalBoardAudioProcessor::setRackSlotBypass(int rackIdx, bool bypassed) {
    if (rackIdx < 0 || rackIdx >= 3) return;
    static constexpr const char* kBypassIds[3] = { params::kChorusBypass, params::kDelayBypass, params::kReverbBypass };
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        auto& rs = workingBoardState.rackSlots[static_cast<size_t>(rackIdx)];
        rs.occupied = true;
        rs.bypassed = bypassed;
        publishBoardStateLocked();
    }
    if (auto* p = apvts.getParameter(kBypassIds[rackIdx])) {
        p->setValueNotifyingHost(bypassed ? 1.0f : 0.0f);
    }
}

void PedalBoardAudioProcessor::setRackSlotParam(int rackIdx, int paramIdx, float value) {
    if (rackIdx < 0 || rackIdx >= 3 || paramIdx < 0 || paramIdx >= kMaxSlotParams) return;
    std::lock_guard<std::mutex> lock(boardStateMutex);
    auto& rs = workingBoardState.rackSlots[static_cast<size_t>(rackIdx)];
    rs.occupied = true;
    rs.params[static_cast<size_t>(paramIdx)] = value;
    publishBoardStateLocked();
}

void PedalBoardAudioProcessor::movePedalSlotInStompOrder(int slotId, int targetStompIdx) {
    std::lock_guard<std::mutex> lock(boardStateMutex);
    std::vector<int> pedalChainPositions;
    std::vector<ChainNode> pedalNodes;
    int fromStompIdx = -1;

    for (int i = 0; i < workingBoardState.numNodes; ++i) {
        const auto& nd = workingBoardState.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot &&
            nd.slotId >= 0 && nd.slotId < kMaxPedalSlots &&
            workingBoardState.slots[static_cast<size_t>(nd.slotId)].occupied) {
            if (nd.slotId == slotId) {
                fromStompIdx = static_cast<int>(pedalNodes.size());
            }
            pedalChainPositions.push_back(i);
            pedalNodes.push_back(nd);
        }
    }

    const int count = static_cast<int>(pedalNodes.size());
    if (fromStompIdx < 0 || count <= 1) return;
    targetStompIdx = std::clamp(targetStompIdx, 0, count - 1);
    if (fromStompIdx == targetStompIdx) return;

    const ChainNode moved = pedalNodes[static_cast<size_t>(fromStompIdx)];
    pedalNodes.erase(pedalNodes.begin() + fromStompIdx);
    pedalNodes.insert(pedalNodes.begin() + targetStompIdx, moved);

    for (int i = 0; i < count; ++i) {
        workingBoardState.nodes[static_cast<size_t>(pedalChainPositions[static_cast<size_t>(i)])] = pedalNodes[static_cast<size_t>(i)];
    }
    publishBoardStateLocked();
}

void PedalBoardAudioProcessor::moveDynamicChainNode(int fromChainIdx, int toChainIdx) {
    std::lock_guard<std::mutex> lock(boardStateMutex);
    const int n = workingBoardState.numNodes;
    if (fromChainIdx < 0 || fromChainIdx >= n || toChainIdx < 0 || toChainIdx >= n || fromChainIdx == toChainIdx) {
        return;
    }
    const ChainNode moved = workingBoardState.nodes[static_cast<size_t>(fromChainIdx)];
    if (fromChainIdx < toChainIdx) {
        for (int i = fromChainIdx; i < toChainIdx; ++i) {
            workingBoardState.nodes[static_cast<size_t>(i)] = workingBoardState.nodes[static_cast<size_t>(i + 1)];
        }
    } else {
        for (int i = fromChainIdx; i > toChainIdx; --i) {
            workingBoardState.nodes[static_cast<size_t>(i)] = workingBoardState.nodes[static_cast<size_t>(i - 1)];
        }
    }
    workingBoardState.nodes[static_cast<size_t>(toChainIdx)] = moved;
    publishBoardStateLocked();
}

void PedalBoardAudioProcessor::cycleDynamicChainNodeBus(int chainIdx) {
    std::lock_guard<std::mutex> lock(boardStateMutex);
    if (chainIdx < 0 || chainIdx >= workingBoardState.numNodes) return;
    auto& bus = workingBoardState.nodes[static_cast<size_t>(chainIdx)].bus;
    bus = static_cast<BusLane>((static_cast<uint8_t>(bus) + 1u) % 4u);
    publishBoardStateLocked();
}

void PedalBoardAudioProcessor::setDynamicChainNodeBus(int chainIdx, BusLane lane) {
    std::lock_guard<std::mutex> lock(boardStateMutex);
    if (chainIdx < 0 || chainIdx >= workingBoardState.numNodes) return;
    workingBoardState.nodes[static_cast<size_t>(chainIdx)].bus = lane;
    publishBoardStateLocked();
}

BusLane PedalBoardAudioProcessor::getPedalSlotBus(int slotId) const noexcept {
    const auto bs = getDynamicBoardState();
    for (int i = 0; i < bs.numNodes; ++i) {
        const auto& nd = bs.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot && nd.slotId == slotId) {
            return nd.bus;
        }
    }
    return BusLane::Pre;
}

void PedalBoardAudioProcessor::setPedalSlotBus(int slotId, BusLane lane) {
    std::lock_guard<std::mutex> lock(boardStateMutex);
    for (int i = 0; i < workingBoardState.numNodes; ++i) {
        auto& nd = workingBoardState.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot && nd.slotId == slotId) {
            nd.bus = lane;
            publishBoardStateLocked();
            return;
        }
    }
}

void PedalBoardAudioProcessor::movePedalSlotToOtherSlot(int draggedSlotId, int targetSlotId) {
    if (draggedSlotId == targetSlotId) return;
    std::lock_guard<std::mutex> lock(boardStateMutex);
    int fromIdx = -1;
    int toIdx = -1;
    for (int i = 0; i < workingBoardState.numNodes; ++i) {
        const auto& nd = workingBoardState.nodes[static_cast<size_t>(i)];
        if (nd.kind == ChainNodeKind::PedalSlot) {
            if (nd.slotId == draggedSlotId) fromIdx = i;
            if (nd.slotId == targetSlotId)  toIdx = i;
        }
    }
    if (fromIdx < 0 || toIdx < 0 || fromIdx == toIdx) return;

    const ChainNode moved = workingBoardState.nodes[static_cast<size_t>(fromIdx)];
    if (fromIdx < toIdx) {
        for (int i = fromIdx; i < toIdx; ++i) {
            workingBoardState.nodes[static_cast<size_t>(i)] = workingBoardState.nodes[static_cast<size_t>(i + 1)];
        }
    } else {
        for (int i = fromIdx; i > toIdx; --i) {
            workingBoardState.nodes[static_cast<size_t>(i)] = workingBoardState.nodes[static_cast<size_t>(i - 1)];
        }
    }
    workingBoardState.nodes[static_cast<size_t>(toIdx)] = moved;
    publishBoardStateLocked();
}

void PedalBoardAudioProcessor::setExpressionPedalCc(int ccNumber) {
    expressionPedalCc.store(std::clamp(ccNumber, 0, 127), std::memory_order_release);
    expressionLearnArmed.store(false, std::memory_order_release);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setExpressionPedalPosition(float normPos) {
    const float clamped = std::clamp(normPos, 0.0f, 1.0f);
    bool anyUpdated = false;
    {
        std::lock_guard<std::mutex> lock(boardStateMutex);
        for (int i = 0; i < kMaxPedalSlots; ++i) {
            auto& sl = workingBoardState.slots[static_cast<size_t>(i)];
            if (sl.occupied && isExpressionPedalSlotModel(sl.category, sl.modelIndex)) {
                sl.params[0] = clamped;
                anyUpdated = true;
            }
        }
        if (anyUpdated) {
            publishBoardStateLocked();
        }
    }
    if (auto* p = apvts.getParameter(params::kWahPedal)) {
        p->setValueNotifyingHost(clamped);
    }
}

juce::File PedalBoardAudioProcessor::getStudioSettingsFile() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("PedalBoard");
    if (!dir.exists()) {
        dir.createDirectory();
    }
    return dir.getChildFile("studio_settings.json");
}

void PedalBoardAudioProcessor::loadStudioSettings() {
    const auto file = getStudioSettingsFile();
    if (!file.existsAsFile()) {
        signalChain.setNamLoudnessNormalization(true);
        signalChain.getTuner().setReferenceHz(440.0f);
        return;
    }
    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (auto* obj = parsed.getDynamicObject()) {
        if (obj->hasProperty("inputChannelMode")) {
            inputChannelMode.store(std::clamp(static_cast<int>(obj->getProperty("inputChannelMode")), 0, 3), std::memory_order_relaxed);
        }
        if (obj->hasProperty("inputPhaseInvert")) {
            inputPhaseInvert.store(static_cast<bool>(obj->getProperty("inputPhaseInvert")), std::memory_order_relaxed);
        }
        if (obj->hasProperty("inputRumbleHpfHz")) {
            const float hz = static_cast<float>(static_cast<double>(obj->getProperty("inputRumbleHpfHz")));
            inputRumbleHpfHz.store(hz <= 1.0f ? 0.0f : std::clamp(hz, 15.0f, 160.0f), std::memory_order_relaxed);
        }
        if (obj->hasProperty("outputSafetyLimiter")) {
            outputSafetyLimiter.store(static_cast<bool>(obj->getProperty("outputSafetyLimiter")), std::memory_order_relaxed);
        }
        if (obj->hasProperty("namLoudnessNorm")) {
            const bool en = static_cast<bool>(obj->getProperty("namLoudnessNorm"));
            namLoudnessNorm.store(en, std::memory_order_relaxed);
            signalChain.setNamLoudnessNormalization(en);
        }
        if (obj->hasProperty("tunerReferenceHz")) {
            const float hz = static_cast<float>(static_cast<double>(obj->getProperty("tunerReferenceHz")));
            signalChain.getTuner().setReferenceHz(hz);
        }
        if (obj->hasProperty("midiProgramChangeEnabled")) {
            midiProgramChangeEnabled.store(static_cast<bool>(obj->getProperty("midiProgramChangeEnabled")), std::memory_order_relaxed);
        }
        if (obj->hasProperty("expressionPedalCc")) {
            expressionPedalCc.store(std::clamp(static_cast<int>(obj->getProperty("expressionPedalCc")), 0, 127), std::memory_order_release);
        }
        if (obj->hasProperty("manualBpm")) {
            const float bpm = std::clamp(static_cast<float>(static_cast<double>(obj->getProperty("manualBpm"))), 40.0f, 240.0f);
            manualBpm.store(bpm, std::memory_order_relaxed);
            effectiveBpm.store(bpm, std::memory_order_relaxed);
        }
    }
}

void PedalBoardAudioProcessor::saveStudioSettings() const {
    auto* obj = new juce::DynamicObject();
    obj->setProperty("inputChannelMode", static_cast<int>(getInputChannelMode()));
    obj->setProperty("inputPhaseInvert", isInputPhaseInvert());
    obj->setProperty("inputRumbleHpfHz", static_cast<double>(getInputRumbleHpfHz()));
    obj->setProperty("outputSafetyLimiter", isOutputSafetyLimiter());
    obj->setProperty("namLoudnessNorm", isNamLoudnessNormalization());
    obj->setProperty("tunerReferenceHz", static_cast<double>(getTunerReferenceHz()));
    obj->setProperty("midiProgramChangeEnabled", isMidiProgramChangeEnabled());
    obj->setProperty("expressionPedalCc", getExpressionPedalCc());
    obj->setProperty("manualBpm", static_cast<double>(getManualBpm()));
    getStudioSettingsFile().replaceWithText(juce::JSON::toString(juce::var(obj), true));
}

void PedalBoardAudioProcessor::setInputChannelMode(InputChannelMode mode) {
    inputChannelMode.store(std::clamp(static_cast<int>(mode), 0, 3), std::memory_order_relaxed);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setInputPhaseInvert(bool invert) {
    inputPhaseInvert.store(invert, std::memory_order_relaxed);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setInputRumbleHpfHz(float hz) {
    const float clamped = (hz <= 1.0f) ? 0.0f : std::clamp(hz, 15.0f, 160.0f);
    inputRumbleHpfHz.store(clamped, std::memory_order_relaxed);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setOutputSafetyLimiter(bool enabled) {
    outputSafetyLimiter.store(enabled, std::memory_order_relaxed);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setNamLoudnessNormalization(bool enabled) {
    namLoudnessNorm.store(enabled, std::memory_order_relaxed);
    signalChain.setNamLoudnessNormalization(enabled);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setTunerReferenceHz(float hz) {
    signalChain.getTuner().setReferenceHz(hz);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setMidiProgramChangeEnabled(bool enabled) {
    midiProgramChangeEnabled.store(enabled, std::memory_order_relaxed);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::setManualBpm(float bpm) {
    const float clamped = std::clamp(std::round(bpm * 2.0f) * 0.5f, 40.0f, 240.0f);
    manualBpm.store(clamped, std::memory_order_relaxed);
    if (!hostTempoSynced.load(std::memory_order_relaxed)) {
        effectiveBpm.store(clamped, std::memory_order_relaxed);
    }
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::triggerTapTempo() {
    const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    lastTapTempoMs.store(static_cast<int64_t>(nowSec * 1000.0), std::memory_order_relaxed);

    if (lastTapTimeSec > 0.0) {
        const double delta = nowSec - lastTapTimeSec;
        if (delta > 2.2 || delta < 0.18) {
            tapCount = 0;
        } else {
            if (tapCount < static_cast<int>(tapIntervalsSec.size())) {
                tapIntervalsSec[static_cast<size_t>(tapCount++)] = delta;
            } else {
                for (size_t i = 0; i + 1 < tapIntervalsSec.size(); ++i) {
                    tapIntervalsSec[i] = tapIntervalsSec[i + 1];
                }
                tapIntervalsSec.back() = delta;
            }
            double sum = 0.0;
            for (int i = 0; i < tapCount; ++i) {
                sum += tapIntervalsSec[static_cast<size_t>(i)];
            }
            const double avgSec = sum / static_cast<double>(tapCount);
            if (avgSec > 0.01) {
                setManualBpm(static_cast<float>(60.0 / avgSec));
            }
        }
    }
    lastTapTimeSec = nowSec;
}

MidiActivitySnapshot PedalBoardAudioProcessor::getLastMidiActivity() const noexcept {
    MidiActivitySnapshot snap;
    snap.timestampMs = lastMidiTimestampMs.load(std::memory_order_acquire);
    snap.type        = lastMidiType.load(std::memory_order_relaxed);
    snap.channel     = lastMidiChannel.load(std::memory_order_relaxed);
    snap.data1       = lastMidiData1.load(std::memory_order_relaxed);
    snap.data2       = lastMidiData2.load(std::memory_order_relaxed);
    return snap;
}

void PedalBoardAudioProcessor::resetStudioSettingsToDefaults() {
    inputChannelMode.store(static_cast<int>(InputChannelMode::Stereo), std::memory_order_relaxed);
    inputPhaseInvert.store(false, std::memory_order_relaxed);
    inputRumbleHpfHz.store(0.0f, std::memory_order_relaxed);
    outputSafetyLimiter.store(true, std::memory_order_relaxed);
    namLoudnessNorm.store(true, std::memory_order_relaxed);
    signalChain.setNamLoudnessNormalization(true);
    signalChain.getTuner().setReferenceHz(440.0f);
    midiProgramChangeEnabled.store(true, std::memory_order_relaxed);
    manualBpm.store(120.0f, std::memory_order_relaxed);
    if (!hostTempoSynced.load(std::memory_order_relaxed)) {
        effectiveBpm.store(120.0f, std::memory_order_relaxed);
    }
    expressionPedalCc.store(11, std::memory_order_release);
    expressionLearnArmed.store(false, std::memory_order_release);
    saveStudioSettings();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::applyPresetData(const PresetData& preset) {
    PresetManager::applyPresetToProcessor(preset, apvts);
    signalChain.setChainOrder(preset.chainOrder);
    signalChain.setBusAssignments(preset.busAssignments);

    if (preset.customIrPath.isNotEmpty() && juce::File(preset.customIrPath).existsAsFile()) {
        signalChain.getCabinet(0).loadCustomIR(juce::File(preset.customIrPath));
    } else {
        signalChain.getCabinet(0).loadBuiltInSyntheticCab();
    }

    if (preset.customIrBPath.isNotEmpty() && juce::File(preset.customIrBPath).existsAsFile()) {
        signalChain.getCabinet(1).loadCustomIR(juce::File(preset.customIrBPath));
    } else {
        signalChain.getCabinet(1).loadBuiltInSyntheticCab();
    }

    if (preset.customIrCPath.isNotEmpty() && juce::File(preset.customIrCPath).existsAsFile()) {
        signalChain.getCabinet(2).loadCustomIR(juce::File(preset.customIrCPath));
    } else {
        signalChain.getCabinet(2).loadBuiltInSyntheticCab();
    }

    if (preset.customNamPath.isNotEmpty() && juce::File(preset.customNamPath).existsAsFile()) {
        loadCustomNamModel(juce::File(preset.customNamPath), 0);
    }
    if (preset.customNamBPath.isNotEmpty() && juce::File(preset.customNamBPath).existsAsFile()) {
        loadCustomNamModel(juce::File(preset.customNamBPath), 1);
    }
    if (preset.customNamCPath.isNotEmpty() && juce::File(preset.customNamCPath).existsAsFile()) {
        loadCustomNamModel(juce::File(preset.customNamCPath), 2);
    }

    if (preset.midiMappings.isArray()) {
        midiLearnManager.loadFromJsonArray(preset.midiMappings);
    }

    if (preset.isFactory && preset.factoryIndex >= 0) {
        currentProgramIndex.store(preset.factoryIndex, std::memory_order_relaxed);
    }

    {
        std::lock_guard<std::mutex> lock(presetNameMutex);
        activePresetName = preset.name;
    }
    syncDynamicBoardFromApvts(preset.clearStompboard);
    notifyUiStateChanged();
}

bool PedalBoardAudioProcessor::loadUserPreset(const juce::File& file) {
    PresetData preset;
    if (!presetManager.loadPresetFromFile(file, preset)) {
        return false;
    }
    applyPresetData(preset);
    return true;
}

bool PedalBoardAudioProcessor::saveUserPreset(const juce::File& file,
                                              const juce::String& presetName,
                                              const juce::String& style,
                                              const juce::String& character,
                                              const juce::String& description) {
    const bool ok = presetManager.savePresetToFile(
        file,
        presetName,
        style,
        character,
        description,
        apvts,
        signalChain.getChainOrder(),
        signalChain.getBusAssignments(),
        signalChain.getCabinet(0).getCustomIrPath(),
        signalChain.getCabinet(1).getCustomIrPath(),
        signalChain.getAmpHead(0).getNamRunner().getModelPath(),
        signalChain.getAmpHead(1).getNamRunner().getModelPath(),
        midiLearnManager,
        signalChain.getCabinet(2).getCustomIrPath(),
        signalChain.getAmpHead(2).getNamRunner().getModelPath()
    );
    if (ok) {
        {
            std::lock_guard<std::mutex> lock(presetNameMutex);
            activePresetName = presetName.isNotEmpty() ? presetName : file.getFileNameWithoutExtension();
        }
        notifyUiStateChanged();
    }
    return ok;
}

bool PedalBoardAudioProcessor::loadCustomCabinetIR(const juce::File& file, int rigIndex) {
    const bool ok = signalChain.getCabinet(rigIndex).loadCustomIR(file);
    if (ok) {
        const char* modParamId = params::modelParamFor(ModuleId::CabinetIR, rigIndex);
        if (auto* p = apvts.getParameter(modParamId)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(70.0f)); // Auto-switch to User IR slot (70)
            p->endChangeGesture();
        }
        notifyUiStateChanged();
    }
    return ok;
}

void PedalBoardAudioProcessor::activateSavedCabinetIR(int rigIndex) {
    signalChain.getCabinet(rigIndex).reloadSavedCustomIR();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::resetCabinetIR(int rigIndex) {
    signalChain.getCabinet(rigIndex).clearCustomIR();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::loadCustomNamModel(const juce::File& file, int rigIndex) {
    const char* chParamId  = (rigIndex == 2) ? params::kAmpCChannel : ((rigIndex == 1) ? params::kAmpBChannel : params::kAmpChannel);
    const char* modParamId = (rigIndex == 2) ? params::kAmpCModel   : ((rigIndex == 1) ? params::kAmpBModel   : params::kAmpModel);
    signalChain.getAmpHead(rigIndex).getNamRunner().loadModelAsync(
        file,
        [safeThis = juce::WeakReference<PedalBoardAudioProcessor>(this), chParamId, modParamId](bool ok) {
            if (safeThis == nullptr) return;
            if (ok) {
                if (auto* chParam = safeThis->apvts.getParameter(chParamId)) {
                    chParam->beginChangeGesture();
                    chParam->setValueNotifyingHost(chParam->convertTo0to1(3.0f));
                    chParam->endChangeGesture();
                }
                if (auto* modParam = safeThis->apvts.getParameter(modParamId)) {
                    modParam->beginChangeGesture();
                    modParam->setValueNotifyingHost(modParam->convertTo0to1(71.0f));
                    modParam->endChangeGesture();
                }
            }
            safeThis->notifyUiStateChanged();
        }
    );
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::clearNamModel(int rigIndex) {
    signalChain.getAmpHead(rigIndex).getNamRunner().clearModel();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::loadCustomSlotNamModel(int slotId, const juce::File& file) {
    if (slotId < 0 || slotId >= kMaxPedalSlots) return;
    signalChain.getSlotNamRunner(slotId).loadModelAsync(
        file,
        [safeThis = juce::WeakReference<PedalBoardAudioProcessor>(this)](bool) {
            if (safeThis != nullptr) {
                safeThis->notifyUiStateChanged();
            }
        }
    );
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::clearSlotNamModel(int slotId) {
    if (slotId < 0 || slotId >= kMaxPedalSlots) return;
    signalChain.getSlotNamRunner(slotId).clearModel();
    notifyUiStateChanged();
}

void PedalBoardAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree root("PedalBoardState");
    root.setProperty("stateSchemaVersion", 2, nullptr);
    root.addChild(apvts.copyState(), -1, nullptr);

    juce::StringArray chainKeys;
    for (const auto mod : signalChain.getChainOrder()) {
        chainKeys.add(moduleKey(mod));
    }
    root.setProperty("chainOrder", chainKeys.joinIntoString(","), nullptr);

    juce::StringArray busKeys;
    const auto buses = signalChain.getBusAssignments();
    for (int i = 0; i < kNumModules; ++i) {
        busKeys.add(busLaneKey(buses[static_cast<size_t>(i)]));
    }
    root.setProperty("busAssignments", busKeys.joinIntoString(","), nullptr);

    root.setProperty("activePresetName", getActivePresetName(), nullptr);
    root.setProperty("customIrPath", signalChain.getCabinet(0).getCustomIrPath(), nullptr);
    root.setProperty("customIrBPath", signalChain.getCabinet(1).getCustomIrPath(), nullptr);
    root.setProperty("customIrCPath", signalChain.getCabinet(2).getCustomIrPath(), nullptr);
    root.setProperty("customNamPath", signalChain.getAmpHead(0).getNamRunner().getModelPath(), nullptr);
    root.setProperty("customNamBPath", signalChain.getAmpHead(1).getNamRunner().getModelPath(), nullptr);
    root.setProperty("customNamCPath", signalChain.getAmpHead(2).getNamRunner().getModelPath(), nullptr);
    root.setProperty("midiMappingsJson", juce::JSON::toString(midiLearnManager.toJsonArray(), false), nullptr);

    // Serialize exact dynamic board state (0..8 stomp slots, node order, and 3 rack slots)
    const auto bs = getDynamicBoardState();
    juce::ValueTree dynTree("DynamicBoard");
    for (int i = 0; i < bs.numNodes; ++i) {
        juce::ValueTree n("Node");
        n.setProperty("kind", static_cast<int>(bs.nodes[static_cast<size_t>(i)].kind), nullptr);
        n.setProperty("slotId", bs.nodes[static_cast<size_t>(i)].slotId, nullptr);
        n.setProperty("bus", static_cast<int>(bs.nodes[static_cast<size_t>(i)].bus), nullptr);
        dynTree.addChild(n, -1, nullptr);
    }
    auto writeSlot = [](const juce::Identifier& tag, int idx, const PedalSlotSnapshot& sl) {
        juce::ValueTree t(tag);
        t.setProperty("index", idx, nullptr);
        t.setProperty("occupied", sl.occupied, nullptr);
        t.setProperty("bypassed", sl.bypassed, nullptr);
        t.setProperty("category", static_cast<int>(sl.category), nullptr);
        t.setProperty("modelIndex", sl.modelIndex, nullptr);
        juce::StringArray vals;
        for (int p = 0; p < kMaxSlotParams; ++p) {
            vals.add(juce::String(sl.params[static_cast<size_t>(p)], 5));
        }
        t.setProperty("params", vals.joinIntoString(","), nullptr);
        return t;
    };
    for (int i = 0; i < kMaxPedalSlots; ++i) {
        if (bs.slots[static_cast<size_t>(i)].occupied) {
            auto slotTree = writeSlot("PedalSlot", i, bs.slots[static_cast<size_t>(i)]);
            if (isNamPedalSlotModel(bs.slots[static_cast<size_t>(i)].category, bs.slots[static_cast<size_t>(i)].modelIndex)) {
                const auto sp = signalChain.getSlotNamRunner(i).getModelPath();
                if (sp.isNotEmpty()) {
                    slotTree.setProperty("namPath", sp, nullptr);
                }
            }
            dynTree.addChild(slotTree, -1, nullptr);
        }
    }
    for (int i = 0; i < 3; ++i) {
        dynTree.addChild(writeSlot("RackSlot", i, bs.rackSlots[static_cast<size_t>(i)]), -1, nullptr);
    }
    root.addChild(dynTree, -1, nullptr);

    if (auto xml = root.createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

void PedalBoardAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr) {
        return;
    }

    const auto root = juce::ValueTree::fromXml(*xmlState);
    if (!root.isValid()) {
        return;
    }

    // In Standalone, skip obsolete v1 auto-saved filterState so updated app launches cleanly to Clean Init!
    const int schemaVer = static_cast<int>(root.getProperty("stateSchemaVersion", 0));
    if (wrapperType == wrapperType_Standalone && schemaVer < 2) {
        return;
    }

    if (const auto paramsTree = root.getChildWithName(apvts.state.getType()); paramsTree.isValid()) {
        apvts.replaceState(paramsTree);
    }

    const auto orderStr = root.getProperty("chainOrder").toString();
    if (orderStr.isNotEmpty()) {
        const auto tokens = juce::StringArray::fromTokens(orderStr, ",", "");
        if (tokens.size() == kNumModules) {
            ChainOrder parsed {};
            std::array<bool, kNumModules> seen {};
            bool ok = true;
            for (int i = 0; i < kNumModules; ++i) {
                ModuleId id {};
                if (!moduleFromKey(tokens[i].trim(), id) || seen[static_cast<size_t>(id)]) {
                    ok = false;
                    break;
                }
                seen[static_cast<size_t>(id)] = true;
                parsed[static_cast<size_t>(i)] = id;
            }
            if (ok) {
                signalChain.setChainOrder(parsed);
            }
        }
    }

    const auto busStr = root.getProperty("busAssignments").toString();
    if (busStr.isNotEmpty()) {
        const auto tokens = juce::StringArray::fromTokens(busStr, ",", "");
        if (tokens.size() == kNumModules) {
            BusAssignments parsed {};
            for (int i = 0; i < kNumModules; ++i) {
                parsed[static_cast<size_t>(i)] = busLaneFromKey(tokens[i].trim());
            }
            signalChain.setBusAssignments(parsed);
        }
    }

    const auto savedName = root.getProperty("activePresetName").toString();
    if (savedName.isNotEmpty()) {
        std::lock_guard<std::mutex> lock(presetNameMutex);
        activePresetName = savedName;
    }

    const auto irPath = root.getProperty("customIrPath").toString();
    if (irPath.isNotEmpty() && juce::File(irPath).existsAsFile()) {
        signalChain.getCabinet(0).loadCustomIR(juce::File(irPath));
    }

    const auto irBPath = root.getProperty("customIrBPath").toString();
    if (irBPath.isNotEmpty() && juce::File(irBPath).existsAsFile()) {
        signalChain.getCabinet(1).loadCustomIR(juce::File(irBPath));
    }

    const auto irCPath = root.getProperty("customIrCPath").toString();
    if (irCPath.isNotEmpty() && juce::File(irCPath).existsAsFile()) {
        signalChain.getCabinet(2).loadCustomIR(juce::File(irCPath));
    }

    const auto namPath = root.getProperty("customNamPath").toString();
    if (namPath.isNotEmpty() && juce::File(namPath).existsAsFile()) {
        signalChain.getAmpHead(0).getNamRunner().loadModelAsync(juce::File(namPath));
    }

    const auto namBPath = root.getProperty("customNamBPath").toString();
    if (namBPath.isNotEmpty() && juce::File(namBPath).existsAsFile()) {
        signalChain.getAmpHead(1).getNamRunner().loadModelAsync(juce::File(namBPath));
    }

    const auto namCPath = root.getProperty("customNamCPath").toString();
    if (namCPath.isNotEmpty() && juce::File(namCPath).existsAsFile()) {
        signalChain.getAmpHead(2).getNamRunner().loadModelAsync(juce::File(namCPath));
    }

    const auto midiJson = root.getProperty("midiMappingsJson").toString();
    if (midiJson.isNotEmpty()) {
        midiLearnManager.loadFromJsonArray(juce::JSON::parse(midiJson));
    }

    const auto dynTree = root.getChildWithName("DynamicBoard");
    if (dynTree.isValid()) {
        DynamicBoardState bs {};
        int nodeIdx = 0;
        for (int c = 0; c < dynTree.getNumChildren(); ++c) {
            const auto child = dynTree.getChild(c);
            const auto type = child.getType();
            if (type == juce::Identifier("Node") && nodeIdx < static_cast<int>(bs.nodes.size())) {
                ChainNode nd {};
                nd.kind   = static_cast<ChainNodeKind>(static_cast<int>(child.getProperty("kind", 0)));
                nd.slotId = static_cast<int>(child.getProperty("slotId", 0));
                nd.bus    = static_cast<BusLane>(static_cast<int>(child.getProperty("bus", 0)));
                bs.nodes[static_cast<size_t>(nodeIdx++)] = nd;
            } else if (type == juce::Identifier("PedalSlot") || type == juce::Identifier("RackSlot")) {
                const bool isRack = (type == juce::Identifier("RackSlot"));
                const int idx = static_cast<int>(child.getProperty("index", -1));
                if ((isRack && (idx < 0 || idx >= 3)) || (!isRack && (idx < 0 || idx >= kMaxPedalSlots))) continue;
                auto& dst = isRack ? bs.rackSlots[static_cast<size_t>(idx)] : bs.slots[static_cast<size_t>(idx)];
                dst.occupied   = static_cast<bool>(child.getProperty("occupied", true));
                dst.bypassed   = static_cast<bool>(child.getProperty("bypassed", false));
                dst.category   = static_cast<ModuleId>(static_cast<int>(child.getProperty("category", 0)));
                dst.modelIndex = static_cast<int>(child.getProperty("modelIndex", 0));
                dst.params     = defaultParamsForCategory(dst.category, dst.modelIndex);
                const auto pTokens = juce::StringArray::fromTokens(child.getProperty("params").toString(), ",", "");
                for (int p = 0; p < std::min(pTokens.size(), kMaxSlotParams); ++p) {
                    dst.params[static_cast<size_t>(p)] = pTokens[p].getFloatValue();
                }
                if (!isRack && isNamPedalSlotModel(dst.category, dst.modelIndex)) {
                    const auto slotNamPath = child.getProperty("namPath").toString();
                    if (slotNamPath.isNotEmpty() && juce::File(slotNamPath).existsAsFile()) {
                        signalChain.getSlotNamRunner(idx).loadModelAsync(juce::File(slotNamPath));
                    }
                }
            }
        }
        if (nodeIdx > 0) {
            std::lock_guard<std::mutex> lock(boardStateMutex);
            bs.numNodes = nodeIdx;
            workingBoardState = bs;
            publishBoardStateLocked();
        } else {
            syncDynamicBoardFromApvts(true);
        }
    } else {
        syncDynamicBoardFromApvts(savedName == "Clean Init");
    }
    notifyUiStateChanged();
}

juce::AudioProcessorEditor* PedalBoardAudioProcessor::createEditor() {
    return new PedalBoardAudioProcessorEditor(*this);
}

} // namespace pedalboard

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new pedalboard::PedalBoardAudioProcessor();
}
