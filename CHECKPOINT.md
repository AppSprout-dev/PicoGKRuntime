# AVX2 mesh voxelization measurement

Same `picogk.so`, this branch, 4 vCPU VM. `torquon-voxelcheck` voxel 0.50, three runs. The switch is off unless `PICOGK_MESH_AVX2` is `1`, `true`, `yes`, or `on`.

Every run hashed the section-1 spur as `293597927f580e132755fc9007f8a3b2c9d83ed2708cad235f6ff658888f2504`, active 147164, mass pass. A separate 9217-triangle sphere voxelization also matched off and on (`66ff42a42e445d07d3a0cea702b6306b1c68de0a100b7874da062ddb6f093e35`). Medians are the middle of the three sorted runs.

| threads | switch | wall s | median wall s | peak RSS kB | median RSS kB |
| --- | --- | --- | --- | --- | --- |
| max | off | 147.4601813, 148.2767195, 150.4786393 | 148.2767195 | 1007172, 1006736, 1006744 | 1006744 |
| max | on | 141.4182987, 142.1337822, 144.1055884 | 142.1337822 | 1007100, 1007144, 1007428 | 1007144 |
| 1 | off | 459.366674, 452.9534314, 452.3613542 | 452.9534314 | 1007844, 1008180, 1008048 | 1008048 |
| 1 | on | 429.1516439, 428.0079666, 429.6916194 | 429.1516439 | 1008428, 1008296, 1007976 | 1008296 |

Max-thread median is 6.1429373 s lower with the switch on (4.14%). One-thread median is 23.8017875 s lower (5.25%). The slower on-run is still faster than the faster off-run at both thread counts.
