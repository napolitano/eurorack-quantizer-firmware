#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");

const root = path.resolve(__dirname, "..");
global.window = global;

for (const file of ["calculations.js", "steps.js", "flows.js"]) {
  const source = fs.readFileSync(path.join(root, "docs", "calibration", "interactive", file), "utf8");
  vm.runInThisContext(source, { filename: file });
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

function near(actual, expected, tolerance, message) {
  assert(Math.abs(actual - expected) <= tolerance, `${message}: got ${actual}, expected ${expected}`);
}

near(CalibrationMath.targetVoltage(4, 1), 4.083333333333333, 1e-12, "C#4 target");
near(CalibrationMath.voltageError(4.0, "4.001").millivolts, 1.0, 1e-9, "mV error");
near(CalibrationMath.voltageError(4.0, "4.001").cents, 1.2, 1e-9, "cent conversion");
near(CalibrationMath.pretrim("1.001", "9.000", "5.000").span, 7.999, 1e-12, "RAW span");
near(CalibrationMath.pretrim("1.001", "9.000", "5.000").spanErrorMillivolts, -1.0, 1e-9, "RAW span error");
assert(CalibrationMath.parseVoltage("5,000") === 5, "comma decimal parsing");
assert(CalibrationMath.parseVoltage("") === null, "empty voltage parsing");

const validReport = `
FM QUANTIZER STORED CALIBRATION REPORT
Firmware target: 0.2.2 (Unreleased)
Calibration format: 1
Generation: 7
ADC A: offset=1 gain=2/3
ADC B: offset=1 gain=2/3
DAC A: offset=1 gain=2/3
DAC B: offset=1 gain=2/3
DAC A verification: PASS / max residual 3 codes
DAC B verification: PASS / max residual 4 codes
OVERALL: PASS
----- LABEL -----
CALIBRATION VERIFIED
`;
const parsed = CalibrationMath.validateStoredReport(validReport);
assert(parsed.valid, "qualified stored report should validate");
assert(parsed.generation === 7, "report generation extraction");
assert(parsed.residualA === 3 && parsed.residualB === 4, "report residual extraction");
assert(!CalibrationMath.validateStoredReport("Calibration profile: NOT STORED").valid, "unqualified report must fail");

assert(CalibrationSteps.adcA.length === 10 && CalibrationSteps.adcB.length === 10, "each ADC channel must have ten reference steps");
assert(CalibrationSteps.adcA.map(s => s.target).join(",") === "1,3,5,7,9,0,2,4,6,8", "Channel A ADC order");
assert(CalibrationSteps.adcB.map(s => s.target).join(",") === "1,3,5,7,9,0,2,4,6,8", "Channel B ADC order");

const full = CalibrationFlows.definitions.full.steps;
assert(full.length >= 45, "complete flow should expose individual calibration steps");
for (const id of ["build-calibration", "upload-calibration", "adc-a-0v", "adc-b-8v", "dac-a", "dac-b", "dual-loop", "output-a", "output-b", "report", "build-production", "upload-production", "return-power"]) {
  assert(full.some(step => step.id === id), `full flow missing ${id}`);
}

const raw = CalibrationFlows.definitions.raw.steps;
assert(raw.some(step => step.id === "raw-adc-prerequisite"), "RAW flow must require stored ADC calibration");
assert(raw.some(step => step.id === "start-dac-only"), "RAW flow must continue into DAC-only calibration");
assert(raw.findIndex(step => step.id === "raw-pretrim-b") < raw.findIndex(step => step.id === "start-dac-only"), "DAC-only must follow RAW pre-trim");

console.log(`Interactive calibration runtime tests: PASS (${full.length} complete-flow steps)`);
