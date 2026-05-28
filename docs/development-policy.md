# TL-OCS Development Policy

This repository is the new TL-OCS ns-3 workspace at `/home/dyn/simulation`.

Long-term development rules:

- `/home/dyn/sim` is a read-only reference. Do not modify, format, clean,
  rename, delete, commit, or regenerate files there.
- Do not copy or migrate the old `hybrid-dcn-main.cc`.
- The ns-3 upstream source tree does not need to be tracked by this Git repo.
- Use `git status --short --untracked-files=no` for Git status acceptance.
- Self-authored TL-OCS code belongs under `contrib/tl-ocs`.
- `scratch` files are thin runners only.
- Do not over-validate, silently fall back, or automatically repair
  configuration.
- Do not write fake metrics. Only export values that were actually produced by
  the current run.
- Every development round must build or smoke, commit, and report the commit
  SHA. Do not push.
- Fold small corrections into the next substantive task instead of creating
  housekeeping-only rounds.
