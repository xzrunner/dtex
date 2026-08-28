# DTex

Dynamic texture. Draw call batching.

## TextureBuffer load contract

`LoadStart()` and `LoadFinish()` form a strictly paired, nestable scope. A
successful inner `LoadFinish()` only closes that inner scope. The outermost
`LoadFinish()` returns `false` for a CPU/GPU failure and keeps the pending batch
retryable by a later empty `LoadStart()` / `LoadFinish()` pair.

`LoadFinish() == true` means the scope completed without an execution failure;
it does **not** mean every submitted key fitted in the fixed atlas. Capacity-
limited keys remain deferred. Call `Query(key, ...)` after a successful finish
to determine which keys were actually published, and drive another finish to
process deferred keys.

Regions must be non-empty, forward, inside the source texture, and representable
by DTex's 16-bit `Rect`. Negative padding/extrusion and arithmetic overflow are
rejected. Published lookup tables and eviction packers are committed only after
all fallible CPU preparation and GPU work succeed.

If GPU work fails after an eviction clear was attempted, lookup tables for all
staged victim blocks are invalidated before returning. This deliberately turns
possibly cleared old entries into cache misses; the pending replacement batch is
retained and an empty `LoadStart()` / `LoadFinish()` pair retries it.

`TexRenderer` uses 32-bit indices, so a single compatible source/destination
batch is no longer limited to 16,383 quads by 16-bit vertex indices.

## Test-only failure seams

Deterministic allocation/GPU failure injection exists only when every relevant
translation unit is compiled with `DTEX_ENABLE_TEST_SEAMS`. The macro changes
class declarations/layout, so a seam-enabled test must compile a separate DTex
test library (all DTex sources and every consumer of DTex headers with the same
definition). It must never mix those objects with the production `dtex` library.
The production `dtex` target intentionally does not define this macro.
