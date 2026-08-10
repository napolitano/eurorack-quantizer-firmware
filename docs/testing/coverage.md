# Native coverage policy

Native source coverage is used as a **regression signal**, not as a substitute for requirement-oriented tests. Coverage is measured only for the portable production implementation under `lib/fmq/src/`; test code and third-party Unity code are excluded.

## Contents

- [Current reference](#current-reference)
- [Hard CI floors](#hard-ci-floors)
- [Why the floor is below the reference](#why-the-floor-is-below-the-reference)
- [Running coverage locally](#running-coverage-locally)
- [How to use coverage results](#how-to-use-coverage-results)

## Current reference

After the requirement-driven edge-case expansion, an independent GCC/gcov verification of the complete 29-suite / 254-test host set measured approximately:

| Metric | Reference |
|---|---:|
| Line coverage | **95.69%** |
| Branch coverage | **80.28%** |

> [!NOTE]
> GitHub Actions is the authoritative measurement because compiler/gcov/gcovr versions can slightly change accounting. The committed values are a reference point, not a claim that every future toolchain will print byte-for-byte identical percentages.

## Hard CI floors

The policy in [`scripts/native_coverage_policy.json`](../../scripts/native_coverage_policy.json) currently requires at least:

| Metric | Minimum |
|---|---:|
| Line coverage | **92.0%** |
| Branch coverage | **70.0%** |

A pull request or push falling below either floor fails CI.

## Why the floor is below the reference

The reference and the hard floor serve different purposes. The reference records the current quality level; the floor prevents a material silent regression while leaving limited headroom for coverage-accounting differences and legitimate refactoring.

> [!IMPORTANT]
> Do not lower a coverage floor simply to make a change pass. A lower floor requires an explicit engineering reason. Normal development should keep or raise the floor as coverage becomes more mature.

## Running coverage locally

Install the host tooling, then run the instrumented suite:

```sh
python -m pip install --upgrade gcovr
pio test -e native_coverage
mkdir -p coverage
gcovr --root . --filter lib/fmq/src --exclude test --txt --output coverage/coverage.txt
gcovr --root . --filter lib/fmq/src --exclude test --xml-pretty --output coverage/coverage.xml
gcovr --root . --filter lib/fmq/src --exclude test --html-details --output coverage/coverage.html
python scripts/check_native_coverage.py coverage/coverage.xml
```

CI additionally publishes the HTML/XML/text reports as the `native-coverage` workflow artifact.

## How to use coverage results

Prioritize uncovered code when it represents a meaningful behavioral boundary, for example:

- invalid persisted enum/range values;
- state-machine reset or cancellation paths;
- wraparound and exact timing boundaries;
- all user-addressable storage slots;
- linked/unlinked state invariants;
- trigger-vs-pitch behavior;
- safety fallbacks.

Avoid writing tests solely to execute defensive lines that cannot occur through a valid public API. The desired order remains:

1. requirement or externally visible behavior;
2. edge/error path;
3. regression test;
4. source coverage improvement as a consequence.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
