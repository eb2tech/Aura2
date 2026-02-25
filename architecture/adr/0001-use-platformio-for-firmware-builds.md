# 0001 – Use PlatformIO for firmware builds

- **Status:** Accepted
- **Date:** 2025-02-22

## Context

The original Aura project used the Arduino IDE. Aura2 aims to be more maintainable, reproducible, and friendly to modern tooling and CI, while targeting the same class of hardware (CYD board) and similar firmware concerns.

## Decision

Use PlatformIO with Visual Studio Code as the primary build and development environment for Aura2.

## Consequences

- **Positive:**
  - Reproducible builds via `platformio.ini`.
  - Easier dependency management and environment configuration.
  - Better integration with VS Code, debugging, and linting.
- **Negative:**
  - Higher initial learning curve compared to Arduino IDE.
  - Contributors must install and understand PlatformIO.
