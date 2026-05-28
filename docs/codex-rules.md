# Codex Rules

Every future TL-OCS development round must finish with:

- actual code changes, not only analysis;
- a build or smoke run;
- a git commit;
- the commit SHA in the final report;
- `git status --short --untracked-files=no` for status acceptance, because the
  ns-3 upstream source tree is intentionally not tracked in this project repo;
- a clear statement about whether forbidden areas were touched;
- confirmation that `/home/dyn/sim` remained unmodified;
- confirmation when old `hybrid-dcn-main.cc` was not copied or migrated.

Forbidden areas:

- `/home/dyn/sim` must remain read-only.
- ns-3 upstream core directories must not be modified unless CMake integration
  or a documented build issue makes it unavoidable.
- scratch files must not become the home of core TL-OCS logic.

Implementation discipline:

- self-authored TL-OCS code belongs under `contrib/tl-ocs`;
- add only the files needed for the current step;
- avoid empty placeholder classes and large unused directory trees;
- prefer small testable modules over monolithic simulation entries.
- avoid over-validating configuration, silent fallback behavior, and automatic
  configuration repair;
- surface invalid configuration early instead of guessing replacement values;
- fold small corrections into the next substantive development round instead of
  creating separate housekeeping-only tasks.
