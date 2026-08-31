(function (global) {
  "use strict";

  var S = global.CalibrationSteps.base;
  var adcA = global.CalibrationSteps.adcA;
  var adcB = global.CalibrationSteps.adcB;

  function fullFlow() {
    return [
      S.equipment, S.adapter, S.bootloader, S.environment, S.buildCalibration,
      S.powerOffBeforeUsb, S.connectUsb, S.uploadCalibration, S.maintenanceBootUsb,
      S.transitionToEurorack, S.rawChoice, S.enterRaw, S.pretrimA, S.pretrimB,
      S.exitRaw, S.startFull
    ].concat(adcA, adcB, [
      S.dacA, S.dacB, S.dualLoop, S.outputTestIntro, S.outputA, S.outputB,
      S.exitOutput, S.powerOffForReport, S.report, S.buildProduction,
      S.uploadProduction, S.returnPower, S.complete
    ]);
  }

  global.CalibrationFlows = {
    definitions: {
      full: {
        label: "Complete calibration",
        description: "Repository → maintenance build/upload → safe power transition → optional RAW pre-trim → full ADC/DAC calibration → DMM acceptance → stored report → production reflash.",
        steps: fullFlow()
      },
      raw: {
        label: "RAW pre-trim + DAC calibration",
        description: "For modules with a valid stored ADC profile: build/upload maintenance firmware, perform A/B RAW span pre-trim, then immediately run DAC-only self-calibration and qualification before restoring production firmware.",
        steps: [S.equipment, S.adapter, S.bootloader, S.environment, S.buildCalibration, S.powerOffBeforeUsb, S.connectUsb, S.uploadCalibration, S.maintenanceBootUsb, S.transitionToEurorack,
          {
            id:"raw-adc-prerequisite", phase:"Hardware pre-trim", type:"checks", title:"Confirm the ADC prerequisite", summary:"DAC-only calibration requires a valid stored ADC calibration. If the module has no valid stored ADC profile, restart and use Complete calibration instead.",
            alerts:[{level:"caution", title:"Required prerequisite", text:"RAW pre-trim changes the output hardware. After any trimmer movement, DAC-only or full calibration is mandatory. DAC-only depends on a valid stored ADC calibration."}],
            checks:[{id:"adc-valid",label:"This module has a valid stored ADC calibration that has not been invalidated by input-path hardware changes."}]
          },
          S.enterRaw, S.pretrimA, S.pretrimB, S.exitRaw,
          {
            id:"start-dac-only", phase:"DAC self-calibration", type:"checks", title:"Start DAC-only calibration", summary:"From dim-amber idle, press SHIFT + LOAD. The firmware will request Channel A loopback, Channel B loopback, then final dual-loopback verification.",
            checks:[{id:"started",label:"I pressed SHIFT + LOAD and DAC-only calibration started."}]
          },
          S.dacA, S.dacB, S.dualLoop, S.outputTestIntro, S.outputA, S.outputB, S.exitOutput, S.powerOffForReport, S.report, S.buildProduction, S.uploadProduction, S.returnPower, S.complete]
      },
      verify: {
        label: "Verify stored calibration",
        description: "Build/upload maintenance firmware, run the stored dual-loopback verification, perform external CAL output measurements, read the report, then restore production firmware.",
        steps: [S.equipment, S.adapter, S.bootloader, S.environment, S.buildCalibration, S.powerOffBeforeUsb, S.connectUsb, S.uploadCalibration, S.maintenanceBootUsb, S.transitionToEurorack,
          {
            id:"verify-start", phase:"Qualification", type:"checks", title:"Start stored-calibration verification", summary:"From dim-amber idle, press LOAD. When both IN/OUT pairs blink, patch both loopbacks and press SAVE.",
            checks:[{id:"load",label:"I pressed LOAD from idle."},{id:"loops",label:"Both CV OUT → CV IN loopbacks are connected."},{id:"pass",label:"Verification completed with PASS."}]
          }, S.outputTestIntro, S.outputA, S.outputB, S.exitOutput, S.powerOffForReport, S.report, S.buildProduction, S.uploadProduction, S.returnPower, S.complete]
      },
      erase: {
        label: "Erase calibration data",
        description: "Safely install maintenance firmware and explicitly erase only calibration journal and qualification metadata.",
        steps: [S.bootloader, S.environment, S.buildCalibration, S.powerOffBeforeUsb, S.connectUsb, S.uploadCalibration, S.maintenanceBootUsb, S.transitionToEurorack,
          {
            id:"erase", phase:"Maintenance", type:"checks", title:"Erase stored calibration data", summary:"Hold SHIFT + SAVE + LOAD for 5 seconds. Release when the full ring pulses red, then press SAVE to confirm.",
            alerts:[{level:"caution", title:"Destructive maintenance operation", text:"This invalidates the calibration journal and qualification metadata. User scales, full-configuration presets and live state remain intact."}],
            checks:[{id:"held",label:"I held SHIFT + SAVE + LOAD together for 5 seconds."},{id:"red",label:"The full ring began pulsing red."},{id:"save",label:"I pressed SAVE to confirm the erase."},{id:"amber",label:"Three amber flashes confirmed completion."}]
          }, S.powerOffForReport, S.buildProduction, S.uploadProduction, S.returnPower, S.complete]
      }
    }
  };
}(window));
