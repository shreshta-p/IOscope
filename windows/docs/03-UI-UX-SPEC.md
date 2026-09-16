# UI / UX

Six primary destinations: LIVE, WORKLOAD LAB, EXPERIMENTS, RUNS, LEARN, ANALYZE.
Use a restrained engineering workstation: charcoal surfaces, readable type, subtle
material lighting, cyan read flow and amber write flow with arrows/text as redundancy.
The twin occupies the main canvas; selected-component inspector and compact trend
strip support it. No grid of generic metric cards or decorative activity.

At 1440×900, navigation, origin badge, emergency stop and selected metrics remain
visible. At 1280×720 inspectors collapse into accessible drawers. Larger screens use
space for the twin, not smaller text. Body text minimum 14px; keyboard targets 24px
minimum, primary controls 40px. Contrast meets WCAG AA.

Mode changes stop the prior presentation subscription, not an active hardware run.
Active hardware state and stop remain visible across navigation and replay.
Replay cannot issue workload commands. Leaving a run never implicitly cancels it.
Start summary shows exact parameters, expected disk allocation, duration and sensor
coverage; a start click authorizes that concrete run.

Every empty/error state includes reason and next action. Unavailable is text, not 0.
Reduced motion disables particles/camera easing but retains directional static paths.
Charts have units, scope, window and text summaries. WebGL failure yields a component
list and data path diagram with all controls. Focus order and screen-reader event
announcements must not update every telemetry tick.
