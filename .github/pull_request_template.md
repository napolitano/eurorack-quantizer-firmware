## Summary

Describe what this pull request changes and why.

## Type of change

- [ ] Bug fix
- [ ] New or changed firmware behavior
- [ ] Test / CI / release engineering
- [ ] Documentation with material user impact
- [ ] Refactoring with no intended behavior change

## Validation

- [ ] `pio test -e native` passes
- [ ] `pio run -e nanoatmega328new` passes
- [ ] `pio run -e nanoatmega328` passes
- [ ] AVR resource-budget checks pass, when the AVR toolchain is available
- [ ] New or changed behavior has regression coverage
- [ ] Relevant hardware behavior has been tested or the remaining hardware-validation requirement is stated below

## Hardware compatibility

- [ ] This change requires no PCB, component, pin-assignment, or wiring modification.
- [ ] The change remains within the Arduino Nano R3 / ATmega328P resource and timing constraints.

## Documentation and changelog

- [ ] User/technical documentation is updated where behavior changed, or no documentation update is required.
- [ ] `CHANGELOG.md` contains a release-relevant entry when appropriate, or this change is intentionally excluded under the project's editorial-change policy.
- [ ] Historical release sections have not been rewritten.

## Additional verification notes

List hardware measurements, CI details, known limitations, screenshots, or other information that helps review the change. Use `N/A` when there is nothing to add.
