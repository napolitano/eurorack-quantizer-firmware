/**
 * @file FirmwareController.cpp
 * Wires the portable firmware core to the ATmega328P hardware and scheduler.
 *
 * @author Axel Napolitano
 * @note Original FM Quantizer concept and Rust firmware by Quinn Freedman.
 * @copyright Copyright (C) 2026 Axel Napolitano
 * @license GPL-3.0-or-later
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <Arduino.h>
#include <SPI.h>
#include <avr/interrupt.h>
#include "platform/nano_atmega328p/FirmwareController.h"

#include "fmq/application/Button.h"
#include "fmq/config/FirmwareConfig.h"
#include "platform/nano_atmega328p/BoardConfig.h"
#include "fmq/application/ControlInputProcessor.h"
#include "fmq/ui/LedFrameEncoder.h"
#include "fmq/persistence/LiveStateStore.h"
#include "fmq/ui/Menu.h"
#include "fmq/domain/PitchConversion.h"
#include "fmq/domain/Quantizer.h"
#include "fmq/persistence/SaveSlotStore.h"
#include "fmq/persistence/StartupSequenceStore.h"
#include "fmq/application/StartupAnimation.h"
#include "fmq/application/RetroArpeggiator.h"
#include "platform/nano_atmega328p/AvrAnalogInputs.h"
#include "platform/nano_atmega328p/AvrDigitalInput.h"
#include "platform/nano_atmega328p/AvrTriggerInputs.h"
#include "platform/nano_atmega328p/RuntimeDiagnostics.h"
#include "platform/nano_atmega328p/CalibrationConsole.h"
#include "platform/nano_atmega328p/AvrDigitalOutput.h"
#include "platform/nano_atmega328p/AvrEeprom.h"
#include "platform/nano_atmega328p/AvrSpiBus.h"
#include "platform/nano_atmega328p/AvrSystemClock.h"
#include "platform/nano_atmega328p/Mcp4922Dac.h"
#include "platform/nano_atmega328p/Tlc5947LedDriver.h"

using namespace fmq;

using namespace fmq::hwconfig;
using fmq::platform::nano::FirmwareController;
using fmq::platform::nano::RuntimeDiagnostics;
using fmq::platform::nano::CalibrationConsole;

// --- 1 kHz sample tick ------------------------------------------------------
namespace {
constexpr uint16_t kTimer2Prescaler = 128;
constexpr uint32_t kTimer2CountsPerTick =
    F_CPU / kTimer2Prescaler / config::kControlLoopFrequencyHz;
constexpr uint8_t kTimer2CompareValue =
    static_cast<uint8_t>(kTimer2CountsPerTick - 1u);
static_assert(kTimer2CountsPerTick > 0u && kTimer2CountsPerTick <= 256u,
              "Timer2 control-loop configuration is out of range");
static_assert((F_CPU / kTimer2Prescaler) % config::kControlLoopFrequencyHz == 0u,
              "Timer2 control-loop frequency must divide exactly");
constexpr uint16_t kInvalidDacCode = config::kDacMaximumCode + 1u;
}

static volatile uint8_t g_pendingSamples = 0;

/// Timer2 compare-match ISR: fires at 1 kHz and flags a new sample period.
ISR(TIMER2_COMPA_vect) {
  if (g_pendingSamples != static_cast<uint8_t>(0xFFu)) {
    ++g_pendingSamples;
  }
}
ISR(ADC_vect) { AvrAnalogInputs::handleInterrupt(); }

/// Configure Timer2 in CTC mode for the configured control-loop frequency.
static void configureSampleTimer() {
  TCCR2A = _BV(WGM21);              // CTC: clear timer on compare match
  TCCR2B = _BV(CS22) | _BV(CS20);  // prescaler 128
  OCR2A = kTimer2CompareValue;
  TIMSK2 = _BV(OCIE2A);            // enable compare-match A interrupt
}

// --- Hardware and core objects ---------------------------------------------
static AvrSystemClock g_clock;
static AvrEeprom g_eeprom;
static AvrSpiBus g_spi;
static AvrAnalogInputs g_analog(kPinButtonLadder, kPinCvInputA, kPinCvInputB);

static AvrDigitalOutput g_dacCs(kPinDacChipSelect, /*initialHigh=*/true);
static AvrDigitalOutput g_ledCs(kPinLedChipSelect, /*initialHigh=*/true);
static AvrDigitalOutput g_ledBlank(kPinLedBlank, /*initialHigh=*/true);
static AvrDigitalOutput g_trigOutA(kPinTrigOutputA);
static AvrDigitalOutput g_trigOutB(kPinTrigOutputB);
static AvrDigitalOutput g_inputLedA(kPinInputLedA);
static AvrDigitalOutput g_outputLedA(kPinOutputLedA);
static AvrDigitalOutput g_inputLedB(kPinInputLedB);
static AvrDigitalOutput g_outputLedB(kPinOutputLedB);

static AvrDigitalInput g_shift(kPinShift, kButtonsUsePullups);
static AvrDigitalInput g_save(kPinSave, kButtonsUsePullups);
static AvrDigitalInput g_load(kPinLoad, kButtonsUsePullups);
static AvrTriggerInputs g_triggerInputs(
    kPinTrigInputA, kPinTrigInputB, kTriggerInputsActiveHigh,
    kTriggerInputsUsePullups);

static Mcp4922Dac g_dac(g_spi, g_dacCs);
static Tlc5947LedDriver g_ledDriver(g_spi, g_ledCs, g_ledBlank);

static QuantizerState g_quantizer;
static AsyncEepromWriter g_eepromWriter(g_eeprom);
static SaveSlotStore g_slotStore(g_eeprom, g_eepromWriter);
static LiveStateStore g_liveStore(g_eeprom, g_eepromWriter);
static StartupSequenceStore g_startupSequenceStore(g_eeprom, g_eepromWriter);
static Menu g_menu;
static RetroArpeggiator g_retroArpeggiator;

constexpr uint32_t kNoRetroArpHold = 0xFFFFFFFFu;
static uint32_t g_retroArpHoldStartMs = kNoRetroArpHold;
static bool g_retroArpHoldConsumed = false;
static uint32_t g_retroArpFeedbackUntilMs = 0u;
static LedColor g_retroArpFeedbackColor = LedColor::Off;

static ControlInputProcessor g_controls(config::kDigitalDebounceMs, config::kLongPressMs);

// Cached outputs so we only touch the SPI bus when something actually changes.
static LedFrame g_lastFrame;
static uint16_t g_lastDacCodeA = kInvalidDacCode;
static uint16_t g_lastDacCodeB = kInvalidDacCode;

// Live-autosave bookkeeping.
static bool g_liveDirty = false;
static uint32_t g_lastChangeMs = 0;
static RuntimeDiagnostics g_diagnostics;
static uint8_t g_controlTick = 0;
static MenuOutput g_lastMenuOutput{};

static bool isButtonCurrentlyDown(LongPressButtonState state) {
  return state != LongPressButtonState::ButtonIsUp &&
         state != LongPressButtonState::ButtonJustClickedShort &&
         state != LongPressButtonState::ButtonJustReleasedLong;
}

static void updateRetroArpGesture(const MenuInput &input, uint32_t nowMs) {
  if (g_menu.inCalibration()) {
    g_retroArpHoldStartMs = kNoRetroArpHold;
    g_retroArpHoldConsumed = false;
    return;
  }

  const bool noteButtonActive = input.keyEvent.type != ButtonEventType::None;
  const bool companionControlActive =
      noteButtonActive || isButtonCurrentlyDown(input.saveButton) ||
      isButtonCurrentlyDown(input.loadButton);

  if (!input.shiftPressed) {
    g_retroArpHoldStartMs = kNoRetroArpHold;
    g_retroArpHoldConsumed = false;
    return;
  }

  if (companionControlActive) {
    // Once SHIFT has participated in a normal shortcut, require a complete
    // release before another SHIFT-alone Retro Arpeggiator hold can begin.
    g_retroArpHoldStartMs = kNoRetroArpHold;
    g_retroArpHoldConsumed = true;
    return;
  }

  if (g_retroArpHoldConsumed) {
    return;
  }
  if (g_retroArpHoldStartMs == kNoRetroArpHold) {
    g_retroArpHoldStartMs = nowMs;
    return;
  }
  if (nowMs - g_retroArpHoldStartMs < config::kRetroArpToggleHoldMs) {
    return;
  }

  g_retroArpeggiator.toggle(nowMs);
  g_retroArpHoldConsumed = true;
  g_retroArpFeedbackUntilMs = nowMs + config::kRetroArpFeedbackMs;
  g_retroArpFeedbackColor =
      g_retroArpeggiator.enabled() ? LedColor::Amber : LedColor::Red;
}

static LedFrame retroArpFeedbackFrame() {
  LedFrame frame;
  for (uint8_t note = 0; note < kNoteCount; ++note) {
    frame[note] = g_retroArpFeedbackColor;
  }
  return frame;
}

/// Encode and send a note-LED frame using the currently active brightness.
static void renderNoteLeds(const LedFrame &frame, bool force) {
  // During calibration the brightness (not just the colours) changes, so the
  // frame must be resent even when the colours are identical.
  if (!force && frame == g_lastFrame) {
    return;
  }
  const BrightnessCalibration &cal = g_menu.activeBrightness();
  uint8_t bytes[kTlc5947FrameBytes];
  encodeLedFrame(frame, cal.redLevel(), cal.greenLevel(), bytes);
  g_ledDriver.writeFrame(bytes);
  g_lastFrame = frame;
}

/// Play the selected note-ring sequence followed by the four discrete LEDs.
static void playStartupAnimation() {
  if (config::kStartupAnimationEnabled) {
    const uint8_t sequenceIndex = config::kRotateStartupSequences
                                      ? g_startupSequenceStore.loadSequenceToPlay(
                                            StartupAnimation::kSequenceCount)
                                      : 0u;
    const StartupSequence sequence =
        static_cast<StartupSequence>(sequenceIndex);
    StartupAnimation animation(sequence);
    const BrightnessCalibration &brightness = g_menu.activeBrightness();
    const uint32_t animationStartMs = g_clock.millis();

    while (true) {
      const uint32_t elapsedMs = g_clock.millis() - animationStartMs;
      if (animation.isDone(elapsedMs)) {
        break;
      }

      const StartupAnimationSample sample = animation.sampleAt(elapsedMs);
      uint8_t bytes[kTlc5947FrameBytes];
      encodeLedFrameScaled(sample.frame, brightness.redLevel(),
                           brightness.greenLevel(), sample.intensityQ12, bytes);
      g_ledDriver.writeFrame(bytes);
      delay(1);
    }

    renderNoteLeds(LedFrame(), /*force=*/true);

    if (config::kRotateStartupSequences) {
      const uint8_t nextSequence = static_cast<uint8_t>(
          (sequenceIndex + 1u) % StartupAnimation::kSequenceCount);
      // This queues one EEPROM byte. The shared writer flushes it without
      // blocking once the real-time loop starts.
      (void)g_startupSequenceStore.storeNextSequence(nextSequence);
    }
  }

  if (config::kStartupStatusLedTestEnabled) {
    AvrDigitalOutput *statusLeds[] = {
        &g_inputLedA, &g_outputLedA, &g_inputLedB, &g_outputLedB};

    for (AvrDigitalOutput *statusLed : statusLeds) {
      statusLed->set(kStatusLedsActiveHigh);
      delay(config::kStatusLedOnMs);
      statusLed->set(!kStatusLedsActiveHigh);
      delay(config::kStatusLedOffMs);
    }
  }
}

void FirmwareController::begin() {
  // GPIO initialisation belongs here, after the Arduino core is ready. Global
  // constructors only store pin metadata.
  g_dacCs.begin();
  g_ledCs.begin();
  g_ledBlank.begin();
  g_trigOutA.begin();
  g_trigOutB.begin();
  g_inputLedA.begin();
  g_outputLedA.begin();
  g_inputLedB.begin();
  g_outputLedB.begin();
  g_shift.begin();
  g_save.begin();
  g_load.begin();
  g_triggerInputs.begin();
  g_trigOutA.set(!kTriggerOutputsActiveHigh);
  g_trigOutB.set(!kTriggerOutputsActiveHigh);
  g_inputLedA.set(!kStatusLedsActiveHigh);
  g_outputLedA.set(!kStatusLedsActiveHigh);
  g_inputLedB.set(!kStatusLedsActiveHigh);
  g_outputLedB.set(!kStatusLedsActiveHigh);

  // Effective Rust behaviour is AVCC: fm-lib::init_async_adc() overwrites the
  // earlier Aref constructor setting with ADMUX.REFS=AVCC. The board profile
  // reflects that effective behaviour.
  analogReference(kUseExternalAref ? EXTERNAL : DEFAULT);

  if (config::kDiagnosticsEnabled) Serial.begin(config::kCalibrationBaud);
  g_spi.begin();
  g_ledDriver.begin();  // clear LEDs and enable the driver outputs

  // Match the Rust startup baseline by default: it only inspects save-slot
  // occupancy and does not restore the running state. Live restore remains an
  // opt-in C++ extension.
  if (config::kRestoreLiveStateOnBoot) {
    LiveState restored;
    if (g_liveStore.load(restored)) {
      g_quantizer = restored.state;
      g_menu.setCommittedBrightness(restored.brightness);
    }
  }
  g_menu.begin(g_slotStore);

  playStartupAnimation();

  // Start the interrupt-driven ADC after the blocking LED self-test.
  g_analog.begin(kUseExternalAref);

  // Holding SHIFT during power-on enters a deliberately explicit calibration
  // console. It exposes raw ADC readings and raw DAC codes; it never guesses
  // electrical calibration values.
  delay(config::kStartupInputSettleMs);
  const bool shiftPressedAtBoot = kButtonsActiveLow ? !g_shift.isHigh() : g_shift.isHigh();
  if (config::kCalibrationConsoleEnabled && shiftPressedAtBoot) {
    CalibrationConsole console(g_analog, g_dac);
    console.run();
  }

  // Measure a series of fresh, unpressed ladder snapshots. A timeout or an
  // implausible/stable-high-button value is made visible instead of silently
  // accepting a bad calibration.
  uint32_t ladderSum = 0;
  uint16_t ladderMin = config::kAdcMaximumCode;
  uint16_t ladderMax = 0;
  uint8_t samples = 0;
  const uint32_t calibrationStart = g_clock.millis();
  while (samples < config::kLadderCalibrationSamples &&
         g_clock.millis() - calibrationStart < config::kLadderCalibrationTimeoutMs) {
    if (!g_analog.sampleReady()) continue;
    g_analog.beginCycle();
    const uint16_t value = g_analog.read(0);
    ladderSum += value;
    if (value < ladderMin) ladderMin = value;
    if (value > ladderMax) ladderMax = value;
    ++samples;
  }
  bool ladderCalibrationOk = false;
  if (samples == config::kLadderCalibrationSamples) {
    const uint16_t ladderMean = static_cast<uint16_t>(ladderSum / samples);
    ladderCalibrationOk = ladderMax - ladderMin <= config::kLadderRestStabilitySpan &&
                          ladderMean >= config::kLadderMinimumValidRest &&
                          ladderMean <= config::kLadderMaximumValidRest;
    if (ladderCalibrationOk) g_controls.calibrateLadderRest(ladderMean);
  }
  if (!ladderCalibrationOk) {
    LedFrame failure; for (uint8_t i=0;i<kNoteCount;++i) failure[i]=LedColor::Red;
    uint8_t bytes[kTlc5947FrameBytes];
    encodeLedFrame(failure, config::kDefaultRedPwm, 0, bytes);
    for (uint8_t flash = 0; flash < config::kStartupErrorFlashCount; ++flash) {
      g_ledDriver.writeFrame(bytes);
      delay(config::kStartupErrorOnMs);
      renderNoteLeds(LedFrame(), /*force=*/true);
      delay(config::kStartupErrorOffMs);
    }
  }

  // Start real-time processing only after all blocking startup work.
  configureSampleTimer();
  g_pendingSamples = 0;
  sei();
  g_lastChangeMs = g_clock.millis();
}

void FirmwareController::run() {
  cli();
  const uint8_t pending = g_pendingSamples;
  g_pendingSamples = 0;
  sei();
  if (pending == 0) {
    g_eepromWriter.service();
    g_liveStore.observeWriter();
    return;
  }
  g_diagnostics.observeQueue(pending);
  ++g_diagnostics.ticks;

  const uint32_t now = g_clock.millis();
  if (g_analog.sampleReady()) g_analog.beginCycle(); else ++g_diagnostics.adcStaleTicks;
  const uint16_t ladderValue = g_analog.read(0);
  const uint16_t cvA = g_analog.read(1);
  const uint16_t cvB = g_analog.read(2);

  // External interrupts latch sub-millisecond rising edges. The current gate
  // level is ORed with the latch, preserving Track-and-Hold gate semantics.
  const bool triggerEdgeA = g_triggerInputs.consumeActivationA();
  const bool triggerEdgeB = g_triggerInputs.consumeActivationB();
  const bool trigA = g_triggerInputs.levelA() || triggerEdgeA;
  const bool trigB = g_triggerInputs.levelB() || triggerEdgeB;
  const QuantizationResult result = g_quantizer.step(
      adcToSemitones(cvA, 0), adcToSemitones(cvB, 1), trigA, trigB);

  ++g_controlTick;
  if (g_controlTick % config::kUiDivider == 0) {
    const RawControlInput rawInput = {
        ladderValue,
        kButtonsActiveLow ? !g_shift.isHigh() : g_shift.isHigh(),
        kButtonsActiveLow ? !g_save.isHigh() : g_save.isHigh(),
        kButtonsActiveLow ? !g_load.isHigh() : g_load.isHigh()};
    const MenuInput input = g_controls.sample(now, rawInput);
    updateRetroArpGesture(input, now);
    g_lastMenuOutput = g_menu.update(g_quantizer, input, result, now, g_slotStore);
    ++g_diagnostics.uiRuns;
    if (g_lastMenuOutput.persistentStateChanged) {
      // A front-panel configuration edit can move the quantized pitch on the
      // next sample. Do not turn that UI action into a trigger pulse/LED flash.
      g_quantizer.suppressNextOutputTriggers();
      g_liveDirty = true;
      g_lastChangeMs = now;
    }
  }

  const SemitoneQ8_8 outputPitchA = g_retroArpeggiator.process(
      result.channelA.actualSemitones, result.channelA.nominalSemitones,
      g_quantizer.channels[kChannelAIndex].config().notes, now);
  const SemitoneQ8_8 outputPitchB = g_retroArpeggiator.process(
      result.channelB.actualSemitones, result.channelB.nominalSemitones,
      g_quantizer.channels[kChannelBIndex].config().notes, now);

  const uint16_t dacCodeA = semitonesToDac(outputPitchA, kChannelAIndex);
  const uint16_t dacCodeB = semitonesToDac(outputPitchB, kChannelBIndex);
  if (dacCodeA != g_lastDacCodeA) {
    g_dac.write(Mcp4922Dac::Channel::A, dacCodeA);
    g_lastDacCodeA = dacCodeA;
  }
  if (dacCodeB != g_lastDacCodeB) {
    g_dac.write(Mcp4922Dac::Channel::B, dacCodeB);
    g_lastDacCodeB = dacCodeB;
  }

  g_trigOutA.set(result.channelA.outputTrigger == kTriggerOutputsActiveHigh);
  g_trigOutB.set(result.channelB.outputTrigger == kTriggerOutputsActiveHigh);
  g_outputLedA.set(result.channelA.outputTriggerUi == kStatusLedsActiveHigh);
  g_outputLedB.set(result.channelB.outputTriggerUi == kStatusLedsActiveHigh);
  g_inputLedA.set(result.channelA.inputTriggerUi == kStatusLedsActiveHigh);
  g_inputLedB.set(result.channelB.inputTriggerUi == kStatusLedsActiveHigh);

  if (g_controlTick % config::kLedDivider == 0 || g_menu.inCalibration()) {
    const bool showRetroArpFeedback = now < g_retroArpFeedbackUntilMs;
    const LedFrame displayFrame =
        showRetroArpFeedback ? retroArpFeedbackFrame() : g_lastMenuOutput.frame;
    renderNoteLeds(displayFrame, g_menu.inCalibration() || showRetroArpFeedback);
    ++g_diagnostics.ledWrites;
  }

  // EEPROM progress is serviced in both idle loops and control ticks. Each call
  // advances at most one byte, so no control deadline is monopolised.
  g_eepromWriter.service();
  g_liveStore.observeWriter();
  if (config::kAutosaveLiveState && g_liveDirty && !g_eepromWriter.busy() &&
      now - g_lastChangeMs >= config::kLiveAutosaveQuiescenceMs) {
    if (g_liveStore.commit(g_quantizer, g_menu.committedBrightness())) {
      g_liveDirty = false;
    }
  }
  g_diagnostics.report(now);
}
