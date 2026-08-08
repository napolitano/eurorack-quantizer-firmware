# Purpose: Ensure gcov coverage instrumentation is also linked into native tests.
# Author: Axel Napolitano
# Copyright (C) 2026 Axel Napolitano
# SPDX-License-Identifier: GPL-3.0-or-later

"""PlatformIO linker configuration for the native coverage environment.

`--coverage` in build_flags instruments C/C++ translation units, but the final
native test executable must also be linked with the GCC coverage runtime.
PlatformIO's native test link step does not reliably propagate the compile-side
coverage option, so add it explicitly to LINKFLAGS.
"""

Import("env")

if "--coverage" not in env.get("LINKFLAGS", []):
    env.Append(LINKFLAGS=["--coverage"])
