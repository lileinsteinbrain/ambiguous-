## What this is
An early-stage research prototype exploring how pressure-based embodied interaction can regulate system entropy over time.

## Input
- Continuous pressure input via FSR sensor (intensity + irregularity)

## Processing
- Pressure variance mapped to an internal entropy value
- Temporal smoothing and decay applied to introduce inertia and avoid abrupt state changes

## Output
- Real-time haptic feedback via DRV2605
- System state shifts expressed as continuous, breathing-like responses

## What currently works
- The system responds continuously to touch rather than discrete commands.
- Gentle, stable interaction leads to a calmer internal system state.

## What’s unfinished
- The entropy model is heuristic and requires further tuning and validation.
- Long-term interaction patterns have not yet been studied.

## Why it matters
Softshell treats interaction as a temporal, co-regulated process, suggesting alternative models for calm and non-intrusive human–machine interfaces.
