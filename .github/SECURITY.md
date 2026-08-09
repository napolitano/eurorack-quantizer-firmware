# Security Policy

## Supported versions

Security fixes are maintained for the latest tagged release of the firmware. Older releases may be superseded without receiving backports.

| Version | Supported |
| --- | --- |
| Latest tagged release | Yes |
| Older releases | No |

## Reporting a vulnerability

> [!WARNING]
> Do **not** disclose suspected security vulnerabilities in a public issue, discussion, pull request, or commit message. Use a private reporting channel first.

Please do **not** disclose suspected security vulnerabilities in a public issue, discussion, pull request, or commit message.

Use GitHub's **private vulnerability reporting** for this repository whenever it is available. Include enough information to reproduce and assess the issue, for example:

- affected firmware version or commit;
- affected hardware revision or board configuration, if relevant;
- a concise description of the vulnerability and its impact;
- reproducible steps or a minimal proof of concept;
- any known workaround or mitigation.

If private vulnerability reporting is not available, open a public issue containing **no vulnerability details** and request a private contact channel from the maintainer.

## Scope

This policy covers vulnerabilities in the firmware and repository-controlled build or release automation. General hardware faults, component tolerances, calibration drift, assembly errors, third-party toolchain vulnerabilities, and vulnerabilities in external services are outside the direct scope of this repository unless the firmware or repository configuration materially contributes to the issue.

## Response

Reports are reviewed on a best-effort basis. Valid reports will be investigated, and fixes will normally be prepared privately before public disclosure when practical. Release notes and security advisories may be published once a fix is available.

Please allow reasonable time for investigation and coordinated disclosure before publishing technical details.

---

<p align="center">From Munich With <img src="../docs/assets/blue-heart.svg" alt="blue heart" width="14" height="14"></p>
