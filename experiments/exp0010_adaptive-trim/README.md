# exp0010_adaptive-trim

Scene-aware trimming gate.

## Hypothesis

exp0009's mixed result (AGG improved but 01 and 10 regressed) implies trimming is good when correspondences are plentiful (urban dense) and bad when they're scarce (highway, residential). The dense/sparse signal is `n_corrs / n_source`.

Threshold: only trim if `n_corrs > 0.40 * n_source`. Otherwise keep all correspondences.

Expected: 01 and 10 recover toward exp0008 levels (1.90 / 0.76); urban sequences (07, 05) keep exp0009's wins. AGG should drop slightly.

## Changes vs parent

Single-line guard added to the trim block in `icp()`:
```cpp
if (iter >= 1 && corrs.size() >= 50 &&
    corrs.size() > size_t(source.size() * 0.40)) { ... }
```

Everything else unchanged.

## Determinism notes

The threshold is a pure function of input data (source size, NN search results). No memory-address dependency. The branch may or may not be taken on a given frame, but it's deterministic per frame.
