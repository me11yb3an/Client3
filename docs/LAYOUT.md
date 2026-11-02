# Layout on the Client3 fork

This branch starts from Client3 commit `53c4cf8` and places the build 244 export into that tree. [docs/MIGRATION.md](MIGRATION.md) is the file-by-file result.

```text
README.md                         this project's readme
docs/CLIENT3-README.md            upstream Client3 readme, moved aside
src/                              Client3 sources, plus the build 244 overlay
save/                             save implementation
server/                           both JavaScript bundles and NOTICE.md
provenance/src-tree.rar           original export archive
provenance/checkpoint-units.mk    unmodified build 244 makefile
provenance/checkpoint-link.sh     unmodified build 244 link line
provenance/src-tree-offline_vita.c
                                  the export's other offline_vita.c, not the one units.mk compiles
units.mk                          same makefile, with in-repo paths pointed at this checkout
link.sh                           same link line, with in-repo object paths pointed here
build244.mk                       translation units excluded from the Client3 wildcards
SOURCE-MANIFEST.json              byte inventory of the export, paths unchanged
```

Upstream `rom/`, `sce_sys/`, `vita.mk`, and the other platform makefiles stay where Client3 put them. This branch does not add another `rom/` and does not delete the inherited one.

`src/offline_vita.c` is the file `units.mk` compiles (`853b5b90`). The export also had `src-tree/offline_vita.c` (`0183d4b3`), which differs by 231 lines. That copy is kept at `provenance/src-tree-offline_vita.c` so the bytes are not dropped. The five `ba_hw_r*.inc` duplicates matched after newline normalization, so the tree keeps one copy under `src/`.

## What this layout does not take from Client3-Vita

[Client3-Vita](https://github.com/Brendonm17/Client3-Vita) was used as a documentation reference: a front-page README, a `docs/` directory, and sources that can be read without unpacking an archive. No source file was copied from that repository. Its `vita.mk` was not copied. This fork already has Client3's own `vita.mk`.

## Not added

- `RS2_Ver_0.92.vpk` and its `client.pak`, `server.pak`, save, and `libshacccg.suprx`.
- Revision 377 sources.
- A repository-wide `LICENSE`.
