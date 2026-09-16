# Native workload execution plan

Approved scope: the master prompt and continuing V1 authorization. Implement the
existing safety/workload specifications; no request for additional privilege.

1. Add pure admission policy and fake-resource tests in agent/safety.hpp and
   safety_tests.cpp. Check schema bounds, reserves after allocation, offered-rate
   write budgets, thermal coverage and stale/threshold samples. Denied requests do
   not create files or processes. Plans reserve aggregate experiment budgets.
2. Add RAII owned scratch and child-job adapters. Create a same-user private
   LocalAppData directory, reject reparse ancestors, exclusively create regular
   targets and manifests, verify final handle paths and file identity, and delete
   only owned files. Initialize in bounded chunks with cancellation. Use a Job
   Object with kill-on-close and a suspended child before assignment/resume.
3. Pin the official DiskSpd release and executable hash. Translate only typed
   controls to internally quoted arguments: one worker, one target, explicit rate,
   warmup/cooldown, software cache mode, latency and XML. Capture bounded output.
   Parse with DTD/external entities prohibited; reject missing/malformed summaries.
4. Add a controller with one admitted run, independent watchdog, cancellation and
   durable state. Record preparation, running, stopping and terminal outcomes even
   if the UI disconnects. Reject duplicate starts while busy. Never report success
   before validated results and terminal metadata are durably saved.
5. Extend canonical recording contracts to native runs; generate types, update
   validators and fixtures. UI displays effective controls, unavailable thermal
   restrictions, active state and cancellation. Emergency stop calls native cancel.
6. Exercise fake child timeout/crash, scratch ownership, quota, cancellation and
   malformed XML cases. Only then execute an explicitly bounded light read trial
   on a new 64 MiB owned target; retain the real result and cleanup evidence.

No arbitrary paths, shell strings, raw-device workloads, global cache purge or
administrator requirement. Unknown CPU/SSD temperatures enforce light intensity
and <=15 seconds. Existing high-intensity presets need an explicit restricted
variant; never silently alter accepted workload metadata.

After the gate passes, implement experiments serially in the order in 19-ROADMAP.
Each phase uses the same admitted controller and records exact changed controls.
