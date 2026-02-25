# 0002 – Use EEZ Studio for UI layout

- **Status:** Accepted
- **Date:** 2025-02-22

## Context

The original Aura project manually created and positioned LVGL widgets in C. This made layout changes tedious and error-prone. Aura2 aims to experiment with a more visual, iterative approach to UI design.

## Decision

Use EEZ Studio as the primary tool for defining UI layout, generating LVGL-related code or assets where appropriate. Manual C code should focus on behavior and state, not layout geometry.

## Consequences

- **Positive:**
  - Faster iteration on UI layout and visual design.
  - Clear separation between layout (EEZ Studio) and behavior (C code).
  - Easier to reason about screen structure.
- **Negative:**
  - Introduces a dependency on EEZ Studio and its project format.
  - Generated code may require careful handling to avoid manual edits being overwritten.
