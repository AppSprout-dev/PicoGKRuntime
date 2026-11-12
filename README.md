# Welcome to PicoGK Runtime

PicoGK Runtime is the C++ backbone to [PicoGK](https://picogk.org) a compact, robust geometry kernel for [Computational Engineering](https://leap71.com/computationalengineering/). 

This repo is hosted at: https://github.com/leap71/PicoGKRuntime

You will usually not clone this directory, unless you want to compile the PicoGK Runtime yourself instead of [using the available installers.](https://github.com/leap71/PicoGK/releases)

PicoGK Runtime is based on the [OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb) and [GLFW](https://github.com/glfw/glfw) open source projects.

For more information, [please see the main PicoGK repository](https://github.com/leap71/PicoGK).

PicoGK was developed by Lin Kayser, and is maintained by [LEAP 71](www.leap71.com). PicoGK and PicoGK Runtime are released under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) (SPDX Apache-2.0).

## RenderImplicit threading

`Voxels_RenderImplicit` evaluates the signed-distance callback on oneTBB worker threads by default. The callback must be pure in the sample point: no shared mutable state and no thread-affine resources. A stateful implicit can race. Set `PICOGK_IMPLICIT_SERIAL` to force the original single-threaded loop. `Voxels_IntersectImplicit` calls `RenderImplicit`, so it uses the same callback contract.

`PICOGK_IMPLICIT_SERIAL` is read with `getenv` on every `RenderImplicit` call. It is not cached.

The variable is trimmed of ASCII whitespace (space, tab, CR, LF, VT, FF) and compared case-insensitively:

- Serial: `1`, `true`, `yes`, `on`
- Parallel: unset, empty, `0`, `false`, `no`, `off`

Any other value selects the serial path and prints one warning to stderr.

Two behaviors of the parallel path are part of the contract. A callback that samples the field it is rendering sees that field's state from before the current chunk is written back. If a worker throws, chunks already written stay written and the chunk in flight is not, which is a different partial grid than the serial loop. The serial loop writes each voxel before the next sample.

## Mesh voxelization

`Voxels_RenderMesh` keeps the scalar OpenVDB mesh voxelizer unless `PICOGK_MESH_AVX2` is set to `1`, `true`, `yes`, or `on` (trimmed, case-insensitive) on an x86-64 machine with AVX2. Unset, empty, `0`, `false`, `no`, and `off` stay on the scalar path. Any other value stays on the scalar path and prints one warning to stderr. The switch is read once per process. Degenerate triangles stay on the scalar path either way. On the AVX2 path, a non-degenerate triangle whose voxel box fits in 2^20 cells keeps its visit set in a side table instead of the scratch prim-id tree. A larger triangle keeps the prim-id visit set, and the side table's hash is released back to 4096 slots after each triangle. The distance and index writes, including the minimum-index tie break, stay the same. Other architectures compile the scalar path only.
