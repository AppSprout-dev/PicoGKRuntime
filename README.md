# PicoGK Runtime

PicoGK Runtime is the native C++ backbone of [PicoGK](https://picogk.org/), a compact geometry kernel for [Computational Engineering](https://leap71.com/computationalengineering/), developed by [LEAP 71](https://leap71.com/).

This repository contains the C++ implementation and native interface of **PicoGK 2.x**, which is moving into maintenance mode.

## Building PicoGK Runtime

PicoGK Runtime is not distributed or built independently. It is included as a Git submodule in [PicoGK.Legacy](https://github.com/leap71/PicoGK.Legacy), which manages the native dependencies, cross-platform compilation, testing, and distribution.

If you want to build PicoGK 2.x from source, please use the PicoGK.Legacy superproject.

The runtime is based on the open-source [OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb), [GLFW](https://github.com/glfw/glfw), and [Dear ImGui](https://github.com/ocornut/imgui) projects.

## PicoGK Repositories

- [**PicoGK**](https://github.com/leap71/PicoGK) — The existing C# geometry library and public API for PicoGK 2.x.
- [**PicoGKRuntime**](https://github.com/leap71/PicoGKRuntime) — The native C++ implementation of PicoGK 2.x (this repository).
- [**PicoGK.Legacy**](https://github.com/leap71/PicoGK.Legacy) — The unified build and distribution system for PicoGK 2.x.
- [**PicoGK.Core**](https://github.com/leap71/PicoGK.Core) — The next-generation geometry kernel, featuring a redesigned architecture and native runtime.

PicoGK 2.x will continue to receive maintenance and bug fixes, while new architectural development takes place in PicoGK.Core.

## License

PicoGK was originally developed by Lin Kayser and is maintained by [LEAP 71](https://leap71.com/).

PicoGK and PicoGK Runtime are released under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) (SPDX: Apache-2.0).

## RenderImplicit threading

`Voxels_RenderImplicit` evaluates the signed-distance callback on oneTBB worker threads by default. The callback must be pure in the sample point: no shared mutable state and no thread-affine resources. A stateful implicit can race. Set `PICOGK_IMPLICIT_SERIAL` to force the original single-threaded loop. `Voxels_IntersectImplicit` calls `RenderImplicit`, so it uses the same callback contract.

`PICOGK_IMPLICIT_SERIAL` is read with `getenv` on every `RenderImplicit` call. It is not cached.

The variable is trimmed of ASCII whitespace (space, tab, CR, LF, VT, FF) and compared case-insensitively:

- Serial: `1`, `true`, `yes`, `on`
- Parallel: unset, empty, `0`, `false`, `no`, `off`

Any other value selects the serial path and prints one warning to stderr.

Two behaviors of the parallel path are part of the contract. A callback that samples the field it is rendering sees that field's state from before the current chunk is written back. If a worker throws, chunks already written stay written and the chunk in flight is not, which is a different partial grid than the serial loop. The serial loop writes each voxel before the next sample.

`Voxels_RenderImplicitBatch` and `Voxels_IntersectImplicitBatch` are additive. They take `PKPFnfSdfBatch`, a callback that reads a contiguous point array and writes one distance per point. The per-point entries and their behavior are unchanged. The batch path uses the same `PICOGK_IMPLICIT_SERIAL` switch, the same 2^20 writeback chunk, and the same x, y, z order. Workers call the callback on disjoint slices of the chunk and do not write the grid. The calling thread writes the chunk after every slice has returned. A batch callback that samples the field sees that same pre-chunk state.

## Mesh voxelization

`Voxels_RenderMesh` keeps the scalar OpenVDB mesh voxelizer unless `PICOGK_MESH_AVX2` is set to `1`, `true`, `yes`, or `on` (trimmed, case-insensitive) on an x86-64 machine with AVX2. Unset, empty, `0`, `false`, `no`, and `off` stay on the scalar path. Any other value stays on the scalar path and prints one warning to stderr. The switch is read once per process. Degenerate triangles stay on the scalar path either way. On the AVX2 path, a non-degenerate triangle whose voxel box fits in 2^20 cells keeps its visit set in a side table instead of the scratch prim-id tree. A larger triangle keeps the prim-id visit set, and the side table's hash is released back to 4096 slots after each triangle. The distance and index writes, including the minimum-index tie break, stay the same. Other architectures compile the scalar path only.
