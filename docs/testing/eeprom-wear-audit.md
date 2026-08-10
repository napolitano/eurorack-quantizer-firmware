# EEPROM endurance audit

This audit documents how the firmware uses the ATmega328P's 1 KiB EEPROM and identifies the cells that receive the highest write frequency. It is an engineering review, not a promise about the lifetime of a particular MCU.

## Contents

- [Device endurance basis](#device-endurance-basis)
- [EEPROM layout](#eeprom-layout)
- [Write behavior](#write-behavior)
- [Live-state worst case](#live-state-worst-case)
- [Other persisted data](#other-persisted-data)
- [Verification](#verification)
- [Conclusion](#conclusion)
- [Vendor reference](#vendor-reference)

## Device endurance basis

The ATmega328P provides 1024 bytes of EEPROM. Microchip specifies EEPROM endurance in erase/write cycles; the design therefore treats unnecessary physical writes as something to avoid rather than assuming writes are free.

> [!NOTE]
> `AvrEeprom::writeByte()` reads the old byte and starts a physical EEPROM write only when the value actually changes. `AsyncEepromWriter` advances by at most one physical byte operation at a time, so persistence does not deliberately block a complete 1 kHz control cycle.

## EEPROM layout

| Region | Size / count | Purpose |
|---|---:|---|
| Scale slots | 12 × 6 bytes | User scales |
| Full-config slots | 12 × 36 bytes | User Full Configurations |
| Live-state ring | 12 × 42 bytes | Wear-levelled working state + LED calibration |
| Startup sequence | 1 byte at address 1023 | Next startup animation |

The live ring begins at byte 504. Twelve complete 42-byte records fit before startup metadata, leaving a small unused gap.

## Write behavior

User Scale and Full Configuration slots are written only on explicit SAVE operations. Their records are invalidated before their payload/CRC is updated, then the record marker is committed last. Re-saving the same slot therefore gives the marker cell more wear than most payload cells, but these writes are directly user initiated rather than periodic.

Live-state autosave is different: a relevant front-panel edit marks state dirty, and the firmware waits for **3000 ms of quiescence** before queuing a new record. Each successful commit advances to the next live-ring slot.

## Live-state worst case

The ring contains 12 slots. A payload cell is revisited only after 12 live-state commits. The record-marker byte is the conservative limiting cell because on an already-used slot it can physically transition twice during a rewrite:

1. `0xA5 → 0xFF` to invalidate the old record;
2. `0xFF → 0xA5` to commit the new record.

Using a conservative 100,000-cycle endurance figure per EEPROM byte, the marker-cell bound corresponds to approximately:

```text
12 slots × 100,000 cycles / 2 marker writes ≈ 600,000 live-state commits
```

Other live-record bytes have a conservative upper bound of approximately 1.2 million ring commits before the same cell reaches 100,000 physical writes, and unchanged values are skipped entirely.

The theoretical absolute stress case of one persistent-state change followed by exactly 3 seconds of quiescence, continuously around the clock, would reach the marker-cell bound in roughly 21 days. That is intentionally an unrealistic abuse case, but it shows why the wear-levelled ring matters.

For perspective only:

| Sustained live-state commits | Approx. time to 600k commits |
|---:|---:|
| 10/day | 164 years |
| 50/day | 33 years |
| 100/day | 16.4 years |
| 500/day | 3.3 years |
| 1,000/day | 1.6 years |

> [!IMPORTANT]
> These figures are arithmetic based on the conservative per-byte endurance figure and the current write algorithm. They are not predicted product lifetimes. Temperature, silicon variation, actual byte-change patterns, and usage all matter.

## Other persisted data

### User save slots

These are not wear-levelled because SAVE is an explicit user action. The marker receives two physical transitions when overwriting a previously valid record; payload bytes are skipped when unchanged.

### Startup-sequence byte

The startup-sequence selector uses one fixed EEPROM byte. With rotating startup sequences enabled it normally changes once per boot. Even without wear levelling, reaching 100,000 physical changes would require 100,000 power-up sequence updates. Writing the same value again is suppressed.

### LED calibration

LED calibration is carried inside the wear-levelled live state rather than assigned a repeatedly rewritten fixed byte.

## Verification

`integration/test_eeprom_wear` instruments the EEPROM test double with per-address physical-write counters. It verifies that:

- 24 consecutive live-state commits visit every one of the 12 live slots exactly twice;
- each live marker receives the expected three physical writes across those two visits;
- non-marker live bytes receive no more than two writes;
- no unrelated EEPROM region is touched;
- rewriting an unchanged byte is suppressed;
- storing an unchanged startup-sequence value does not add a physical write.

This makes the arithmetic above a checked property of the current implementation rather than documentation that can silently drift.

## Conclusion

The current live-state design is appropriate for normal musical use: it combines a 3-second quiescence delay, a 12-record ring, asynchronous byte writes, and unchanged-byte suppression. No EEPROM algorithm change is justified by the audit at this time.

A future change that increases autosave frequency, reduces the ring, or writes fixed metadata more often must update this audit and its tests.

## Vendor reference

This audit uses the ATmega328P device-family datasheet minimum of **100,000 EEPROM write/erase cycles per location** as its conservative arithmetic basis. The canonical current source for the device datasheet and memory-size information is the [Microchip ATmega328P product page](https://www.microchip.com/en-us/product/ATmega328P).

> [!NOTE]
> If Microchip publishes a revised device datasheet with different endurance limits, update both this audit and the corresponding engineering assumptions rather than treating the 100,000-cycle figure as timeless.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
