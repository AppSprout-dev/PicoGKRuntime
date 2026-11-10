# WIP checkpoint: AVX2 mesh voxelization

Paused before another measurement. No pull request. The switch stays off unless `PICOGK_MESH_AVX2` is `1`, `true`, `yes`, or `on`.

This commit builds a different binary from the one that was timed. The timed `PICOGK_MESH_AVX2=1` library kept the AVX2 flood inside `voxelizeTriangle`. That default-off path was slower than stock, so the flood was moved into `voxelizeTriangleAvx` (`noinline`) and the scalar loop was put back. That split compiled on this machine at 2026-11-10 07:18:41 UTC. It has not been timed.

## Measured on this VM (4 vCPU)

`torquon-voxelcheck` voxel 0.50. The hash is the section-1 spur, SHA-256 `293597927f580e132755fc9007f8a3b2c9d83ed2708cad235f6ff658888f2504`, active 147164, mass pass, on every run below. Medians are the middle of three runs.

Stock `ac184195`, switch absent:

| threads | wall s | peak RSS kB |
| --- | --- | --- |
| max | 159.4451476, 156.8538366, 162.5993624 (median 159.4451476) | 1009468, 1009644, 1009208 (median 1009468) |
| 1 | 574.0916635, 539.0496362, 534.9401618 (median 539.0496362) | 1008140, 1008272, 1008244 (median 1008244) |

Same tree with the AVX2 flood still inside `voxelizeTriangle`, `PICOGK_MESH_AVX2=1`:

| threads | wall s | peak RSS kB |
| --- | --- | --- |
| max | 157.5857464, 149.4192607, 149.264994 (median 149.4192607) | 1007432, 1007232, 1007520 (median 1007432) |
| 1 | 445.3578777, 459.6388858, 440.4075089 (median 445.3578777) | 1008820, 1008692, 1008844 (median 1008820) |

Same binary, switch unset, max threads only: 175.2812642, 172.8249449, 164.8264962 s (median 172.8249449), RSS 1010192, 1010420, 1009876 kB (median 1010192). Spur hash unchanged. That is slower than stock.

Other builds, one run each, switch off and on, 1 thread and max, same spur-style grid hash within each build: shapekernel-gyroid `45339034581a659cc7f8976136b97dd12d7d3764029dd8302c5011e88ada8b3f` (active 39118), shapekernel-basepipe `12be753db6305f367c35f2d55a088932b90734acb4855934328f4d3b3e9a25cb` (active 1799052), torquon-m48-implicit `82eee2a5748d9d4a438479acdf3c43d73b205a14585789a7bee9df2c41ebb6b3` (active 613542), torquon-m48-lattice `7cef6d6ca3da4c9512e51749f0300a67a8955f129300893a0f1c550328936312` (active 574339). 1 thread and max matched each other as well.

A closed sphere (9217 triangles, voxel 0.50) hashed equal off and on: `66ff42a42e445d07d3a0cea702b6306b1c68de0a100b7874da062ddb6f093e35`. A coarse sphere (49 triangles, radius 250 mm) also matched: `a33ff16abd55fe23b4f60346a09c17d9f468a1a9368b478e12345700ea583894`. Those RenderMesh times were about 0.1 s and about 1.3 s and did not show a timing gap. 9720 closest-point queries matched the shipped scalar `closestPointOnTriangleToPoint` in point bits and in `float(lengthSqr)` (0 mismatches).

## Not done

Do not treat the 149.4 s / 445.4 s medians as the result of this commit. Re-time this binary on a quiet machine: `torquon-voxelcheck` 0.50, three runs at max threads and three at 1 thread with `PICOGK_MESH_AVX2=1`, and three max-thread runs with the variable unset. Open a pull request only if the on medians beat the stock medians above and the unset medians do not regress, and the spur hash stays the same. Re-check one RenderMesh hash on this binary.

No HIP kernel is in this branch. This VM has no GPU. A GPU path would sit behind a default-off switch in `Voxels::roFloatGridFromMesh` and would still have to scatter an irregular narrow band into the VDB tree. It was not written.
