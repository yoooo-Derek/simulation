# Migration Notes

The previous project at `/home/dyn/sim` is a read-only reference implementation
and behavior baseline for this repository.

The old code shows that earlier work concentrated the simulation workflow around
`src/main/hybrid-dcn-main.cc` and a scratch-compatible entry. That file must not
be copied directly into this project. Future migration must extract behavior by
module boundary, with small reviewed changes and fresh tests in
`contrib/tl-ocs`.

Rules for `/home/dyn/sim`:

- read-only reference only;
- do not modify, format, commit, clean, rename, delete, or regenerate files;
- do not copy large code blocks;
- do not migrate `hybrid-dcn-main.cc` wholesale;
- use it only to understand previous behavior, invariants, and validation
  expectations.

This round creates a new ns-3 contrib module scaffold rather than migrating old
simulation logic.

