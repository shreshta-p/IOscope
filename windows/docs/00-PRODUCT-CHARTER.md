# Product charter

IOscope explains how storage, memory, CPU and GPU interact on a Windows laptop.
Audience: systems/storage/GPU engineers and technically curious reviewers.
Priorities: correctness, contracts, reliability/safety, 3D/UX, maintainability,
performance, demo impact, then feature quantity.

Success is two reproducible demonstrations: a queue-depth sweep and model-like
storage-to-GPU transfer, each observable, explainable, saved and replayable.
V1 includes only Live, Workload Lab, Experiments, Runs, Learn and Analyze.

No Linux runtime dependency, distributed storage, cloud accounts, kernel tuning,
overclocking, filesystem comparison suite, topology editor or stress/torture modes.
This initial delivery ends at the specification/contract boundary.
