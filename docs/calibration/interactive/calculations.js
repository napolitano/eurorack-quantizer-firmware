(function (global) {
  "use strict";

  function parseVoltage(value) {
    if (value === null || value === undefined || String(value).trim() === "") return null;
    var parsed = Number(String(value).replace(",", "."));
    return Number.isFinite(parsed) ? parsed : null;
  }

  function targetVoltage(octave, pitchClass) {
    return Number(octave) + Number(pitchClass) / 12.0;
  }

  function voltageError(target, measured) {
    var m = parseVoltage(measured);
    if (m === null) return null;
    var volts = m - Number(target);
    return {
      volts: volts,
      millivolts: volts * 1000.0,
      cents: volts * 1200.0
    };
  }

  function pretrim(c1, c9, c5) {
    var v1 = parseVoltage(c1);
    var v9 = parseVoltage(c9);
    var v5 = parseVoltage(c5);
    if (v1 === null || v9 === null) return null;
    var span = v9 - v1;
    var result = {
      v1: v1,
      v9: v9,
      span: span,
      spanErrorMillivolts: (span - 8.0) * 1000.0,
      midpoint: (v1 + v9) / 2.0,
      c5: v5
    };
    if (v5 !== null) {
      result.c5ErrorMillivolts = (v5 - 5.0) * 1000.0;
      result.midpointErrorMillivolts = (v5 - result.midpoint) * 1000.0;
    }
    return result;
  }

  function validateStoredReport(text) {
    var source = String(text || "");
    var required = [
      "FM QUANTIZER STORED CALIBRATION REPORT",
      "Calibration format:",
      "Generation:",
      "ADC A: offset=",
      "ADC B: offset=",
      "DAC A: offset=",
      "DAC B: offset=",
      "DAC A verification: PASS / max residual",
      "DAC B verification: PASS / max residual",
      "OVERALL: PASS",
      "CALIBRATION VERIFIED"
    ];
    var missing = required.filter(function (fragment) { return source.indexOf(fragment) < 0; });
    var generationMatch = source.match(/Generation:\s*(\d+)/);
    var residualAMatch = source.match(/DAC A verification:\s*PASS \/ max residual\s*(\d+)\s*codes/);
    var residualBMatch = source.match(/DAC B verification:\s*PASS \/ max residual\s*(\d+)\s*codes/);
    return {
      valid: missing.length === 0,
      missing: missing,
      generation: generationMatch ? Number(generationMatch[1]) : null,
      residualA: residualAMatch ? Number(residualAMatch[1]) : null,
      residualB: residualBMatch ? Number(residualBMatch[1]) : null
    };
  }

  function formatSigned(value, digits, suffix) {
    if (value === null || value === undefined || !Number.isFinite(value)) return "—";
    var sign = value > 0 ? "+" : "";
    return sign + value.toFixed(digits) + (suffix || "");
  }

  global.CalibrationMath = {
    parseVoltage: parseVoltage,
    targetVoltage: targetVoltage,
    voltageError: voltageError,
    pretrim: pretrim,
    validateStoredReport: validateStoredReport,
    formatSigned: formatSigned
  };
}(window));
