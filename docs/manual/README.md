# User manual workspace

This directory is the workspace for the editable end-user manual of the Eurorack Quantizer.
The canonical editable document will be maintained as a **LibreOffice Writer** document (`.odt`).

## Required typeface

> [!IMPORTANT]
> Install the **Ubuntu Font Family** before editing the manual. Font substitution can change line breaks, pagination, spacing, and therefore the reproducibility of the published layout.

The manual uses the **Ubuntu Font Family**. The required fonts must be installed
on the editing system before opening or modifying the manual; otherwise
LibreOffice Writer may substitute metrics and alter line breaks, pagination and
layout.

Official source:

- Ubuntu Font Family: https://design.ubuntu.com/font
- Ubuntu Font Licence: https://canonical.com/legal/font-licence

The Ubuntu Font Family is distributed under the **Ubuntu Font Licence 1.0**.
That licence permits use, study, modification and redistribution subject to its
terms. The font files themselves are not part of this repository and are not
covered by the manual's Creative Commons licence.

## File naming and firmware releases

Editable manuals use the versioned filename
`quantizer-user-manual.X.Y.Z.odt`. The version identifies the firmware
generation documented by that manual; a new firmware patch or minor release
does not require a new manual unless user-visible behaviour has changed enough
to justify one.

For a tagged firmware release, the release workflow selects the **highest manual
version that is not newer than the firmware version**. For example, firmware
`0.2.1` reuses `quantizer-user-manual.0.2.0.odt` when no `0.2.1` manual exists.
A newer manual is never attached to an older firmware release. If no compatible
manual exists at all, the firmware release continues and emits a workflow
warning instead of failing.

The selected ODT is published unchanged as a release asset. The release runner
also installs LibreOffice Writer and the Ubuntu Font Family and attempts a
headless PDF export using the same manual version in the filename. PDF export
is best-effort: failure to create the PDF does not prevent publication of the
ODT or firmware. Any manual assets that are created are included in the release
SHA-256 and MD5 checksum manifests.

## Reusable concept artwork

The repository keeps reusable functional diagrams under [`../assets/concepts/`](../assets/concepts/). These SVG sources are intended for both GitHub documentation and the LibreOffice user manual so signal-flow and control concepts do not need to be redrawn independently for each format. Preserve their functional color semantics when adapting them for the manual.

The concept diagrams are part of the user-manual artwork set but retain their own directory-specific **CC BY-NC 4.0** license. See [`../assets/concepts/LICENSE.md`](../assets/concepts/LICENSE.md).

## Manual licence

Unless a file states otherwise, the original manual content in this directory
is licensed under **Creative Commons Attribution-NonCommercial 4.0
International (CC BY-NC 4.0)**. See [LICENSE.md](LICENSE.md).

Firmware source code remains covered by the repository root licence; the manual
licence does not replace or modify the software licence.

---

<p align="center">From Munich With <img src="../assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
