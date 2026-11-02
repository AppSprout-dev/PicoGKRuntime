# WIP: narrow-band skip in `RenderImplicit`

Paused before pull requests. No speedup is claimed. The hash gate is not finished.

Candidate commit on this branch, before this note: `36b83099993464614358800e9c570f6b6f2b9a5b`.
Baseline runtime the timings used: `0f26321c18ed878a7820ef769c38fd5d49d39242`.
Both `picogk.so` files were produced by `picogk-bench` `scripts/build-runtime.sh`. `build-info.json` for both says `trace: false`, `dirty: false`, and `picogk_cxx_flags` `-O3 -DNDEBUG -std=gnu++20 -fPIC -O3 -fvisibility=hidden -fvisibility-inlines-hidden`.

## Done

- Read-only check against `leap71/PicoGKRuntime` `main`: both sides were `0f26321c18ed878a7820ef769c38fd5d49d39242` (ahead 0, behind 0). Nothing was synced.
- `Voxels::RenderImplicit` in `Source/PicoGKVdbVoxels.h` can skip whole blocks when probe samples are consistent with Lipschitz 1.05 and the block is strictly outside the narrow band (no write on an empty tree, same inactive `+background`) or strictly inside (`grid->fill` of inactive `-background`). Anything else uses the original per-voxel loop. The callback stays on the calling thread.
- If all eight corners are already inside the band, the original loop runs immediately.
- `PICOGK_IMPLICIT_REFERENCE=1` forces the original loop. `PICOGK_IMPLICIT_STATS=1` prints `PICOGK_IMPLICIT_STATS domain=... calls=... skipped=...` on stderr. Neither flag is a new API parameter.
- No public API or exported C ABI change. The C# callback is not parallelized.

## Left

- Hash gate not run: `torquon-m48-implicit`, `torquon-m48-lattice`, `torquon-voxelcheck` at 0.50 mm and 0.30 mm, Phenara bay at 0.50 mm on the candidate, section 5u housing.
- Phenara 0.50 mm before-library repeats stopped during r4. No r4 JSON was written. The after library was not started for 0.50 mm.
- No five-run timings for M48, voxelcheck, or section 5u. No section 5u callback counts.
- Pull requests were not opened.

## Machine

From `/proc/cpuinfo` and `/proc/meminfo` on the VM that produced the numbers below:

```
cpu family	: 6
model		: 207
model name	: Intel(R) Xeon(R) Processor
stepping	: 2
cpu MHz		: 2400.000
cache size	: 327680 KB
```

`nproc` is 4. `MemTotal: 16387280 kB`. `Ubuntu 24.04.5 LTS`, kernel `6.12.94+`. Compiler and CMake strings are the `build-info.json` values above. A Phenara log line recorded `C# Framework: .NET 10.0.14`.

This is the same headline CPU family, model, vCPU count, and memory size as the recorded baseline machine. The before column is still this VM's re-run, not the historical `results/BASELINE.md` times.

## Hash gate

Recorded baseline hashes, and whether a run on this VM matched them. "match" means the run JSON `grid_hash` was that string.

| build | recorded hash | active | 1 thread | max threads |
| --- | --- | --- | --- | --- |
| shapekernel-gyroid 0.50, before and after | `45339034581a659cc7f8976136b97dd12d7d3764029dd8302c5011e88ada8b3f` | 39118 | match | match |
| shapekernel-basepipe 0.50, before and after | `12be753db6305f367c35f2d55a088932b90734acb4855934328f4d3b3e9a25cb` | 1799052 | match | match |
| phenara-export-bay 1.00, before and after | `92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f` | 19082991 | match | match |
| phenara-export-bay 0.50, before only | `265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758` | 76862227 | match | match |
| phenara-export-bay 0.50, after | same recorded hash | 76862227 | not run | not run |
| torquon-m48-implicit 0.50 | `82eee2a5748d9d4a438479acdf3c43d73b205a14585789a7bee9df2c41ebb6b3` | 613542 | not run | not run |
| torquon-m48-lattice 0.50 | `7cef6d6ca3da4c9512e51749f0300a67a8955f129300893a0f1c550328936312` | 574339 | not run | not run |
| torquon-voxelcheck 0.30 | `fbf02e4c61d166a850dae113ec0a3171a897aa3b16b984d2a4c11ff53d86e331` | 442760 | not run | not run |
| torquon-voxelcheck 0.50 | `293597927f580e132755fc9007f8a3b2c9d83ed2708cad235f6ff658888f2504` | 147164 | not run | not run |
| section 5u housing | `b37778d917477fbf77c7ad9e7f2c9b9edde5f1dda3367b007f68360c1c64bc6c` | 3990988 | not run | not run |

## Measured runs

Threads were `max` except the resample rows, which say `t1` or `tmax`. `wall_s` is the timed section. `rss_peak_kb` is GNU time peak RSS. Warmup rows are included and are not part of a median.

`/tmp/phenara-measure.log` printed:

```
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=7.6031888 rss=2923024
RESAMPLE hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991
RESAMPLE hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=7.0521024 rss=2861192
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=7.1702748 rss=2861436
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=8.0408895 rss=2930640
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.9354643 rss=2913004
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.9895717 rss=2914628
MEDIAN before phenara 1.0 n=5 wall=7.0521 rss=2913004
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.8701689 rss=2820680
RESAMPLE hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991
RESAMPLE hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.9134924 rss=2865608
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.6530648 rss=2877540
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.820465 rss=2925520
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.8081574 rss=2951340
HASH status=ok hash=92fbc02eec000615ba1bdceb90fa4e8bc2243b232a34b10e39b4d0081a43817f active=19082991 wall=6.7437066 rss=2863024
MEDIAN after phenara 1.0 n=5 wall=6.8082 rss=2877540
HASH status=ok hash=265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758 active=76862227 wall=22.3225528 rss=10923604
RESAMPLE hash=265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758 active=76862227
RESAMPLE hash=265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758 active=76862227
HASH status=ok hash=265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758 active=76862227 wall=21.9296524 rss=10677460
HASH status=ok hash=265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758 active=76862227 wall=22.5102831 rss=10666480
HASH status=ok hash=265e741abd372dec70c188d69e3b67391e48123fd1d8f22351bd57db8270b758 active=76862227 wall=21.9574232 rss=10732120
```

The first Phenara 1.00 block is the before library (warmup, then 1-thread resample, then max-thread resample, then r1 through r5). The second block is the after library. The 0.50 block is before only: warmup, 1-thread resample, max-thread resample, r1, r2, r3. r4 started and was killed with no result line.

`process_wall_s` from the same run JSON files, Phenara only:

```
before 1.00 warmup 37.08
before 1.00 r1 36.3 r2 36.71 r3 37.32 r4 36.28 r5 35.71
after 1.00 warmup 35.7
after 1.00 r1 35.55 r2 35.77 r3 35.61 r4 35.86 r5 35.96
before 0.50 warmup 138.49
before 0.50 r1 137.11 r2 139.16 r3 136.2
```

ShapeKernel fields `wall_s` and `rss_peak_kb` from the run JSON (there is no separate median log line):

```
before shapekernel-gyroid 0.5 max warmup wall_s=0.0319734 rss_peak_kb=84864
before shapekernel-gyroid 0.5 max r1 wall_s=0.0274926 rss_peak_kb=85400
before shapekernel-gyroid 0.5 max r2 wall_s=0.0269433 rss_peak_kb=83312
before shapekernel-gyroid 0.5 max r3 wall_s=0.0275787 rss_peak_kb=84432
before shapekernel-gyroid 0.5 max r4 wall_s=0.0294316 rss_peak_kb=92972
before shapekernel-gyroid 0.5 max r5 wall_s=0.0278818 rss_peak_kb=85700
before shapekernel-basepipe 0.5 max warmup wall_s=4.1893475 rss_peak_kb=379760
before shapekernel-basepipe 0.5 max r1 wall_s=4.2159443 rss_peak_kb=385948
before shapekernel-basepipe 0.5 max r2 wall_s=4.1674122 rss_peak_kb=375488
before shapekernel-basepipe 0.5 max r3 wall_s=4.409445 rss_peak_kb=382848
before shapekernel-basepipe 0.5 max r4 wall_s=4.1586669 rss_peak_kb=390352
before shapekernel-basepipe 0.5 max r5 wall_s=4.1677912 rss_peak_kb=379028
after shapekernel-gyroid 0.5 max warmup wall_s=0.044404 rss_peak_kb=88404
after shapekernel-gyroid 0.5 max r1 wall_s=0.0413039 rss_peak_kb=96464
after shapekernel-gyroid 0.5 max r2 wall_s=0.0383932 rss_peak_kb=88324
after shapekernel-gyroid 0.5 max r3 wall_s=0.0395674 rss_peak_kb=91288
after shapekernel-gyroid 0.5 max r4 wall_s=0.0382798 rss_peak_kb=85936
after shapekernel-gyroid 0.5 max r5 wall_s=0.0378843 rss_peak_kb=90912
after shapekernel-basepipe 0.5 max warmup wall_s=4.156659 rss_peak_kb=373812
after shapekernel-basepipe 0.5 max r1 wall_s=4.1736946 rss_peak_kb=376592
after shapekernel-basepipe 0.5 max r2 wall_s=4.0831921 rss_peak_kb=378088
after shapekernel-basepipe 0.5 max r3 wall_s=4.1610315 rss_peak_kb=378956
after shapekernel-basepipe 0.5 max r4 wall_s=4.0776709 rss_peak_kb=377620
after shapekernel-basepipe 0.5 max r5 wall_s=4.0635411 rss_peak_kb=371736
```

## Callback counts

`/tmp/stats-gyroid-ref.err` (`PICOGK_IMPLICIT_REFERENCE=1` on the candidate):

```
PICOGK_IMPLICIT_STATS domain=166375 calls=166375 skipped=0
PICOGK_IMPLICIT_STATS domain=103823 calls=103823 skipped=0
```

`/tmp/stats-gyroid-fast.err` (candidate fast path):

```
PICOGK_IMPLICIT_STATS domain=166375 calls=124239 skipped=42136
PICOGK_IMPLICIT_STATS domain=103823 calls=103823 skipped=0
```

`/tmp/stats-pipe-ref.err` and `/tmp/stats-pipe-fast.err` are empty. No basepipe, Phenara, M48, voxelcheck, or section 5u callback counts were captured. `domain` is the old per-voxel count. `skipped` is `domain - calls`.

## Caveats

- A block is pruned only when the probes that were taken are consistent with Lipschitz 1.05. A spike between probes can still be dropped. PicoGK implicits are not guaranteed signed-distance fields. The gyroid path often cannot prune.
- On these runs the candidate gyroid walls are higher than the baseline walls. Basepipe is close. Phenara 1.00 has a lower logged median on the candidate. None of that is a gate result.
- Parallelizing the C# callback would change the thread-safety contract and was not done.
