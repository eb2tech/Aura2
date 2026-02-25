# 0003 – Integrate with Home Assistant via MQTT

- **Status:** Accepted
- **Date:** 2025-02-22

## Context

Aura2 is intended to participate in a broader home automation ecosystem. Home Assistant is already in use, and MQTT is a well-supported, lightweight protocol for IoT-style integrations.

## Decision

Use MQTT as the integration mechanism with Home Assistant. Aura2 will publish sensor data and subscribe to relevant topics using MQTT conventions compatible with Home Assistant.

## Consequences

- **Positive:**
  - Simple, decoupled integration with Home Assistant.
  - Reusable patterns for future devices and widgets.
  - Works well over constrained networks.
- **Negative:**
  - Requires MQTT broker configuration and network reliability.
  - Adds complexity to connection management and reconnection logic in firmware.
