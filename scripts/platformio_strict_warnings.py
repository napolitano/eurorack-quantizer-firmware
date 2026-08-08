# Purpose: Apply project-only strict warnings to FMQ production and test sources.
# Author: Axel Napolitano
# Copyright (C) 2026 Axel Napolitano
# SPDX-License-Identifier: GPL-3.0-or-later

"""PlatformIO middleware for strict warnings on first-party native-test sources.

The native environment deliberately keeps third-party/generated sources on the
normal warning level.  The stricter diagnostics below are added only to FMQ
production sources and repository-owned test sources.  This prevents compiler
warnings in PlatformIO's generated Unity adapter or the upstream Unity library
from being promoted to project build failures, while preserving -Werror for the
code that this repository owns.
"""

Import("env")

STRICT_FLAGS = [
    "-Werror",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wshadow",
    "-Wpedantic",
]

PROJECT_DIR = env.subst("$PROJECT_DIR").replace("\\", "/").rstrip("/").lower()
FIRST_PARTY_ROOTS = (
    PROJECT_DIR + "/lib/fmq/src/",
    PROJECT_DIR + "/test/",
)


def _is_first_party_source(node):
    """Return True for repository-owned production or test translation units."""
    path = node.get_abspath().replace("\\", "/").lower()
    return any(path.startswith(root) for root in FIRST_PARTY_ROOTS)


def _enable_strict_warnings(build_env, node):
    """Add strict diagnostics only to first-party source compilation."""
    if not _is_first_party_source(node):
        return node

    current = list(build_env.get("CCFLAGS", []))
    for flag in STRICT_FLAGS:
        if flag not in current:
            current.append(flag)

    return build_env.Object(node, CCFLAGS=current)


env.AddBuildMiddleware(_enable_strict_warnings)
