/*
  ==============================================================================

    S3000XL-VST - S3000XL MAME VST Emulation
    Open Source GPLv2/v3
    https://www.sojusrecords.com

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "mpc_clickmap.h"
#include "BinaryData.h" // Small icons generated locally; original MAME GUI remains

EnsoniqSD1AudioProcessorEditor::EnsoniqSD1AudioProcessorEditor (EnsoniqSD1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), presetBrowser (p)
{
    // Start the UI polling timer (30 Hz is enough for smooth UI and MAME frame fetching)
    startTimerHz(30);
  
    // PRESET MANAGER
    addChildComponent(presetBrowser);
    // PANEL RESET FOR FILE MANAGER
    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) {
        if (audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire)
            || !audioProcessor.isMameRunningFlag()) {
            // ROM not found — reset to normal view so ROM locator is shown
            audioProcessor.requestedViewIndex.store(0, std::memory_order_release);
        } else {
            presetBrowser.setVisible(true);
            // SafePointer for timer
            juce::Component::SafePointer<EnsoniqSD1AudioProcessorEditor> safeThis(this);
            juce::Timer::callAfterDelay(50, [safeThis]() {
                if (safeThis != nullptr) safeThis->presetBrowser.toFront(false);
            });
        }
    }

    presetBrowser.onClose = [this] {
            if (presetBrowser.isSYXProcessing) return;

            // Cancel any pending MIDI timers
            audioProcessor.midiOpCancelled.store(true, std::memory_order_release);
            audioProcessor.suppressMidiInput.store(false, std::memory_order_release);
            audioProcessor.clearMidiBuffer();
            
            juce::Thread::sleep(20);
            
            juce::Timer::callAfterDelay(200, [this]() {
                audioProcessor.midiOpCancelled.store(false, std::memory_order_release);
            });
                    
            presetBrowser.showClosingProgress("Returning to Synth...", 1400.0);
            juce::Component::SafePointer<EnsoniqSD1AudioProcessorEditor> safeThis(this);
                    
            // --- 1. READ MEMORY AND VFD IMMEDIATELY (No Delay!) ---
            if (safeThis->audioProcessor.mameMachine != nullptr) {
                bool isCompareOnMem = false;
                auto* osram_share = safeThis->audioProcessor.mameMachine->root_device().memshare("osram");
                if (osram_share != nullptr && osram_share->bytes() > 0x92DC) {
                    uint8_t* osram = static_cast<uint8_t*>(osram_share->ptr());
                    isCompareOnMem = (osram[0x92DC] != 0x00);
                }

                juce::String vfd = safeThis->audioProcessor.getHardwareVfdText();
                bool isWriting = vfd.containsIgnoreCase("WRITE EDIT");
                bool hasError = vfd.containsIgnoreCase("ERROR");

                double now = safeThis->audioProcessor.mameMachine->time().as_double();
                double safeTime = now + 0.05;
                
                safeThis->audioProcessor.suppressMidiInput.store(true, std::memory_order_release);
                safeThis->audioProcessor.pushMidiByte(0xF7, safeTime);
                safeTime += 0.05;
                
                auto pushBtn = [&](int btn, double downT) {
                    uint8_t d[11] = { 0xF0, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)((btn>>4)&0x0F), (uint8_t)(btn&0x0F), 0xF7 };
                    for (int i=0; i<11; ++i) safeThis->audioProcessor.pushMidiByte(d[i], downT + i*0.00035);
                    int up = btn + 96;
                    double upT = downT + 0.15; // 150ms hold
                    uint8_t u[11] = { 0xF0, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)((up>>4)&0x0F), (uint8_t)(up&0x0F), 0xF7 };
                    for (int i=0; i<11; ++i) safeThis->audioProcessor.pushMidiByte(u[i], upT + i*0.00035);
                };

                // Return to Sounds mode ONLY if a preset was previewed
                if (safeThis->presetBrowser.wasPresetPreviewed) {
                    if (auto* p = safeThis->audioProcessor.apvts.getParameter("btn_sounds")) {
                        p->setValueNotifyingHost(1.0f);
                        juce::Timer::callAfterDelay(50, [p]{ p->setValueNotifyingHost(0.0f); });
                    }
                }
                
                // Escape sub-menus
                if (safeThis->presetBrowser.wasSYXPreview || safeThis->presetBrowser.isErrorState || isWriting || hasError) {
                    pushBtn(19, safeTime); // EXIT
                    safeTime += 0.40;
                }
                
                // Escape Compare explicitly based on the instant read
                if (isCompareOnMem) {
                    pushBtn(63, safeTime); // COMPARE OFF
                    safeTime += 0.40;
                }
                
                // Force Sounds
                pushBtn(11, safeTime);
                
                // Clear flags
                safeThis->presetBrowser.wasSYXPreview = false;
                safeThis->presetBrowser.wasPresetPreviewed = false;
                safeThis->presetBrowser.wasWriteSinglePreset = false;
                safeThis->presetBrowser.wasSYXImport = false;
                safeThis->presetBrowser.wasSEQImport = false;
                safeThis->presetBrowser.isSYXProcessing = false;
                safeThis->presetBrowser.isErrorState = false;
                
                safeThis->audioProcessor.suppressMidiInput.store(false, std::memory_order_release);
                        
                // --- RESTORE UI DELAYED UNTIL MACHINE SETTLES ---
                juce::Timer::callAfterDelay(300, [safeThis]() {
                    if (safeThis == nullptr) return;
                    
                    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(safeThis->audioProcessor.apvts.getParameter("layout_view"))) {
                        safeThis->audioProcessor.requestedViewIndex.store(choice->getIndex(), std::memory_order_release);
                    } else {
                        safeThis->audioProcessor.requestedViewIndex.store(0, std::memory_order_release);
                    }
                    safeThis->audioProcessor.requestViewChange.store(true, std::memory_order_release);
                            
                    safeThis->presetBrowser.saveStateToProcessor();
                    safeThis->audioProcessor.fileManagerState.visible = false;
                    
                    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(safeThis->audioProcessor.apvts.getParameter("layout_view"))) {
                        safeThis->audioProcessor.fileManagerState.viewBeforeBrowser = choice->getIndex();
                    }
                    
                    safeThis->presetBrowser.setVisible(false);
                    safeThis->updateWindowSize();
                    safeThis->repaint();
                });
            }
        };
    
    this->setWantsKeyboardFocus(false);
    this->setMouseClickGrabsKeyboardFocus(false);
    
    // --- BASE LAYOUT DIMENSIONS ---
    int viewIdx = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    lastView = viewIdx;

    int baseW = 2048;
    int baseH = 1744; // Default Compact
    if (viewIdx == 1) baseH = 671;
    else if (viewIdx == 2) baseH = 379;
    else if (viewIdx == 3) baseH = 1476;
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    else if (viewIdx == 4) baseH = 925 + 379;
#endif // SD1_DEBUG_RACK_PANEL

    float aspect = MPC_VIEW_W / MPC_VIEW_H;

    // --- WINDOW SETTINGS & RESIZER ---
    // --- Correctly restore window dimensions for File Manager view ---
        int startW = 1200;
        if (viewIdx == 4) {
            startW = audioProcessor.fileManagerState.fmWindowWidth >= 900 ? audioProcessor.fileManagerState.fmWindowWidth : 1200;
        } else {
            startW = audioProcessor.savedWindowWidth >= 900 ? audioProcessor.savedWindowWidth : 1200;
        }
        int startH = juce::roundToInt(startW / aspect);

    setResizable(true, true);
   
    getConstrainer()->setFixedAspectRatio(aspect);
    setResizeLimits(900, juce::roundToInt(900.0f / aspect), 2400, juce::roundToInt(2400.0f / aspect));
    setSize(startW, startH);
    
    // --- BUTTON INITIALIZATION ---
    addAndMakeVisible(loadMediaButton);
    loadMediaButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    loadMediaButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    loadMediaButton.setWantsKeyboardFocus(false);
    loadMediaButton.setMouseClickGrabsKeyboardFocus(false);
    loadMediaButton.onClick = [this] { loadMediaButtonClicked(); };
    floppyIcon = juce::Drawable::createFromImageData(BinaryData::floppy_svg, BinaryData::floppy_svgSize);
    cartIcon = juce::Drawable::createFromImageData(BinaryData::cart_svg, BinaryData::cart_svgSize);
    
    // Hide UI elements if the plugin is in a disabled state
    if (audioProcessor.isRomMissing.load(std::memory_order_acquire) ||
        audioProcessor.isRomInvalid.load(std::memory_order_acquire) ||
        audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire) ||
        audioProcessor.isUnsupportedAUHost.load(std::memory_order_acquire)) {
        
        loadMediaButton.setVisible(false);
        settingsButton.setVisible(false);
    }
    
    // --- ROM BUTTONS ---
    addAndMakeVisible(locateRomButton);
    locateRomButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0055aa));
    locateRomButton.onClick = [this] { locateRomButtonClicked(); };

    addAndMakeVisible(rescanRomButton);
    rescanRomButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffaa5500));
    rescanRomButton.onClick = [this] { audioProcessor.checkRomAndBootMame(); repaint(); };
    
    // SAVE MACRO
    saveBadge.setButtonText(""); // Text and icon will be drawn natively in paint()
    
    // Make the button completely transparent
    saveBadge.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    saveBadge.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    saveBadge.setWantsKeyboardFocus(false);
    saveBadge.setMouseClickGrabsKeyboardFocus(false);

    saveBadge.onClick = [this]() {
        if (audioProcessor.mameMachine == nullptr) return;
        
        // If already active, cancel
        if (audioProcessor.isSaveMacroActive.load(std::memory_order_acquire)) {
            audioProcessor.macroBankToHold.store(-1, std::memory_order_release);  // release hold (audio thread)
            audioProcessor.saveMacroHeldBank = -1;
            audioProcessor.isSaveMacroActive.store(false, std::memory_order_release);
            savePromptLabel.setVisible(false);
            return;
        }
        
        // Check if VFD already shows WRITE EDIT (user pressed WRITE manually)
        juce::String vfd = audioProcessor.getHardwareVfdText();
        if (!vfd.containsIgnoreCase("WRITE EDIT")) {
            // Press WRITE button momentarily (150ms)
            if (auto* p = audioProcessor.apvts.getParameter("btn_write")) {
                p->setValueNotifyingHost(1.0f);
                juce::Timer::callAfterDelay(150, [this]() {
                    if (auto* p2 = audioProcessor.apvts.getParameter("btn_write"))
                        p2->setValueNotifyingHost(0.0f);
                });
            }
        }
        
        // Phase 1: waiting for bank button click on the MAME panel
        audioProcessor.saveMacroHeldBank = -1;
        audioProcessor.macroBankToHold.store(-1, std::memory_order_release);
        audioProcessor.detectedBankMask.store(0, std::memory_order_release);
        audioProcessor.isSaveMacroActive.store(true, std::memory_order_release);
        savePromptLabel.setText("Select a BANK button on the panel...", juce::dontSendNotification);
        savePromptLabel.setVisible(true);
    };
    // S3000XL: Save Program button removed (overlapped the LCD; unused on MPC).

    // Set up the prompt label
    savePromptLabel.setJustificationType(juce::Justification::centred);
    savePromptLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    savePromptLabel.setVisible(false); // Hidden by default
    addAndMakeVisible(savePromptLabel);
    
    // --- SETTINGS BUTTON & PANEL INITIALIZATION ---
    addAndMakeVisible(settingsButton);
    settingsButton.setWantsKeyboardFocus(false);
    settingsButton.setMouseClickGrabsKeyboardFocus(false);
    settingsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    settingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    settingsButton.onClick = [this] { toggleSettings(); };

    // GroupComponent and its children are hidden by default
    addChildComponent(settingsGroup);
    settingsGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::orange);
    settingsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::grey);

    addChildComponent(bufferLabel);
    bufferLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    bufferLabel.setJustificationType(juce::Justification::centredRight);

    addChildComponent(bufferCombo);
    bufferCombo.addItemList({ "128", "256", "512", "1024", "2048", "4096", "8192" }, 1);
    bufferAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "buffer_size", bufferCombo);

    addChildComponent(viewLabel);
    viewLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    viewLabel.setJustificationType(juce::Justification::centredRight);

    addChildComponent(viewCombo);
    viewCombo.addItemList({ "Compact (Default)", "Full Keyboard", "Rack Panel", "Tablet View" }, 1);
    viewAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "layout_view", viewCombo);
        
    addChildComponent(aboutLabel);
    aboutLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    aboutLabel.setJustificationType(juce::Justification::centred);

    addChildComponent(webLink);

    addChildComponent(closeSettingsButton);
    closeSettingsButton.setWantsKeyboardFocus(false);
    closeSettingsButton.setMouseClickGrabsKeyboardFocus(false);
    closeSettingsButton.onClick = [this] { toggleSettings(); };
        
    // Hide UI elements if the plugin is in a disabled state
    if (audioProcessor.isRomMissing.load(std::memory_order_acquire) ||
        audioProcessor.isRomInvalid.load(std::memory_order_acquire) ||
        audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire) ||
        audioProcessor.isUnsupportedAUHost.load(std::memory_order_acquire)) {
        
        loadMediaButton.setVisible(false);
        settingsButton.setVisible(false);
        saveBadge.setVisible(false);
        savePromptLabel.setVisible(false);
    }
    
    // --- RESTORE FILE MANAGER STATE ---
    if (audioProcessor.fileManagerState.visible) {
        if (audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire)
            || !audioProcessor.isMameRunningFlag()) {
            // ROM not found — do not restore file manager; ROM locator will be shown
            audioProcessor.fileManagerState.visible = false;
        } else {
            lastViewBeforeBrowser = audioProcessor.fileManagerState.viewBeforeBrowser;
            audioProcessor.requestedViewIndex.store(4, std::memory_order_release);
            presetBrowser.setVisible(true);
            presetBrowser.toFront(false);
            updateWindowSize();
            juce::Component::SafePointer<EnsoniqSD1AudioProcessorEditor> safeThis(this);
            juce::Timer::callAfterDelay(150, [safeThis]() {
                if (safeThis != nullptr) safeThis->presetBrowser.restoreStateFromProcessor();
            });
        }
    }
    
    // Logic for opening the window / new instance
    // Schedule reset 0.5s after GUI opens (or 3.5s if the MAME engine is still booting)
        double now = audioProcessor.mameMachine ? audioProcessor.mameMachine->time().as_double() : 0.0;
        audioProcessor.scheduledCompareResetTime.store(std::max(3.5, now + 0.5), std::memory_order_release);
        
}

EnsoniqSD1AudioProcessorEditor::~EnsoniqSD1AudioProcessorEditor()
{
    if (!audioProcessor.stateJustLoaded.exchange(false, std::memory_order_acq_rel)) {
        presetBrowser.saveStateToProcessor();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(audioProcessor.apvts.getParameter("layout_view"))) {
            audioProcessor.fileManagerState.viewBeforeBrowser = choice->getIndex();
        }
        saveGlobalSettings();
    }
    stopTimer();
}

void EnsoniqSD1AudioProcessorEditor::timerCallback()

    {
    // Standard offline protection
    if (audioProcessor.isNonRealtime()) {
        audioProcessor.getFrameFlag().store(false, std::memory_order_relaxed);
        return;
    }

    // Check isWarmBoot
    bool isWarm = audioProcessor.isWarmBoot.exchange(false, std::memory_order_acquire);

    // --- UNIFIED COMPARE RESET (Load State, Cold Boot & Window Reopen) ---
    if ((isWarm || !startupCompareChecked) && audioProcessor.isMameRunningFlag() && audioProcessor.mameMachine != nullptr) {
                                     
                // Check MAME internal time (secs)
                double mameTime = audioProcessor.mameMachine->time().as_double();
            
                // 1.Load State  (isWarm) -> 3500ms
                // 2. New Instance (mameTime < 5.0) -> 3500ms
                // 3. Window reopen (mameTime > 5.0) -> 500ms
                int delayMs = (isWarm || mameTime < 5.0) ? 3500 : 500;

                startupCompareChecked = true;
            
                // SafePointer for timer
                juce::Component::SafePointer<EnsoniqSD1AudioProcessorEditor> safeThis(this);
                juce::Timer::callAfterDelay(delayMs, [safeThis]() {
                    if (safeThis != nullptr) safeThis->audioProcessor.forceCompareOff();
                });

    }
    
    // SAVE PROGRAM MACRO — 2-phase via esqpanel detection + set_button hold
    if (audioProcessor.isSaveMacroActive.load(std::memory_order_acquire) && !isSettingsVisible) {
        int mask = audioProcessor.detectedBankMask.load(std::memory_order_acquire);
        
        if (audioProcessor.saveMacroHeldBank < 0) {
            // Phase 1: detect first pressed bank → start holding it
            for (int i = 0; i < 10; ++i) {
                if (mask & (1 << i)) {
                    audioProcessor.saveMacroHeldBank = i;
                    audioProcessor.macroBankToHold.store(i, std::memory_order_release);
                    savePromptLabel.setText("Bank " + juce::String(i) + " held. Click a SOFT button to save...", juce::dontSendNotification);
                    break;
                }
            }
        } else {
            // Phase 2: bank held. Allow switching to a different bank.
            for (int i = 0; i < 10; ++i) {
                if (i != audioProcessor.saveMacroHeldBank && (mask & (1 << i))) {
                    // User clicked a different bank → switch hold
                    audioProcessor.saveMacroHeldBank = i;
                    audioProcessor.macroBankToHold.store(i, std::memory_order_release);
                    savePromptLabel.setText("Bank " + juce::String(i) + " held. Click a SOFT button to save...", juce::dontSendNotification);
                    break;
                }
            }
            
            // Check VFD for "WRITING" → auto-complete
            juce::String vfd = audioProcessor.getHardwareVfdText();
            if (vfd.containsIgnoreCase("WRITING")) {
                audioProcessor.macroBankToHold.store(-1, std::memory_order_release);
                audioProcessor.saveMacroHeldBank = -1;
                audioProcessor.isSaveMacroActive.store(false, std::memory_order_release);
                savePromptLabel.setVisible(false);
            }
        }
        
        // Flash the prompt
        if (audioProcessor.isSaveMacroActive.load(std::memory_order_acquire)) {
            bool flashState = (juce::Time::getMillisecondCounter() % 1000) < 500;
            savePromptLabel.setVisible(flashState);
        }
    } else {
        savePromptLabel.setVisible(false);
    }
    
    // Global settings save triggered from background
    if (audioProcessor.requestGlobalSave.exchange(false, std::memory_order_acquire)) {
        saveGlobalSettings();
    }

    // --- RESTORE FILE MANAGER UI ON STATE LOAD ---
    if (audioProcessor.requestFileManagerUIRefresh.exchange(false, std::memory_order_acquire)) {
                if (audioProcessor.fileManagerState.visible
                    && !audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire)
                    && audioProcessor.isMameRunningFlag()) {
                    audioProcessor.requestedViewIndex.store(4, std::memory_order_release);
                    presetBrowser.setVisible(true);
                    presetBrowser.toFront(false);
                    updateWindowSize();
                    juce::Component::SafePointer<EnsoniqSD1AudioProcessorEditor> safeThis(this);
                    juce::Timer::callAfterDelay(100, [safeThis]() {
                        if (safeThis != nullptr) safeThis->presetBrowser.restoreStateFromProcessor();
                    });
                } else {
                    presetBrowser.setVisible(false);
                    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) {
                        
                        // --- FIX: RESTORE TRUE APVTS VIEW ON PROJECT LOAD ---
                        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(audioProcessor.apvts.getParameter("layout_view"))) {
                            audioProcessor.requestedViewIndex.store(choice->getIndex(), std::memory_order_release);
                        } else {
                            audioProcessor.requestedViewIndex.store(0, std::memory_order_release);
                        }
                        audioProcessor.requestViewChange.store(true, std::memory_order_release);
                        updateWindowSize();
                }
            }
        }
    
    // Protection against drawing on the dead window
    if (!isShowing() || getWidth() <= 0 || getHeight() <= 0 || getTopLevelComponent()->getPeer() == nullptr) {
        audioProcessor.getFrameFlag().store(false, std::memory_order_relaxed);
        return;
    }
    
    // Dynamic Window Aspect Ratio Monitor
    int currentView = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    if (currentView != lastView) {
        lastView = currentView;
        updateWindowSize(); // Automatically snaps window to the new layout's aspect ratio
    }

    // Frame Update
    if (audioProcessor.getFrameFlag().exchange(false, std::memory_order_acquire)) {
        repaint();
    }
    
    // An incomplete on-wire MIDI Sample Dump is NOT a portable project.
    // Report failure once, on the UI thread only, without pretending that
    // another sample already in MAME belongs to the just loaded DAW song.
    if(audioProcessor.originalSdsRecallRejected.exchange(
        false,std::memory_order_acq_rel))
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Akai project recall was rejected",
            "This DAW project was saved while the original Akai MIDI Sample "
            "Dump or its keygroup assignment was unfinished. It contains "
            "no complete original sampler state. The previous instrument "
            "state was left unchanged; import the WAV again, wait until "
            "the Akai firmware confirms the program and save the project.");
    bool missing = audioProcessor.isRomMissing.load(std::memory_order_acquire);
    bool invalid = audioProcessor.isRomInvalid.load(std::memory_order_acquire);
    bool checkFailed = audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire);
    bool unsupportedHost = audioProcessor.isUnsupportedAUHost.load(std::memory_order_acquire);
        
    // P030: MIDI SDS transport state is not a sample/program success receipt.
    // The original firmware alone may confirm actual keygroup assignment.
    const int sdsStage=audioProcessor.originalSdsStage.load(std::memory_order_acquire);
    if(sdsStage>=1 && sdsStage<=7) {
        if(sdsStage==1) loadMediaButton.setButtonText("SDS READY");
        if(sdsStage==2) loadMediaButton.setButtonText("SDS SEND");
        if(sdsStage==3) loadMediaButton.setButtonText("SDS SENT");
        if(sdsStage==4) loadMediaButton.setButtonText("SDS ERROR");
        if(sdsStage==5) loadMediaButton.setButtonText("SDS MAP");
        if(sdsStage==6) loadMediaButton.setButtonText("SDS MAP OK");
        if(sdsStage==7) loadMediaButton.setButtonText("SDS MAP FAIL");
    }
    // P004 display only original firmware-verified recording phases.
    // GUI timer never advances the firmware or presses hardware controls.
    const auto captureStage=static_cast<akai::capture::Stage>(
        audioProcessor.autoCaptureStage.load(std::memory_order_acquire));
    const bool captureActive=akai::capture::isRunningStage(captureStage);
    const bool captured = captureStage==akai::capture::Stage::RecordedSample;
    if(captureActive) {
        if(captureStage>=akai::capture::Stage::Capturing
           && captureStage<=akai::capture::Stage::WaitStop)
            loadMediaButton.setButtonText("WAV REC");
        else if(captureStage>=akai::capture::Stage::PressName
             && captureStage<=akai::capture::Stage::WaitNamed)
            loadMediaButton.setButtonText("WAV NAME");
        else if(captureStage>=akai::capture::Stage::PressSelectSingle
             && captureStage<=akai::capture::Stage::WaitAssigned)
            loadMediaButton.setButtonText("WAV MAP");
        else
            loadMediaButton.setButtonText("WAV ARM");
    } else if(captured)
        loadMediaButton.setButtonText("WAV DONE");
    else if(sdsStage==0)
        loadMediaButton.setButtonText("MEDIA");
    // One precise message per failure, never a repeated modal at 30 Hz.
    const int statusCode=int(captureStage);
    if (statusCode>=20 && statusCode<=28 && statusCode!=lastCaptureNotice) {
        lastCaptureNotice=statusCode;
        juce::String details;
        if(captureStage==akai::capture::Stage::NeedNewSample)
            details="The original Akai RECORD page did not confirm a unique NEW sample name "
                    "after bounded automatic NAME/number entry attempts. "
                    "No sample was overwritten. Check the native LCD and choose a fresh sample name.";
        else if(captureStage==akai::capture::Stage::NoReadableLcd)
            details="The firmware LCD could not be read reliably in text mode. "
                    "No further automatic keys were sent. You may record the staged WAV manually "
                    "with SAMPLE > REC > ARM > GO.";
        else if(captureStage==akai::capture::Stage::OverwriteRisk)
            details="The firmware indicated an existing sample/overwrite risk. "
                    "The automation refused to confirm it.";
        else if(captureStage==akai::capture::Stage::Busy)
            details="A panel key was already held by the user/host. Automation stopped without "
                    "taking control of that key.";
        else if(captureStage==akai::capture::Stage::TimedOut)
            details="The original Akai firmware did not confirm the expected page/state. "
                    "Automatic recording was stopped without guessing.";
        else if(captureStage==akai::capture::Stage::SourceUnavailable)
            details="The WAV source was unloaded or replaced during Akai capture. "
                    "No further recording keys were sent; discard any incomplete native sample.";
        else if(captureStage==akai::capture::Stage::SampleTruncated)
            details="The Akai stopped recording before the WAV finished. "
                    "Increase the Akai REC TIME to cover the whole WAV, then import again. "
                    "The shortened recording is NOT accepted as a complete import.";
        else if(captureStage==akai::capture::Stage::ProgramUnassigned)
            details="The WAV was recorded by the original Akai firmware but the "
                    "original TEST PROGRAM's first keygroup did not confirm a valid "
                    "sample assignment. The WAV is NOT yet MIDI-playable. "
                    "No guessed sample-memory or program data was written.";
        if(details.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Akai WAV recording",details);
    }

    // ROM error buttons visibility
    locateRomButton.setVisible(missing);
    rescanRomButton.setVisible(missing);
    locateRomButton.setButtonText("Locate ROMs...");
    rescanRomButton.setButtonText("Rescan ROMs Folder");

    // Check if File Manager is opened
    bool isFileManagerOpen = (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4);

    // Main UI buttons should only be visible if there are no errors and MAME can run
    bool showMainUi = !(missing || invalid || checkFailed || unsupportedHost) && !isFileManagerOpen;
    loadMediaButton.setVisible(showMainUi);
    settingsButton.setVisible(showMainUi);
    saveBadge.setVisible(showMainUi && !isSettingsVisible);
    
    if (!showMainUi) {
        savePromptLabel.setVisible(false);
    }
    // --- UX: FIRST LAUNCH WELCOME MESSAGE ---
    // Trigger only if the main UI is ready (MAME booted successfully, no errors)
        if (showMainUi && audioProcessor.showWelcomeMessage.exchange(false, std::memory_order_acquire)) {
            auto options = juce::MessageBoxOptions()
                .withTitle("Welcome to S3000XL-VST!")
                .withMessage("First, click the Settings/About button, set the buffer value suitable for your CPU, and select a GUI panel.\nHave fun!")
                .withButton("OK")
                .withIconType(juce::MessageBoxIconType::InfoIcon)
                .withAssociatedComponent(this);
                
            juce::AlertWindow::showAsync(options, [this](int) {
                saveGlobalSettings(); // Persist the flag immediately after clicking OK
            });
        }
    
}
// debug rack: original paint() preserved below for non-debug builds
#ifndef SD1_DEBUG_RACK_PANEL
void EnsoniqSD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // ==============================================================================
    // --- SAFEGUARDS & ERROR SCREENS ---
    // ==============================================================================
    
    // --- UNSUPPORTED AU HOST WARNING MESSAGE ---
    if (audioProcessor.isUnsupportedAUHost.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::orange);
        g.setFont(22.0f);
        
        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nUnsupported AU Host Detected!\n\nFor DAWs other than Logic, GarageBand, MainStage, Reaper, Ableton Live and Fender Studio Pro (Studio One),\nplease use the VST3 version of this plugin.\n\nThis ensures sample-accurate offline rendering and UI stability.";
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20), juce::Justification::centred, 10);
        return;
    }
    
    // --- SAMPLE RATE MISMATCH WARNING MESSAGE ---
    if (audioProcessor.sampleRateMismatch.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::orange);
        g.setFont(24.0f);
        
        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nSample Rate Changed!\n\nThe host sample rate has changed since the plugin was loaded.\nTo prevent audio glitches, please reload the plugin or restart your DAW.";
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20), juce::Justification::centred, 10);
        return;
    }
    
    // --- MISSING OR INCOMPLETE ROM WARNING MESSAGE ---
        if (audioProcessor.isRomMissing.load(std::memory_order_acquire)) {
            g.fillAll(juce::Colour(0xff222222));
            g.setColour(juce::Colours::orange);
            g.setFont(20.0f);
            
            juce::String romsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                     .getChildFile("S3000XL").getChildFile("ROMs").getFullPathName();

            juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nROM files missing or incomplete!\n\n";
            errorMsg += "The emulator needs 10 specific '.bin' files. You can drop your downloaded .zip files or loose .bin files directly into:\n";
            errorMsg += romsDir + "\n\n";
            errorMsg += "Missing files:\n" + audioProcessor.missingFilesList + "\n";
            errorMsg += "Click 'Locate ROMs...' to extract from a .zip or copy from a folder.";
            
            g.drawFittedText(errorMsg, getLocalBounds().reduced(20).withTrimmedBottom(120), juce::Justification::centred, 12);
            return;
        }
    
    // --- SELF CHECK FAILED WARNING MESSAGE ---
    if (audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::red);
        g.setFont(22.0f);
        
        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nSystem Integrity Check Failed!\n\nThe plugin cannot start due to the following issues:\n\n";
        errorMsg += audioProcessor.selfCheckErrorMsg;
        errorMsg += "\n\nPlease fix these permission/installation issues and reload the plugin.";
        
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20), juce::Justification::centred, 15);
        return;
    }
    
    // --- ENGINE STANDBY / OFFLINE MESSAGE ---
    // If ROMs are fine, NO self-check errors, but MAME is simply not running.
    if (!audioProcessor.isMameRunningFlag() &&
        !audioProcessor.isRomMissing.load(std::memory_order_acquire) &&
        !audioProcessor.isRomInvalid.load(std::memory_order_acquire) &&
        !audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire))
    {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::red);
        g.setFont(22.0f);

        juce::String msg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nMAME(R) Engine Offline\n\nThe emulator is halted or waiting for the host's audio engine.\n\nIf your DAW is currently scanning plugins, this is normal.\nIf the emulator crashed or failed to start, please reload the plugin!";
        g.drawFittedText(msg, getLocalBounds().reduced(20), juce::Justification::centred, 10);
        return;
    }
   
    // FILE MANAGER
    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) {
        return; // If File Manager no MAME GUI
    }
    
    // ==============================================================================
    // --- 1. MAME RENDER ROUTINE (Base Layer: Physical machine, grey panel, buttons)
    // ==============================================================================
    int readIndex = audioProcessor.readyBufferIndex.load(std::memory_order_acquire);
    
    int renderW = juce::jmin(getWidth(), audioProcessor.screenBuffers[readIndex].getWidth());
    int renderH = juce::jmin(getHeight(), audioProcessor.screenBuffers[readIndex].getHeight());
    
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        
    g.drawImage(audioProcessor.screenBuffers[readIndex],
                0, 0, renderW, renderH, 
                0, 0, renderW, renderH, 
                false); 

    // ==============================================================================
    // --- 2. Optional former SD-1 decals absent; use ORIGINAL Akai MAME artwork ---
    // ==============================================================================
    int viewIdx = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    if (viewIdx == 4) {
        return;
    }
    juce::Image overlayImage;

    // Index Mapping: 0 = Compact, 1 = Full, 2 = Rack, 3 = Tablet
    if (viewIdx == 0) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 1) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 2) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 3) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 4) {
        float totalBaseH = 925.0f + 379.0f;
                int fmHeight = juce::roundToInt(getHeight() * (925.0f / totalBaseH));
                int rackHeight = getHeight() - fmHeight;

                g.drawImage(audioProcessor.screenBuffers[readIndex],
                            0, fmHeight, getWidth(), rackHeight,
                            0, 0, audioProcessor.screenBuffers[readIndex].getWidth(),
                            audioProcessor.screenBuffers[readIndex].getHeight());

                // Rajzoljuk rá a Rack feliratokat (decal)
                auto overlayImage = juce::Image{};
                if (overlayImage.isValid()) {
                    g.drawImage(overlayImage, 0, fmHeight, getWidth(), rackHeight, 0, 0, 2048, 379, false);
                }
    }

    if (overlayImage.isValid()) {
        /* S3000XL: SD-1 label overlay disabled (panel comes from MAME s3000xl layout) */;
    }
        
    // ==============================================================================
    // --- 3. PANELS & BADGES (Floppy / Cartridge / Save) ---
    // ==============================================================================
    int viewIdxBadge = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    int baseWBadge = 2048;
    int baseHBadge = 925; // Default Compact
    if (viewIdxBadge == 1) baseHBadge = 671;
    else if (viewIdxBadge == 2) baseHBadge = 379;
    else if (viewIdxBadge == 3) baseHBadge = 1476;

    float scaleXBadge = getWidth() / (float)baseWBadge;
    float scaleYBadge = getHeight() / (float)baseHBadge;

    // --- Variables for Media Badges ---
    float badgeW = 260.0f;
    float badgeH = 30.0f;
    float flopX = 0.0f, flopY = 0.0f;
    float cartX = 0.0f, cartY = 0.0f;

    // --- Variables for Save Badge (SYNCED EXACTLY WITH RESIZED) ---
    float saveW = 130.0f;
    float saveH = 30.0f;
    float saveX = 0.0f, saveY = 0.0f;

    if (viewIdxBadge == 0) { // Compact
        flopX = 304.0f; flopY = 402.0f;
        cartX = 574.0f; cartY = 402.0f;
        
        saveW = 130.0f;  saveH = 30.0f;
        saveX = 471.0f;  saveY = 214.0f;
    } else if (viewIdxBadge == 1) { // Full
        badgeW = 234.0f; badgeH = 30.0f;
        flopX = 59.0f;   flopY = 288.0f;
        cartX = 1718.0f; cartY = 176.0f;
        
        saveW = 130.0f;  saveH = 30.0f;
        saveX = 954.0f;  saveY = 325.0f;
    } else if (viewIdxBadge == 2) { // Rack
        badgeW = 192.0f; badgeH = 30.0f;
        flopX = 20.0f;   flopY = 306.0f;
        cartX = 220.0f;  cartY = 306.0f;
        
        saveW = 130.0f;  saveH = 30.0f;
        saveX = 471.0f;  saveY = 214.0f;
    } else if (viewIdxBadge == 3) { // Tablet
        badgeW = 300.0f; badgeH = 40.0f;
        flopX = 36.0f;   flopY = 1303.0f;
        cartX = 356.0f;  cartY = 1303.0f;
        
        saveW = 180.0f;  saveH = 40.0f;
        saveX = 343.0f;  saveY = 448.0f;
    }

    auto drawBadge = [&](float x, float y, float w, float h, const juce::String& text, juce::Drawable* icon, juce::Colour ledColor, float alpha, bool hasLed) {
        float finalX = x * scaleXBadge;
        float finalY = y * scaleYBadge;
        float finalW = w * scaleXBadge;
        float finalH = h * scaleYBadge;

        juce::Rectangle<float> indRect(finalX, finalY, finalW, finalH);

        g.setColour(juce::Colour(0xcc111111).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(indRect, 4.0f);

        float padding = finalH * 0.2f;
        float iconStartX = indRect.getX() + padding;

        if (hasLed) {
            float ledSize = finalH * 0.35f;
            g.setColour(ledColor.withMultipliedAlpha(alpha));
            g.fillEllipse(iconStartX, indRect.getCentreY() - (ledSize / 2.0f), ledSize, ledSize);
            iconStartX += ledSize + padding;
        } else {
            iconStartX += padding;
        }

        float iconSize = finalH * 0.65f;
        juce::Rectangle<float> iconBounds(iconStartX, indRect.getCentreY() - (iconSize / 2.0f), iconSize, iconSize);

        if (icon != nullptr) {
            icon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, alpha);
        }

        g.setColour(juce::Colours::white.withMultipliedAlpha(alpha));
        g.setFont(finalH * 0.55f);
        
        float textStartX = iconBounds.getRight() + padding;
        juce::Rectangle<float> textRect(textStartX, indRect.getY(), indRect.getRight() - textStartX, finalH);
        g.drawText(text, textRect, juce::Justification::centredLeft, true);
    };

    bool hasFloppy = audioProcessor.isFloppyLoaded.load(std::memory_order_acquire);
    bool hasCart = audioProcessor.isCartLoaded.load(std::memory_order_acquire);

    if (hasFloppy) drawBadge(flopX, flopY, badgeW, badgeH, audioProcessor.loadedFloppyName, floppyIcon.get(), juce::Colours::limegreen, 1.0f, true);
    if (hasCart)   drawBadge(cartX, cartY, badgeW, badgeH, audioProcessor.loadedCartName, cartIcon.get(), juce::Colours::limegreen, 1.0f, true);
    
    float saveAlpha = saveBadge.isEnabled() ? 1.0f : 0.4f;
    juce::ignoreUnused(saveX, saveY, saveW, saveH, saveAlpha); // S3000XL: "Save Program" badge removed (overlapped the LCD; unused on MPC)

    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) != 4)
        paintAnalogKnobIndicators(g);

    // ==============================================================================
    // --- 4. SETTINGS OVERLAY (Dimmed background & Settings Panel)
    // ==============================================================================
    if (isSettingsVisible) {
        g.fillAll(juce::Colour(0x4d000000)); 
        
        auto panelRect = settingsGroup.getBounds().toFloat();
        panelRect.removeFromTop(8.0f);
        
        g.setColour(juce::Colour(0xcc000000)); 
        g.fillRoundedRectangle(panelRect, 5.0f);
    }
}
#endif // !SD1_DEBUG_RACK_PANEL

// debug rack
#ifdef SD1_DEBUG_RACK_PANEL
void EnsoniqSD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // ==============================================================================
    // --- SAFEGUARDS & ERROR SCREENS ---
    // ==============================================================================
    
    // --- UNSUPPORTED AU HOST WARNING MESSAGE ---
    if (audioProcessor.isUnsupportedAUHost.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::orange);
        g.setFont(22.0f);
        
        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nUnsupported AU Host Detected!\n\nFor DAWs other than Logic, GarageBand, MainStage, Reaper, Ableton Live and Fender Studio Pro (Studio One),\nplease use the VST3 version of this plugin.\n\nThis ensures sample-accurate offline rendering and UI stability.";
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20), juce::Justification::centred, 10);
        return;
    }
    
    // --- SAMPLE RATE MISMATCH WARNING MESSAGE ---
    if (audioProcessor.sampleRateMismatch.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::orange);
        g.setFont(24.0f);
        
        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nSample Rate Changed!\n\nThe host sample rate has changed since the plugin was loaded.\nTo prevent audio glitches, please reload the plugin or restart your DAW.";
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20), juce::Justification::centred, 10);
        return;
    }
    
    // --- MISSING OR INCOMPLETE ROM WARNING MESSAGE ---
    if (audioProcessor.isRomMissing.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::orange);
        g.setFont(20.0f);
        
        juce::String romsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                 .getChildFile("S3000XL").getChildFile("ROMs").getFullPathName();

        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nROM files missing or incomplete!\n\n";
        errorMsg += "The emulator needs 10 specific '.bin' files. You can drop your downloaded .zip files or loose .bin files directly into:\n";
        errorMsg += romsDir + "\n\n";
        errorMsg += "Missing files:\n" + audioProcessor.missingFilesList + "\n";
        errorMsg += "Click 'Locate ROMs...' to extract from a .zip or copy from a folder.";
        
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20).withTrimmedBottom(120), juce::Justification::centred, 12);
        return;
    }
    
    // --- SELF CHECK FAILED WARNING MESSAGE ---
    if (audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire)) {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::red);
        g.setFont(22.0f);
        
        juce::String errorMsg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nSystem Integrity Check Failed!\n\nThe plugin cannot start due to the following issues:\n\n";
        errorMsg += audioProcessor.selfCheckErrorMsg;
        errorMsg += "\n\nPlease fix these permission/installation issues and reload the plugin.";
        
        g.drawFittedText(errorMsg, getLocalBounds().reduced(20), juce::Justification::centred, 15);
        return;
    }
    
    // --- ENGINE STANDBY / OFFLINE MESSAGE ---
    if (!audioProcessor.isMameRunningFlag() &&
        !audioProcessor.isRomMissing.load(std::memory_order_acquire) &&
        !audioProcessor.isRomInvalid.load(std::memory_order_acquire) &&
        !audioProcessor.isSelfCheckFailed.load(std::memory_order_acquire))
    {
        g.fillAll(juce::Colour(0xff222222));
        g.setColour(juce::Colours::red);
        g.setFont(22.0f);

        juce::String msg = "S3000XL-VST - S3000XL Emulation (MAME(R))\n\nMAME(R) Engine Offline\n\nThe emulator is halted or waiting for the host's audio engine.\n\nIf your DAW is currently scanning plugins, this is normal.\nIf the emulator crashed or failed to start, please reload the plugin!";
        g.drawFittedText(msg, getLocalBounds().reduced(20), juce::Justification::centred, 10);
        return;
    }
   
    // ==============================================================================
    // --- 1. MAME RENDER ROUTINE (Base Layer: Physical machine, grey panel, buttons)
    // ==============================================================================
    int readIndex = audioProcessor.readyBufferIndex.load(std::memory_order_acquire);
    int viewIdx = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    
    if (viewIdx != 4) {
        // Normál nézeteknél a bal felső sarokból (0,0) rajzoljuk a MAME-et
        int renderW = juce::jmin(getWidth(), audioProcessor.screenBuffers[readIndex].getWidth());
        int renderH = juce::jmin(getHeight(), audioProcessor.screenBuffers[readIndex].getHeight());
            
        g.drawImage(audioProcessor.screenBuffers[readIndex],
                    0, 0, renderW, renderH,
                    0, 0, renderW, renderH,
                    false);
    }

    // ==============================================================================
    // --- 2. JUCE PNG LABELS OVERLAY (Top Layer: Transparent decal over the machine)
    // ==============================================================================
    juce::Image overlayImage;

    // Index Mapping: 0 = Compact, 1 = Full, 2 = Rack, 3 = Tablet
    if (viewIdx == 0) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 1) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 2) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 3) {
        overlayImage = juce::Image{};
    } else if (viewIdx == 4) {
        
        // HIBRID NÉZET: File Manager felül, Rack panel alul
                float totalBaseH = 925.0f + 379.0f;
                int fmHeight = juce::roundToInt(getHeight() * (925.0f / totalBaseH));
                int rackHeight = getHeight() - fmHeight;

                // VÁGÁS (Cropping): Nem torzítjuk a képet, hanem kivágjuk az eredeti Rack nézet alját!
                // A MAME buffere továbbra is teljes méretű (pl. 1200x539), mi csak a szükséges magasságot másoljuk át.
                g.drawImage(audioProcessor.screenBuffers[readIndex],
                            0, fmHeight, getWidth(), rackHeight,  // Cél (hová rajzoljuk a VST ablakban)
                            0, 0, getWidth(), rackHeight,         // Forrás (honnan vágjuk a MAME bufferből)
                            false);

                // Rajzoljuk rá a Rack feliratokat (decal)
                auto rackOverlay = juce::Image{};
                if (rackOverlay.isValid()) {
                    g.drawImage(rackOverlay, 0, fmHeight, getWidth(), rackHeight, 0, 0, 2048, 379, false);
                }
    }

    if (viewIdx != 4 && overlayImage.isValid()) {
        /* S3000XL: SD-1 label overlay disabled (panel comes from MAME s3000xl layout) */;
    }
        
    // ==============================================================================
    // --- 3. PANELS & BADGES (Floppy / Cartridge / Save) ---
    // ==============================================================================
    int viewIdxBadge = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    int baseWBadge = 2048;
    int baseHBadge = 925; // Default Compact
    if (viewIdxBadge == 1) baseHBadge = 671;
    else if (viewIdxBadge == 2) baseHBadge = 379;
    else if (viewIdxBadge == 3) baseHBadge = 1476;

    float scaleXBadge = getWidth() / (float)baseWBadge;
    float scaleYBadge = getHeight() / (float)baseHBadge;

    // --- Variables for Media Badges ---
    float badgeW = 260.0f;
    float badgeH = 30.0f;
    float flopX = 0.0f, flopY = 0.0f;
    float cartX = 0.0f, cartY = 0.0f;

    // --- Variables for Save Badge (SYNCED EXACTLY WITH RESIZED) ---
    float saveW = 130.0f;
    float saveH = 30.0f;
    float saveX = 0.0f, saveY = 0.0f;

    if (viewIdxBadge == 0) { // Compact
        flopX = 304.0f; flopY = 402.0f;
        cartX = 574.0f; cartY = 402.0f;
        
        saveW = 130.0f;  saveH = 30.0f;
        saveX = 471.0f;  saveY = 214.0f;
    } else if (viewIdxBadge == 1) { // Full
        badgeW = 234.0f; badgeH = 30.0f;
        flopX = 59.0f;   flopY = 288.0f;
        cartX = 1718.0f; cartY = 176.0f;
        
        saveW = 130.0f;  saveH = 30.0f;
        saveX = 954.0f;  saveY = 325.0f;
    } else if (viewIdxBadge == 2) { // Rack
        badgeW = 192.0f; badgeH = 30.0f;
        flopX = 20.0f;   flopY = 306.0f;
        cartX = 220.0f;  cartY = 306.0f;
        
        saveW = 130.0f;  saveH = 30.0f;
        saveX = 471.0f;  saveY = 214.0f;
    } else if (viewIdxBadge == 3) { // Tablet
        badgeW = 300.0f; badgeH = 40.0f;
        flopX = 36.0f;   flopY = 1303.0f;
        cartX = 356.0f;  cartY = 1303.0f;
        
        saveW = 180.0f;  saveH = 40.0f;
        saveX = 343.0f;  saveY = 448.0f;
    }

    auto drawBadge = [&](float x, float y, float w, float h, const juce::String& text, juce::Drawable* icon, juce::Colour ledColor, float alpha, bool hasLed) {
        float finalX = x * scaleXBadge;
        float finalY = y * scaleYBadge;
        float finalW = w * scaleXBadge;
        float finalH = h * scaleYBadge;

        juce::Rectangle<float> indRect(finalX, finalY, finalW, finalH);

        g.setColour(juce::Colour(0xcc111111).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(indRect, 4.0f);

        float padding = finalH * 0.2f;
        float iconStartX = indRect.getX() + padding;

        if (hasLed) {
            float ledSize = finalH * 0.35f;
            g.setColour(ledColor.withMultipliedAlpha(alpha));
            g.fillEllipse(iconStartX, indRect.getCentreY() - (ledSize / 2.0f), ledSize, ledSize);
            iconStartX += ledSize + padding;
        } else {
            iconStartX += padding;
        }

        float iconSize = finalH * 0.65f;
        juce::Rectangle<float> iconBounds(iconStartX, indRect.getCentreY() - (iconSize / 2.0f), iconSize, iconSize);

        if (icon != nullptr) {
            icon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, alpha);
        }

        g.setColour(juce::Colours::white.withMultipliedAlpha(alpha));
        g.setFont(finalH * 0.55f);
        
        float textStartX = iconBounds.getRight() + padding;
        juce::Rectangle<float> textRect(textStartX, indRect.getY(), indRect.getRight() - textStartX, finalH);
        g.drawText(text, textRect, juce::Justification::centredLeft, true);
    };

    bool hasFloppy = audioProcessor.isFloppyLoaded.load(std::memory_order_acquire);
    bool hasCart = audioProcessor.isCartLoaded.load(std::memory_order_acquire);

    // KULCS: Fájlkezelő alatt elrejtjük a Badge-eket!
    if (viewIdxBadge != 4) {
        if (hasFloppy) drawBadge(flopX, flopY, badgeW, badgeH, audioProcessor.loadedFloppyName, floppyIcon.get(), juce::Colours::limegreen, 1.0f, true);
        if (hasCart)   drawBadge(cartX, cartY, badgeW, badgeH, audioProcessor.loadedCartName, cartIcon.get(), juce::Colours::limegreen, 1.0f, true);
        
        float saveAlpha = saveBadge.isEnabled() ? 1.0f : 0.4f;
        juce::ignoreUnused(saveX, saveY, saveW, saveH, saveAlpha); // S3000XL: "Save Program" badge removed (overlapped the LCD; unused on MPC)
    }

    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) != 4)
        paintAnalogKnobIndicators(g);

    // ==============================================================================
    // --- 4. SETTINGS OVERLAY (Dimmed background & Settings Panel)
    // ==============================================================================
    if (isSettingsVisible) {
        g.fillAll(juce::Colour(0x4d000000));
        
        auto panelRect = settingsGroup.getBounds().toFloat();
        panelRect.removeFromTop(8.0f);
        
        g.setColour(juce::Colour(0xcc000000));
        g.fillRoundedRectangle(panelRect, 5.0f);
    }
}
#endif // SD1_DEBUG_RACK_PANEL



// vége

void EnsoniqSD1AudioProcessorEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    
    // --- PIXEL-PERFECT RESOLUTION NOTIFICATION ---
    audioProcessor.windowWidth.store(getWidth(), std::memory_order_release);
    audioProcessor.windowHeight.store(getHeight(), std::memory_order_release);
    audioProcessor.requestRenderResize.store(true, std::memory_order_release);

    int viewIdx = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    int baseW = 2048;
    int baseH = 1744; // Compact
    if (viewIdx == 1) baseH = 671;
    else if (viewIdx == 2) baseH = 379;
    else if (viewIdx == 3) baseH = 1476;
    /*else if (viewIdx == 4) baseH = 925; // FILE MANAGER PANEL */
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    else if (viewIdx == 4) baseH = 1304;
#endif // SD1_DEBUG_RACK_PANEL
    
    float targetAspect = MPC_VIEW_W / MPC_VIEW_H;
    float currentAspect = (float)getWidth() / (float)getHeight();

    // SAFETY NET: Only save dimensions if the DAW hasn't corrupted the aspect ratio
        if (std::abs(currentAspect - targetAspect) < 0.05f) {
            // --- SEPARATE SIZE STORAGE FOR SYNTH VS FILE MANAGER ---
            if (viewIdx == 4) {
                audioProcessor.fileManagerState.fmWindowWidth = getWidth();
                audioProcessor.fileManagerState.fmWindowHeight = getHeight();
            } else {
                audioProcessor.savedWindowWidth = getWidth();
                audioProcessor.savedWindowHeight = getHeight();
            }
           // audioProcessor.requestGlobalSave.store(true, std::memory_order_release);
        }

    float scaleX = getWidth() / (float)baseW;
    float scaleY = getHeight() / (float)baseH;

    // --- DYNAMIC BUTTON POSITIONING & SAVE BADGE BOUNDS ---

    int loadBtnX = 55; int loadBtnY = 403;
    int setBtnX  = 55; int setBtnY  = 443;
    
    float btnWidth = 192.0f;
    float btnHeight = 30.0f;
    
    // Save badge defaults (Compact View)
    float saveW = 135.0f; float saveH = 30.0f;
    float saveX = 471.0f; float saveY = 214.0f;
    
    // Prompt Label center defaults (Compact View)
    float promptCenterX = 1013.0f;
    float promptCenterY = 273.0f;

    if (viewIdx == 1) { // Full Keyboard
        loadBtnX = 81 ; loadBtnY = 170;
        setBtnX  = 81;  setBtnY  = 210;
        
        saveW = 135.0f;  saveH = 30.0f;
        saveX = 954.0f; saveY = 325.0f;

        promptCenterX = 1115.0f;
        promptCenterY = 216.0f;
    }
    else if (viewIdx == 2) { // Rack Panel
        loadBtnX = 20;  loadBtnY = 270;
        setBtnX  = 220; setBtnY  = 270;
        
        saveW = 135.0f;  saveH = 30.0f;
        saveX = 471.0f;  saveY = 214.0f;

        promptCenterX = 1013.0f;
        promptCenterY = 273.0f;
    }
    else if (viewIdx == 3) { // Tablet
        btnWidth = 300.0f;
        btnHeight = 40.0f;
        
        loadBtnX = 36;  loadBtnY = 1253;
        setBtnX  = 356; setBtnY  = 1253;
        
        saveW = 190.0f;  saveH = 40.0f;
        saveX = 343.0f;  saveY = 448.0f;

        promptCenterX = 1443.0f;
        promptCenterY = 544.0f;
    }

    loadMediaButton.setBounds(juce::roundToInt(loadBtnX * scaleX), juce::roundToInt(loadBtnY * scaleY),
                              juce::roundToInt(btnWidth * scaleX), juce::roundToInt(btnHeight * scaleY));
                              
    settingsButton.setBounds(juce::roundToInt(setBtnX * scaleX), juce::roundToInt(setBtnY * scaleY),
                             juce::roundToInt(btnWidth * scaleX), juce::roundToInt(btnHeight * scaleY));

    // Place the invisible, clickable hotspot exactly over the drawn save badge
    saveBadge.setBounds(juce::roundToInt(saveX * scaleX), juce::roundToInt(saveY * scaleY),
                        juce::roundToInt(saveW * scaleX), juce::roundToInt(saveH * scaleY));

    // --- POSITION THE PROMPT LABEL (Auto-Centered) ---
    int promptW = juce::roundToInt(400.0f * scaleX);
    int promptH = juce::roundToInt(30.0f * scaleY);
    int finalCenterX = juce::roundToInt(promptCenterX * scaleX);
    int finalCenterY = juce::roundToInt(promptCenterY * scaleY);

    savePromptLabel.setSize(promptW, promptH);
    savePromptLabel.setCentrePosition(finalCenterX, finalCenterY);

    // --- FLEXIBLE SETTINGS PANEL ---
    auto bounds = getLocalBounds();
    auto panelRect = bounds.withSizeKeepingCentre(juce::jmin(600, getWidth() - 20), juce::jmin(320, getHeight() - 20));
    settingsGroup.setBounds(panelRect);

    int y = panelRect.getY() + 35;
    
    int totalWidth = 520;
    int startX = panelRect.getCentreX() - (totalWidth / 2);
    int rowHeight = 25;
    int spacing = (panelRect.getHeight() > 270) ? 15 : 5;

    bufferLabel.setBounds(startX, y, 160, rowHeight);
    bufferCombo.setBounds(bufferLabel.getRight() + 5, y, 80, rowHeight);

    viewLabel.setBounds(bufferCombo.getRight() + 15, y, 100, rowHeight);
    viewCombo.setBounds(viewLabel.getRight() + 5, y, 155, rowHeight);

    y += rowHeight + spacing;

    if (panelRect.getHeight() > 280) {
        if (isSettingsVisible) aboutLabel.setVisible(true);
        aboutLabel.setBounds(panelRect.getX() + 10, y, panelRect.getWidth() - 20, 85);
        y += 85 + spacing;
    } else {
        aboutLabel.setVisible(false);
    }

    webLink.setBounds(panelRect.getX() + 10, y, panelRect.getWidth() - 20, rowHeight);
    y += rowHeight + spacing;

    closeSettingsButton.setBounds(panelRect.getCentreX() - 50, y, 100, rowHeight);
    
    // ROM Buttons positions
    locateRomButton.setBounds(getWidth() / 2 - 210, getHeight() - 100, 200, 40);
    rescanRomButton.setBounds(getWidth() / 2 + 10, getHeight() - 100, 200, 40);
    /*
    // FILE MANAGER
    presetBrowser.setBounds(getLocalBounds());*/
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    if (viewIdx == 4) {

            int fmHeight = juce::roundToInt(getHeight() * (925.0f / 1304.0f));

            presetBrowser.setBounds(0, 0, getWidth(), fmHeight);

            audioProcessor.windowWidth.store(getWidth(), std::memory_order_release);
            audioProcessor.windowHeight.store(getHeight() - fmHeight, std::memory_order_release);
            audioProcessor.requestRenderResize.store(true, std::memory_order_release);

        } else {

            presetBrowser.setBounds(getLocalBounds());


            audioProcessor.windowWidth.store(getWidth(), std::memory_order_release);
            audioProcessor.windowHeight.store(getHeight(), std::memory_order_release);
            audioProcessor.requestRenderResize.store(true, std::memory_order_release);
        }
#else
    // FILE MANAGER
    presetBrowser.setBounds(getLocalBounds());
#endif // SD1_DEBUG_RACK_PANEL
}

// ==============================================================================
// MOUSE EVENT INJECTION
// ==============================================================================

// S3000XL: map a click in window pixels to a buttonParams[] index using the layout
// rects, accounting for the letterbox (view fit into the render target, centered).
int EnsoniqSD1AudioProcessorEditor::mpcHitTest(int px, int py)
{
    const float winW = (float) getWidth(), winH = (float) getHeight();
    if (winW <= 0.0f || winH <= 0.0f) return -1;
    const float scale = juce::jmin(winW / MPC_VIEW_W, winH / MPC_VIEW_H);
    const float offX = (winW - MPC_VIEW_W * scale) * 0.5f;
    const float offY = (winH - MPC_VIEW_H * scale) * 0.5f;
    for (int i = 0; i < MPC_CLICK_COUNT; ++i) {
        const auto& r = mpcClickMap[i];
        const float sx = offX + r.x * scale, sy = offY + r.y * scale;
        const float sw = r.w * scale, sh = r.h * scale;
        if (px >= sx && px < sx + sw && py >= sy && py < sy + sh) return r.paramIndex;
    }
    return -1;
}

void EnsoniqSD1AudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    int viewIdx = audioProcessor.requestedViewIndex.load();
        if (viewIdx == 4) {
            float totalBaseH = 925.0f + 379.0f;
            int fmHeight = juce::roundToInt(getHeight() * (925.0f / totalBaseH));
            if (e.y >= fmHeight) {
                audioProcessor.injectMouseDown(e.x, e.y - fmHeight);
            }
            return;
        }
#endif // SD1_DEBUG_RACK_PANEL
    
    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) return;
    if (isSettingsVisible || getWidth() <= 0 || getHeight() <= 0) return;
    // These two zones represent hardware analogue knobs, not matrix buttons.
    const int mpcIdx = mpcHitTest(e.x, e.y);
    if (mpcIdx == 38 || mpcIdx == 39) {
        if (auto* parameter = analogParameter(mpcIdx)) {
            if (e.mods.isCtrlDown()) {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->getDefaultValue());
                parameter->endChangeGesture();
            } else {
                activeAnalogKnob = mpcIdx;
                analogKnobStartY = e.y;
                analogKnobStartValue = parameter->getValue();
                parameter->beginChangeGesture();
            }
            repaint();
        }
        return;
    }
    // Matrix buttons still follow the original MAME GPIO signal.
    if (mpcIdx >= 0) {
        // DATA ENTRY dial zones (btn_dial_dec=36 left, btn_dial_inc=37 right): click = 1 step,
        // then drag up/down to turn continuously.
        if (mpcIdx == 36 || mpcIdx == 37) {
            // P044: the editor must not change an external hardware input
            // without also changing the automatable/saved host parameter.
            // The original MAME device saves its INTERNAL encoder position;
            // mismatched host DATAENTRY at project restore produces spurious
            // knob turns and selects a different original Akai sample.
            if (auto* parameter = audioProcessor.apvts.getParameter("data_entry")) {
                parameter->beginChangeGesture();
                if (e.mods.isCtrlDown()) {
                    parameter->setValueNotifyingHost(parameter->getDefaultValue());
                    parameter->endChangeGesture();
                    dialDragActive = false;
                } else {
                    const int step = mpcIdx == 37 ? 1 : -1;
                    const int dv = (audioProcessor.dialValue.load(
                        std::memory_order_acquire) + step) & 0xff;
                    parameter->setValueNotifyingHost(float(dv) / 255.0f);
                    dialDragActive = true;
                    dialDragLastY = e.y;
                }
            }
            mpcPressedIndex = -1;
            return;
        }
        mpcPressedIndex = mpcIdx;
        if (mpcIdx < (int) audioProcessor.buttonParams.size() && audioProcessor.buttonParams[mpcIdx])
            audioProcessor.buttonParams[mpcIdx]->store(1.0f, std::memory_order_relaxed);
        return;
    }
    audioProcessor.injectMouseDown(e.x, e.y);
}

void EnsoniqSD1AudioProcessorEditor::mouseUp(const juce::MouseEvent& e) {
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    int viewIdx = audioProcessor.requestedViewIndex.load();
        if (viewIdx == 4) {
            float totalBaseH = 925.0f + 379.0f;
            int fmHeight = juce::roundToInt(getHeight() * (925.0f / totalBaseH));
            if (e.y >= fmHeight) {
                audioProcessor.injectMouseDown(e.x, e.y - fmHeight);
            }
            return;
        }
#endif // SD1_DEBUG_RACK_PANEL
    
    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) return;
    if (isSettingsVisible || getWidth() <= 0 || getHeight() <= 0) return;
    if (activeAnalogKnob >= 0) {
        if (auto* parameter = analogParameter(activeAnalogKnob))
            parameter->endChangeGesture();
        activeAnalogKnob = -1;
        repaint();
        return;
    }
    if (dialDragActive) {
        dialDragActive = false;
        if (auto* parameter = audioProcessor.apvts.getParameter("data_entry"))
            parameter->endChangeGesture();
        return;
    }
    // S3000XL: release the button pressed via buttonParams on mouse-down.
    if (mpcPressedIndex >= 0) {
        if (mpcPressedIndex < (int) audioProcessor.buttonParams.size() && audioProcessor.buttonParams[mpcPressedIndex])
            audioProcessor.buttonParams[mpcPressedIndex]->store(0.0f, std::memory_order_relaxed);
        mpcPressedIndex = -1;
        return;
    }
    audioProcessor.injectMouseUp(e.x, e.y);
    
    // NOTE: Save Program macro completion is handled in timerCallback via "WRITING"
    // VFD detection — NOT on mouseUp, because mouseUp also fires when switching banks,
    // and releasing the bank mid-write would corrupt the save.
}

void EnsoniqSD1AudioProcessorEditor::mouseDrag(const juce::MouseEvent& e) {
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    int viewIdx = audioProcessor.requestedViewIndex.load();
        if (viewIdx == 4) {
            float totalBaseH = 925.0f + 379.0f;
            int fmHeight = juce::roundToInt(getHeight() * (925.0f / totalBaseH));
            if (e.y >= fmHeight) {
                audioProcessor.injectMouseDown(e.x, e.y - fmHeight);
            }
            return;
        }
#endif // SD1_DEBUG_RACK_PANEL
    
    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) return;
    if (isSettingsVisible || getWidth() <= 0 || getHeight() <= 0) return;
    if (activeAnalogKnob >= 0) {
        if (auto* parameter = analogParameter(activeAnalogKnob)) {
            const float v = juce::jlimit(0.0f, 1.0f, analogKnobStartValue
                + static_cast<float>(analogKnobStartY - e.y) / 150.0f);
            parameter->setValueNotifyingHost(v);
            repaint();
        }
        return;
    }
    if (dialDragActive) {
        const int steps = (dialDragLastY - e.y) / 2; // drag up = increase
        if (steps != 0) {
            const int dv = (audioProcessor.dialValue.load(
                std::memory_order_acquire) + steps) & 0xff;
            if (auto* parameter = audioProcessor.apvts.getParameter("data_entry"))
                parameter->setValueNotifyingHost(float(dv) / 255.0f);
            dialDragLastY = e.y;
        }
        return;
    }
    audioProcessor.injectMouseMove(e.x, e.y);
}

void EnsoniqSD1AudioProcessorEditor::mouseMove(const juce::MouseEvent& e) {
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    int viewIdx = audioProcessor.requestedViewIndex.load();
        if (viewIdx == 4) {
            float totalBaseH = 925.0f + 379.0f;
            int fmHeight = juce::roundToInt(getHeight() * (925.0f / totalBaseH));
            if (e.y >= fmHeight) {
                audioProcessor.injectMouseDown(e.x, e.y - fmHeight);
            }
            return;
        }
#endif // SD1_DEBUG_RACK_PANEL
    
    if (audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) return;
    if (isSettingsVisible || getWidth() <= 0 || getHeight() <= 0) return;
    audioProcessor.injectMouseMove(e.x, e.y);
}

juce::RangedAudioParameter* EnsoniqSD1AudioProcessorEditor::analogParameter(int knobIndex) const
{
    if (knobIndex == 38) return audioProcessor.apvts.getParameter("rec_gain");
    if (knobIndex == 39) return audioProcessor.apvts.getParameter("main_volume");
    return nullptr;
}

void EnsoniqSD1AudioProcessorEditor::mouseWheelMove(
    const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (isSettingsVisible || audioProcessor.requestedViewIndex.load(std::memory_order_acquire) == 4) return;
    const int idx = mpcHitTest(e.x, e.y);
    if (idx != 38 && idx != 39) return;
    if (auto* parameter = analogParameter(idx)) {
        const float v = juce::jlimit(0.0f, 1.0f, parameter->getValue() + wheel.deltaY * 0.06f);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(v);
        parameter->endChangeGesture();
        repaint();
    }
}

void EnsoniqSD1AudioProcessorEditor::paintAnalogKnobIndicators(juce::Graphics& g)
{
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    if (w <= 0.0f || h <= 0.0f) return;
    const float scale = juce::jmin(w / MPC_VIEW_W, h / MPC_VIEW_H);
    const float ox = (w - MPC_VIEW_W * scale) * 0.5f;
    const float oy = (h - MPC_VIEW_H * scale) * 0.5f;
    // Knob centres derived from stock MAME s3000xl.lay group transforms.
    // A discreet position dot leaves the original MAME artwork intact.
    auto drawOne = [&](int idx, float x, juce::Colour colour) {
        auto* p = analogParameter(idx);
        if (p == nullptr) return;
        const float v = juce::jlimit(0.0f, 1.0f, p->getValue());
        const float theta = (v * 270.0f - 135.0f) * juce::MathConstants<float>::pi / 180.0f;
        const float cx = ox + x * scale;
        const float cy = oy + 292.2f * scale;
        const float rad = 91.0f * scale;
        const float mx = cx + std::sin(theta) * rad;
        const float my = cy - std::cos(theta) * rad;
        const float sz = juce::jmax(2.0f, 17.0f * scale);
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.fillEllipse(mx - sz * 0.65f, my - sz * 0.65f, sz * 1.3f, sz * 1.3f);
        g.setColour(colour);
        g.fillEllipse(mx - sz * 0.5f, my - sz * 0.5f, sz, sz);
    };
    drawOne(38, 4848.4f, juce::Colour(0xffaf4f40));
    drawOne(39, 5263.4f, juce::Colour(0xffdfdfc6));
}

bool EnsoniqSD1AudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (isSettingsVisible || !audioProcessor.isMameRunningFlag()) return false;
    for (const auto& path : files)
        if (juce::File(path).hasFileExtension(".wav")) return true;
    return false;
}
void EnsoniqSD1AudioProcessorEditor::filesDropped(const juce::StringArray& files,
                                                   int, int)
{
    for (const auto& path : files)
        if (juce::File(path).hasFileExtension(".wav")) {
            importWavFile(juce::File(path));
            return;
        }
}
void EnsoniqSD1AudioProcessorEditor::importWavFile(const juce::File& file)
{
    juce::String error;
    if (!audioProcessor.stageWavForOriginalSds(file,error)) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Akai WAV input",error);
        return;
    }
    // Transfer is performed only by original Akai firmware through its
    // authentic MIDI UART. No analog A/D or host WAV audio callbacks.
    lastCaptureNotice = 0;
    loadMediaButton.setButtonText("SDS: QUEUED");
    loadMediaButton.setTooltip("WAV converted to 16-bit mono MIDI Sample Dump. "
        "The original Akai firmware receives it over the internal MIDI port. "
        "Program keygroup assignment is not yet validated in the VST3 host.");
    repaint();
}
void EnsoniqSD1AudioProcessorEditor::loadMediaButtonClicked()
{
    juce::PopupMenu m;
    m.addSectionHeader("Original Akai MIDI Sample Dump");
    m.addItem(20, "Import WAV via firmware MIDI SDS...");
    const int sdsStage=audioProcessor.originalSdsStage.load(std::memory_order_acquire);
    if(sdsStage==1) m.addSectionHeader("SDS sample prepared");
    if(sdsStage==2) m.addSectionHeader("SDS transfer through original UART");
    if(sdsStage==3) m.addSectionHeader("SDS sent - program mapping not verified");
    if(sdsStage==4) m.addSectionHeader("SDS transfer failed");
    if(sdsStage==5) m.addSectionHeader("Original Akai keygroup assignment in progress");
    if(sdsStage==6) m.addSectionHeader("Original Akai LCD confirms MIDI 99 in keygroup 1");
    if(sdsStage==7) m.addSectionHeader("Original Akai firmware did not confirm the keygroup");
    const auto captureStage=static_cast<akai::capture::Stage>(
        audioProcessor.autoCaptureStage.load(std::memory_order_acquire));
    const bool captureActive=akai::capture::isRunningStage(captureStage);
    m.addItem(22, "Resume automatic WAV recording", !captureActive &&
        audioProcessor.wavState.load(std::memory_order_acquire)==1);
    m.addItem(23, "Cancel automatic WAV recording", captureActive);
    if (audioProcessor.wavState.load(std::memory_order_acquire) != 0) {
        m.addItem(21, "Unload staged WAV");
        m.addSectionHeader("Staged: " + audioProcessor.stagedWavName);
    }
    m.addSeparator();
    m.addSectionHeader("Floppy  (removable - hot swap)");
    m.addItem(2, "Insert Floppy Image (.img, .hfe, .dsk)...");
    m.addItem(4, "Eject Floppy", audioProcessor.isFloppyLoaded.load(std::memory_order_acquire));
    m.addSeparator();
    m.addSectionHeader("CD-ROM  (reboots on change)");
    m.addItem(6, "Insert CD-ROM Image (.chd, .cue, .iso)...");
    m.addItem(7, "Eject CD-ROM", audioProcessor.isCdLoaded.load(std::memory_order_acquire));
    m.addSeparator();
    m.addSectionHeader("SCSI Hard Disk  (fixed - reboots emulator)");
    m.addItem(8, "Set Hard Disk Image (.chd, .hd)...");
    m.addItem(9, "Remove Hard Disk", audioProcessor.isHddLoaded.load(std::memory_order_acquire));

    juce::Component::SafePointer<EnsoniqSD1AudioProcessorEditor> safeThis(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadMediaButton),
        [safeThis](int result) {
            if (safeThis == nullptr || result == 0) return;
            auto* editor = safeThis.getComponent();
            auto& processor = editor->audioProcessor;
            if (result == 23) {
                processor.autoCaptureCancel.store(true,std::memory_order_release);
                return;
            }
            if (result == 22) {
                processor.autoCaptureRequest.store(true,std::memory_order_release);
                editor->lastCaptureNotice=0;
                return;
            }
            if (result == 21) {
                processor.autoCaptureCancel.store(true,std::memory_order_release);
                processor.clearStagedWav();
                processor.stagedWavName.clear();
                editor->repaint();
                return;
            }
            if (result == 20 || result == 2 || result == 6 || result == 8) {
                juce::String title, filter;
                if (result == 20) {
                    title="Select WAV for original Akai MIDI Sample Dump";
                    filter="*.wav;*.WAV";
                } else if (result == 2) {
                    title="Select Floppy Image";filter="*.img;*.hfe;*.dsk;*.ima;*.imd;*.td0;*.ipf";
                } else if (result == 6) {
                    title="Select CD-ROM Image";filter="*.chd;*.cue;*.iso;*.toc;*.nrg;*.gdi";
                } else {
                    title="Select Hard Disk Image";filter="*.chd;*.hd;*.hdv;*.2mg;*.hdi";
                }
                const juce::File initialDir = (processor.lastMediaFolder.isNotEmpty()
                    && juce::File(processor.lastMediaFolder).exists())
                    ? juce::File(processor.lastMediaFolder)
                    : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
                editor->fileChooser=std::make_unique<juce::FileChooser>(title,initialDir,filter);
                const auto flags=juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles;
                editor->fileChooser->launchAsync(flags,[safeThis,result](const juce::FileChooser& fc) {
                    if (safeThis == nullptr) return;
                    auto* e=safeThis.getComponent();
                    const auto file=fc.getResult();
                    if (!file.existsAsFile()) return;
                    if (result==20) {
                        e->importWavFile(file);
                        return;
                    }
                    auto& p=e->audioProcessor;
                    p.lastMediaFolder=file.getParentDirectory().getFullPathName();
                #ifdef _WIN32
                    std::string path=file.getFullPathName().toUTF8().getAddress();
                #else
                    std::string path=file.getFullPathName().toStdString();
                #endif
                    {
                        std::lock_guard<std::mutex> lock(p.mediaMutex);
                        if (result==2) {
                            p.pendingFloppyPath=path;p.loadedFloppyName=file.getFileName();
                            p.isFloppyLoaded.store(true,std::memory_order_release);
                        } else if (result==6) {
                            p.pendingCdPath=path;p.loadedCdName=file.getFileName();
                            p.isCdLoaded.store(true,std::memory_order_release);
                        } else {
                            p.pendingHddPath=path;p.loadedHddName=file.getFileName();
                            p.isHddLoaded.store(true,std::memory_order_release);
                        }
                    }
                    e->saveGlobalSettings();
                    if (result==2) p.requestFloppyLoad.store(true,std::memory_order_release);
                    else {p.shutdownMame();p.checkRomAndBootMame();}
                    e->repaint();
                });
                return;
            }
            if (result==4) {
                {std::lock_guard<std::mutex> lock(processor.mediaMutex);
                 processor.pendingFloppyPath="";processor.loadedFloppyName="";
                 processor.isFloppyLoaded.store(false,std::memory_order_release);}
                processor.requestFloppyLoad.store(true,std::memory_order_release);
            } else if (result==7) {
                {std::lock_guard<std::mutex> lock(processor.mediaMutex);
                 processor.pendingCdPath="";processor.loadedCdName="";
                 processor.isCdLoaded.store(false,std::memory_order_release);}
                editor->saveGlobalSettings();
                processor.shutdownMame();processor.checkRomAndBootMame();
            } else if (result==9) {
                {std::lock_guard<std::mutex> lock(processor.mediaMutex);
                 processor.pendingHddPath="";processor.loadedHddName="";
                 processor.isHddLoaded.store(false,std::memory_order_release);}
                editor->saveGlobalSettings();
                processor.shutdownMame();processor.checkRomAndBootMame();
            }
            editor->repaint();
        });
}

void EnsoniqSD1AudioProcessorEditor::toggleSettings()
{
    isSettingsVisible = !isSettingsVisible;
    settingsGroup.setVisible(isSettingsVisible);
    bufferLabel.setVisible(isSettingsVisible);
    bufferCombo.setVisible(isSettingsVisible);
    viewLabel.setVisible(isSettingsVisible);
    viewCombo.setVisible(isSettingsVisible);
    aboutLabel.setVisible(isSettingsVisible);
    webLink.setVisible(isSettingsVisible);
    closeSettingsButton.setVisible(isSettingsVisible);
    loadMediaButton.setEnabled(!isSettingsVisible);
    saveBadge.setEnabled(!isSettingsVisible);
    if (isSettingsVisible) resized();  // reposition all panel elements for the current window size
    repaint();
}

void EnsoniqSD1AudioProcessorEditor::updateWindowSize()
{
    int viewIdx = audioProcessor.requestedViewIndex.load(std::memory_order_acquire);
    int baseW = 2048;
    int baseH = 1744;
    if (viewIdx == 1) baseH = 671;
    else if (viewIdx == 2) baseH = 379;
    else if (viewIdx == 3) baseH = 1476;
    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
    else if (viewIdx == 4) baseH = 925 + 379;
#endif // SD1_DEBUG_RACK_PANEL
    
    float aspect = MPC_VIEW_W / MPC_VIEW_H;
    
    // --- RESTORE CORRECT TARGET WIDTH BASED ON VIEW ---
    int targetW = 1200;
    if (viewIdx == 4) {
        targetW = audioProcessor.fileManagerState.fmWindowWidth;
    } else {
        targetW = audioProcessor.savedWindowWidth;
    }
    
    if (targetW < 900) {
        targetW = 1200;
    }
    int targetH = juce::roundToInt(targetW / aspect);
    
    if (getWidth() == targetW && getHeight() == targetH) {
        return;
    }
    
    getConstrainer()->setFixedAspectRatio(aspect);
    getConstrainer()->setSizeLimits(900, juce::roundToInt(900.0f / aspect), 2400, juce::roundToInt(2400.0f / aspect));

    juce::MessageManager::callAsync([this, targetW, targetH]() {
        setSize(targetW, targetH);
    });
}

void EnsoniqSD1AudioProcessorEditor::saveGlobalSettings()
{
    // Inter-process lock prevents two plugin instances from doing concurrent
    // read-modify-write on settings.xml (which can erase attributes when the
    // parse fails and a fresh empty XmlElement is written instead).
    juce::InterProcessLock settingsLock("S3000XL_Settings_Lock");
    if (!settingsLock.enter(200)) return;  // skip this save if another instance is holding the lock

    juce::File docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File settingsDir = docsDir.getChildFile("S3000XL");
    if (!settingsDir.exists()) settingsDir.createDirectory();

    juce::File settingsFile = settingsDir.getChildFile("settings.xml");
    
    std::unique_ptr<juce::XmlElement> xml;
    for (int i = 0; i < 5; ++i) {
        if (settingsFile.existsAsFile()) {
            xml = juce::XmlDocument::parse(settingsFile);
            if (xml != nullptr) break;
        } else {
            break;
        }
        juce::Thread::sleep(10);
    }
    
    if (xml == nullptr || !xml->hasTagName("S3000XLSettings")) {
        xml = std::make_unique<juce::XmlElement>("S3000XLSettings");
    }

    if (audioProcessor.lastBrowsedFolder.isNotEmpty()) xml->setAttribute("last_browsed_folder", audioProcessor.lastBrowsedFolder);
    if (audioProcessor.lastMediaFolder.isNotEmpty()) xml->setAttribute("last_media_folder", audioProcessor.lastMediaFolder);
    if (audioProcessor.lastRomFolder.isNotEmpty()) xml->setAttribute("last_rom_folder", audioProcessor.lastRomFolder);
    if (audioProcessor.myComputerPath.isNotEmpty()) xml->setAttribute("my_computer_path", audioProcessor.myComputerPath);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(audioProcessor.apvts.getParameter("buffer_size")))
        xml->setAttribute("buffer_size", p->getIndex());

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(audioProcessor.apvts.getParameter("layout_view")))
        xml->setAttribute("layout_view", p->getIndex());

    xml->setAttribute("window_width", audioProcessor.savedWindowWidth);
    xml->setAttribute("window_height", audioProcessor.savedWindowHeight);
    
    xml->setAttribute("fm_window_width", audioProcessor.fileManagerState.fmWindowWidth);
    xml->setAttribute("fm_window_height", audioProcessor.fileManagerState.fmWindowHeight);
    xml->setAttribute("welcome_shown", !audioProcessor.showWelcomeMessage.load(std::memory_order_acquire));
    xml->setAttribute("rom_path", audioProcessor.customRomPath);

    // S3000XL SCSI media paths (persist HDD image + CD image globally)
    {
        std::lock_guard<std::mutex> lock(audioProcessor.mediaMutex);
        xml->setAttribute("scsi_hdd_path", juce::String(audioProcessor.pendingHddPath));
        xml->setAttribute("scsi_cd_path",  juce::String(audioProcessor.pendingCdPath));
    }
    
    for (int i = 0; i < 10; ++i) xml->removeAttribute("bookmark_" + juce::String(i));
    
    for (int i = 0; i < audioProcessor.bookmarkFolders.size() && i < 10; ++i)
        xml->setAttribute("bookmark_" + juce::String(i), audioProcessor.bookmarkFolders[i]);
    
    juce::File tempFile = settingsFile.getSiblingFile("settings.xml.tmp");
    if (xml->writeTo(tempFile)) {
        tempFile.moveFileTo(settingsFile); 
    }
    
    settingsLock.exit();
}

void EnsoniqSD1AudioProcessorEditor::flushFileManagerState()
{
    presetBrowser.saveStateToProcessor();
}

void EnsoniqSD1AudioProcessorEditor::locateRomButtonClicked()
{
    
    juce::File initialDir = audioProcessor.lastRomFolder.isNotEmpty()
            ? juce::File(audioProcessor.lastRomFolder)
            : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    
    romChooser = std::make_unique<juce::FileChooser>("Select ROM .zip files, or the folder containing the .bin files",
            initialDir,
            "");

    auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::canSelectDirectories |
                              juce::FileBrowserComponent::canSelectMultipleItems; // Enable multi-selection

    romChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) {
        auto files = fc.getResults(); // Fetch array of selected files/folders
        bool show7zWarning = false;
        
        if (!files.isEmpty()) {
            
            // SAVE: Update the ROM folder in the XML
            audioProcessor.lastRomFolder = files[0].getParentDirectory().getFullPathName();
            saveGlobalSettings();
            
            for (const auto& file : files) {
                if (file.exists()) {
                    if (file.isDirectory()) {
                        // 1. User selected a folder: copy loose .bin files (this explicitly looks for the 10 files only)
                        audioProcessor.copyRomsFromFolder(file);
                        
                        // 2. Specifically look for our target zip files (instant O(1) file system check)
                        juce::File zip1 = file.getChildFile("sd132.zip");
                        if (zip1.existsAsFile()) audioProcessor.extractRomsFromZip(zip1);
                        
                        juce::File zip2 = file.getChildFile("esq2x40_vfx.zip");
                        if (zip2.existsAsFile()) audioProcessor.extractRomsFromZip(zip2);
                        
                        // 3. Check for specific .7z files to warn the user
                        if (file.getChildFile("sd132.7z").existsAsFile() || file.getChildFile("esq2x40_vfx.7z").existsAsFile()) {
                            show7zWarning = true;
                        }
                        
                    } else if (file.hasFileExtension(".7z")) {
                        // User explicitly selected a .7z file
                        show7zWarning = true;
                    } else if (file.hasFileExtension(".zip")) {
                        // User selected a zip (they manually selected it, so we extract it regardless of the name)
                        audioProcessor.extractRomsFromZip(file);
                    } else if (file.hasFileExtension(".bin")) {
                        // User clicked a specific .bin file: grab the parent folder and copy all matches from there
                        audioProcessor.copyRomsFromFolder(file.getParentDirectory());
                    }
                }
            }
            
            // Re-evaluate and boot MAME if all 10 are found
            audioProcessor.checkRomAndBootMame();
            
            // Show a friendly warning if .7z files were detected
            if (show7zWarning) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Unsupported Archive Format",
                    "It looks like you selected a '.7z' file.\n\nThe plugin cannot extract .7z archives directly. Please either extract it manually using a tool like 7-Zip/Keka etc., or download the '.zip' versions of the ROMs instead.");
            }
         
            repaint();
        }
    });
}
