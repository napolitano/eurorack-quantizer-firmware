(function (global) {
  "use strict";

  var ASSET = "../../manual/assets/calibration/";

  function checks(items) {
    return items.map(function (item, index) {
      return { id: item.id || ("check-" + index), label: item.label, required: item.required !== false };
    });
  }

  function adcSteps(channel) {
    var positive = [1, 3, 5, 7, 9];
    var verify = [0, 2, 4, 6, 8];
    return positive.concat(verify).map(function (voltage, index) {
      var isZero = voltage === 0;
      var fit = index < positive.length;
      return {
        id: "adc-" + channel.toLowerCase() + "-" + voltage + "v",
        phase: "Analog calibration",
        title: "ADC " + channel + " — " + voltage.toFixed(3) + " V " + (fit ? "fit" : "verification"),
        summary: isZero
          ? "Use the module's local ground reference. The bench PSU must be disconnected."
          : "Adjust the bench source until the DMM at the actual jack adapter reads the requested voltage, then press SAVE on the Quantizer.",
        illustration: ASSET + (isZero ? (channel === "A" ? "03-adc-zero-channel-a.svg" : "04-adc-zero-channel-b.svg") : (channel === "A" ? "01-adc-reference-channel-a.svg" : "02-adc-reference-channel-b.svg")),
        type: isZero ? "zeroReference" : "adcReference",
        channel: channel,
        target: voltage,
        role: fit ? "Fit point" : (isZero ? "Raw zero sanity" : "Independent verification"),
        alerts: isZero ? [{ level: "danger", title: "0 V reference", text: "Disconnect the external voltage source. Short TIP directly to SLEEVE/GND. Do not generate the 0 V reference by setting the bench PSU to 0.000 V." }] : [{ level: "important", title: "Measurement rule", text: "The PSU is the source; the DMM is the reference." }],
        checks: isZero ? checks([
          { id: "psu-disconnected", label: "Bench PSU output and leads are disconnected from the adapter." },
          { id: "tip-sleeve-short", label: "TIP is directly shorted to SLEEVE/GND." },
          { id: "correct-input", label: "The patch cable is connected to CV IN " + channel + "." },
          { id: "saved", label: "I pressed SAVE only after the 0 V reference was correctly connected." }
        ]) : checks([
          { id: "dmm-at-adapter", label: "DMM is measuring TIP relative to SLEEVE at the actual jack adapter." },
          { id: "correct-input", label: "The adapter is connected to CV IN " + channel + "." },
          { id: "dmm-correct", label: "The DMM reads the requested reference voltage." },
          { id: "saved", label: "I pressed SAVE on the Quantizer after the DMM reading was correct." }
        ])
      };
    });
  }

  var base = {
    welcome: {
      id: "welcome", phase: "Start", type: "flowChoice", title: "Choose a calibration workflow",
      summary: "The complete procedure starts with the development environment and maintenance-firmware build, then guides every power transition, calibration step, measurement and production reflash.",
      alerts: [{ level: "important", title: "Scope", text: "The web guide is an assistant. It cannot detect cabling, execute PlatformIO, operate the Quantizer, or verify that power is actually safe." }]
    },
    equipment: {
      id: "equipment", phase: "Preparation", type: "checks", title: "Prepare the calibration bench",
      summary: "Have all required equipment ready before flashing the maintenance image.",
      illustration: ASSET + "00-calibration-bench-layout.svg",
      checks: checks([
        { id: "dmm", label: "Bench DMM with trustworthy DC accuracy and millivolt resolution." },
        { id: "psu", label: "Adjustable low-noise DC source / bench PSU capable of 0…9 V." },
        { id: "eurorack", label: "Normal Eurorack +12 V / -12 V / +5 V / GND power available." },
        { id: "adapter", label: "3.5 mm TS breakout/helper adapter or secure direct TIP/SLEEVE test access." },
        { id: "patch", label: "Patch cables for loopback calibration." },
        { id: "probes", label: "Suitable DMM/PSU probes, clips or banana leads." },
        { id: "usb", label: "USB cable and computer with PlatformIO." },
        { id: "driver", label: "Insulated trimmer screwdriver if RAW hardware pre-trim will be used.", required: false }
      ])
    },
    adapter: {
      id: "adapter", phase: "Preparation", type: "checks", title: "Verify the TIP / SLEEVE adapter",
      summary: "A passive helper PCB is recommended for mechanically secure access to the patch cable conductors.",
      alerts: [{ level: "important", title: "Wiring identity", text: "TIP must be the CV signal conductor. SLEEVE must be module GND / return. Reversed or ambiguous wiring invalidates the measurement setup." }],
      checks: checks([
        { id: "tip", label: "TIP terminal/test point has been identified and continuity-checked." },
        { id: "sleeve", label: "SLEEVE/GND terminal/test point has been identified and continuity-checked." },
        { id: "secure", label: "DMM and PSU leads can be attached without loose conductors or accidental shorts." },
        { id: "zero", label: "A safe way to short TIP directly to SLEEVE is available for the 0 V reference." }
      ])
    },
    bootloader: {
      id: "bootloader", phase: "Build", type: "bootloader", title: "Select the Nano bootloader",
      summary: "The project provides separate PlatformIO environments for current and old Arduino Nano bootloaders. This selection controls every build and upload command in the guide."
    },
    environment: {
      id: "environment", phase: "Build", type: "checks", title: "Check the development environment",
      summary: "Run these checks from the repository root before compiling.",
      command: "pio --version",
      secondaryCommand: "pio project config",
      checks: checks([
        { id: "repo-root", label: "Terminal working directory is the repository root containing platformio.ini." },
        { id: "pio", label: "PlatformIO Core is installed and `pio --version` succeeds." },
        { id: "config", label: "`pio project config` reads this project's platformio.ini without error." }
      ])
    },
    buildCalibration: {
      id: "build-calibration", phase: "Build", type: "command", commandKind: "buildCalibration", title: "Compile the maintenance firmware",
      summary: "Compile first. Do not connect the module merely to discover a build problem.",
      expected: "PlatformIO finishes with SUCCESS for the selected calibration environment."
    },
    powerOffBeforeUsb: {
      id: "power-off-before-usb", phase: "Flash maintenance firmware", type: "checks", title: "Make the module safe for USB flashing",
      summary: "The maintenance image must be flashed with the Eurorack ribbon physically disconnected.",
      alerts: [{ level: "danger", title: "Do not dual-power the module", text: "Never connect normal Eurorack bus power and the Nano USB connection at the same time. The Eurorack +5 V rail and USB-derived +5 V must not drive the module simultaneously." }],
      checks: checks([
        { id: "case-off", label: "Eurorack case is switched OFF." },
        { id: "ribbon-disconnected", label: "Eurorack ribbon is physically disconnected from the module." },
        { id: "usb-not-yet", label: "USB is not connected until the ribbon is confirmed disconnected." }
      ])
    },
    connectUsb: {
      id: "connect-usb", phase: "Flash maintenance firmware", type: "checks", title: "Connect USB only",
      summary: "With the Eurorack ribbon disconnected, connect the Nano to the computer by USB.",
      checks: checks([
        { id: "ribbon-still-out", label: "Eurorack ribbon remains disconnected." },
        { id: "usb-connected", label: "USB cable is connected to the Nano and computer." }
      ])
    },
    uploadCalibration: {
      id: "upload-calibration", phase: "Flash maintenance firmware", type: "command", commandKind: "uploadCalibration", title: "Upload the maintenance firmware",
      summary: "Use a normal bootloader upload. Do not substitute a chip erase or EEPROM programming operation.",
      alerts: [{ level: "caution", title: "Preserve EEPROM", text: "A normal bootloader upload writes program flash only. Do not use chip-erase or EEPROM-upload operations unless calibration/preset loss is explicitly intended." }],
      expected: "PlatformIO / avrdude completes and reports SUCCESS."
    },
    maintenanceBootUsb: {
      id: "maintenance-boot-usb", phase: "Flash maintenance firmware", type: "checks", title: "Confirm maintenance firmware boot",
      summary: "The maintenance firmware starts with three amber full-ring flashes and then remains dim amber. During USB-only operation the analog ±12 V stages are intentionally not powered.",
      checks: checks([
        { id: "amber-boot", label: "I observed three amber full-ring flashes." },
        { id: "amber-idle", label: "The ring is now dim amber / idle." }
      ])
    },
    transitionToEurorack: {
      id: "transition-to-eurorack", phase: "Power transition", type: "checks", title: "Transition from USB to normal Eurorack power",
      summary: "Analog calibration must run from the module's normal rails.",
      alerts: [{ level: "danger", title: "Strict power transition", text: "Disconnect USB completely before reconnecting the Eurorack ribbon. Do not use the web guide as proof that the cable is disconnected." }],
      checks: checks([
        { id: "usb-out", label: "USB cable is completely disconnected from the Nano." },
        { id: "case-off", label: "Eurorack case is OFF while the ribbon is connected." },
        { id: "orientation", label: "Ribbon orientation has been checked." },
        { id: "ribbon-in", label: "Normal Eurorack ribbon is connected." },
        { id: "case-on", label: "Eurorack case is ON; USB remains disconnected." },
        { id: "idle", label: "Maintenance firmware returns to dim-amber idle." }
      ])
    },
    rawChoice: {
      id: "raw-choice", phase: "Hardware pre-trim", type: "rawChoice", title: "Optional RAW hardware pre-trim",
      summary: "RAW mode exposes the physical DAC/reference/output-stage transfer without stored or factory correction. Use it when the A/B hardware gains are noticeably mismatched or on a new build when you want a good physical baseline.",
      alerts: [{ level: "important", title: "Calibration dependency", text: "Any movement of RV1/RV2 invalidates the previous DAC calibration. A DAC-only or full software calibration must follow." }],
      optional: true
    },
    enterRaw: {
      id: "enter-raw", phase: "Hardware pre-trim", type: "checks", title: "Enter RAW Output Test",
      summary: "From dim-amber idle, hold SHIFT for one second. If CAL is available, press SAVE + LOAD briefly to switch to RAW.",
      checks: checks([
        { id: "output-test", label: "I held SHIFT for 1 second and entered Output Test." },
        { id: "raw", label: "The selected note is amber, indicating RAW mode." }
      ])
    },
    pretrimA: {
      id: "raw-pretrim-a", phase: "Hardware pre-trim", type: "pretrim", channel: "A", illustration: ASSET + "05-raw-pretrim-channel-a.svg",
      title: "RAW hardware pre-trim — Channel A", summary: "Measure C1 and C9, adjust the matching trimmer in small increments, and use C5 as a midpoint cross-check."
    },
    pretrimB: {
      id: "raw-pretrim-b", phase: "Hardware pre-trim", type: "pretrim", channel: "B", illustration: ASSET + "06-raw-pretrim-channel-b.svg",
      title: "RAW hardware pre-trim — Channel B", summary: "Repeat the same gain-oriented span check for Channel B."
    },
    exitRaw: {
      id: "exit-raw", phase: "Hardware pre-trim", type: "checks", title: "Exit Output Test",
      summary: "Hold SHIFT for one second to return to dim-amber maintenance idle.",
      checks: checks([{ id: "idle", label: "Output Test has exited and the full ring is dim amber." }])
    },
    startFull: {
      id: "start-full", phase: "Software calibration", type: "checks", title: "Start full calibration",
      summary: "From dim-amber idle, press SAVE once. New coefficients remain uncommitted until the complete wizard passes.",
      checks: checks([{ id: "started", label: "I pressed SAVE and the maintenance firmware is prompting for Channel A ADC references." }])
    },
    dacA: {
      id: "dac-a", phase: "DAC self-calibration", type: "loopback", channel: "A", illustration: ASSET + "07-dac-selfcal-channel-a.svg",
      title: "DAC A self-calibration", summary: "When Channel A IN and OUT LEDs blink together, patch CV OUT A to CV IN A and press SAVE.",
      connections: [["CV OUT A", "CV IN A"]]
    },
    dacB: {
      id: "dac-b", phase: "DAC self-calibration", type: "loopback", channel: "B", illustration: ASSET + "08-dac-selfcal-channel-b.svg",
      title: "DAC B self-calibration", summary: "When Channel B IN and OUT LEDs blink together, patch CV OUT B to CV IN B and press SAVE.",
      connections: [["CV OUT B", "CV IN B"]]
    },
    dualLoop: {
      id: "dual-loop", phase: "Qualification", type: "loopback", illustration: ASSET + "09-final-dual-loopback.svg",
      title: "Final dual-channel loopback verification", summary: "Patch both output-to-input loopbacks simultaneously. Three green flashes indicate final qualification PASS.",
      connections: [["CV OUT A", "CV IN A"], ["CV OUT B", "CV IN B"]],
      checks: checks([
        { id: "both", label: "Both loopbacks are connected exactly as shown." },
        { id: "save", label: "I pressed SAVE after both IN/OUT pairs blinked." },
        { id: "pass", label: "The firmware reported final qualification PASS with three green flashes." }
      ])
    },
    outputTestIntro: {
      id: "output-test-intro", phase: "External DMM acceptance", type: "checks", title: "Enter calibrated Output Test",
      summary: "Use CAL mode for independent external output measurements. The inactive channel is held at calibrated 0 V.",
      alerts: [{ level: "important", title: "Independent check", text: "Loopback qualification is not an independent absolute voltage standard. The DMM check is the external acceptance measurement." }],
      checks: checks([
        { id: "shift", label: "I held SHIFT for 1 second from idle to enter Output Test." },
        { id: "cal", label: "CAL mode is active (green for Channel A / red for Channel B), not amber RAW mode." }
      ])
    },
    outputA: {
      id: "output-a", phase: "External DMM acceptance", type: "outputSweep", channel: "A", illustration: ASSET + "10-cal-output-test-channel-a.svg",
      title: "CAL Output Test — Channel A", summary: "Record C at each octave and all twelve chromatic notes in octave 4. Errors are calculated, but no undocumented pass threshold is imposed."
    },
    outputB: {
      id: "output-b", phase: "External DMM acceptance", type: "outputSweep", channel: "B", illustration: ASSET + "11-cal-output-test-channel-b.svg",
      title: "CAL Output Test — Channel B", summary: "Short-press SHIFT to select Channel B, then repeat the same independent DMM acceptance measurements."
    },
    exitOutput: {
      id: "exit-output", phase: "External DMM acceptance", type: "checks", title: "Exit Output Test",
      summary: "Hold SHIFT for one second and return to dim-amber idle before changing the power arrangement.",
      checks: checks([{ id: "idle", label: "Output Test has exited and the maintenance firmware is idle." }])
    },
    powerOffForReport: {
      id: "power-off-report", phase: "Stored report", type: "checks", title: "Power down before reading the stored report",
      alerts: [{ level: "danger", title: "Do not connect USB yet", text: "Switch the Eurorack case OFF and physically disconnect the ribbon before connecting USB." }],
      checks: checks([
        { id: "case-off", label: "Eurorack case is OFF." },
        { id: "ribbon-out", label: "Eurorack ribbon is disconnected from the module." },
        { id: "usb-in", label: "Only after disconnecting the ribbon, USB is connected." }
      ])
    },
    report: {
      id: "report", phase: "Stored report", type: "report", title: "Read and validate the stored calibration report",
      summary: "Open a serial monitor at 115200 baud. The maintenance firmware prints the newest stored calibration and matching qualification generation on boot.",
      alerts: [{ level: "info", title: "Read-only serial output", text: "The maintenance image has no writable serial command console. USB-only reporting performs no fresh analog measurement because the ±12 V analog stages are unpowered." }]
    },
    buildProduction: {
      id: "build-production", phase: "Return to production", type: "command", commandKind: "buildProduction", title: "Compile the production firmware",
      summary: "Keep the Eurorack ribbon disconnected while USB is connected.", expected: "PlatformIO finishes with SUCCESS for the selected production environment."
    },
    uploadProduction: {
      id: "upload-production", phase: "Return to production", type: "command", commandKind: "uploadProduction", title: "Upload the production firmware",
      summary: "Use a normal flash-only bootloader upload so the stored EEPROM calibration remains intact.",
      alerts: [{ level: "caution", title: "Preserve calibration EEPROM", text: "Do not use chip erase or an EEPROM upload. Normal flash-only uploads preserve the stored calibration." }],
      expected: "PlatformIO / avrdude completes and reports SUCCESS."
    },
    returnPower: {
      id: "return-power", phase: "Return to production", type: "checks", title: "Return the module to normal Eurorack power",
      alerts: [{ level: "danger", title: "Strict power transition", text: "Disconnect USB before reconnecting the Eurorack ribbon." }],
      checks: checks([
        { id: "usb-out", label: "USB is completely disconnected." },
        { id: "case-off", label: "Eurorack case is OFF." },
        { id: "ribbon-in", label: "Ribbon orientation is checked and the normal Eurorack ribbon is connected." },
        { id: "case-on", label: "Eurorack case is ON; USB remains disconnected." },
        { id: "production", label: "The module starts normally with production firmware." }
      ])
    },
    complete: {
      id: "complete", phase: "Complete", type: "summary", title: "Calibration procedure completed",
      summary: "The maintenance workflow, external DMM acceptance, stored report and production reflash have all been completed."
    }
  };

  global.CalibrationSteps = {
    base: base,
    adcA: adcSteps("A"),
    adcB: adcSteps("B")
  };
}(window));
