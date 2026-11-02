# GitHub compliance status

Date: 2 November 2026. This note records what is confirmed and what is still open. Rules cited are the GitHub Terms of Service and Acceptable Use Policies as published that day. No repository-wide license was added. History was not rewritten.

This file was written for the source export and then updated for the Client3 fork. The fork is `me11yb3an/Client3`, parent `lesleyrs/Client3`. The tip of `migration/vita-build-244` does not contain `rom/cache` or `rom/SCC1_Florestan.sf2`. Those files remain in older commits and in `lesleyrs/Client3`. `RS2_Ver_0.92.vpk` was not added. Publishing the branch does not add a license.

## Confirmed

| Item | Evidence | Rule |
| --- | --- | --- |
| The Vita migration did not add a cache, VPK, or player save | `RS2_Ver_0.92.vpk` was listed and left out. Its `client.pak`, `server.pak`, `offline_test.sav`, and `libshacccg.suprx` are not in the branch. This branch's tip also drops the inherited `rom/cache` and soundfont. | Terms D.3 for files this project would have added. Parent history still contains Client3's `rom/cache`. |
| No credentials | Source scan found no private keys or tokens. `config.ini` inside the VPK has the offline fixture password `test` and was not added. | Acceptable Use section 3. |
| Bundle notices | `server/NOTICE.md` carries the Lost City MIT notice, the rsbuf MIT notice, the rsmod-pathfinder ISC notice, and the node-forge BSD notice. | Terms D.3, for those upstream portions. |
| Engine file pin | `Environment.ts` in both bundles matches Engine-TS `e1dea19f256c7ff1a89d47024c811c755ad2184d` and not `b502762d`. | Attribution. The rest of each bundle was not rebuilt and diffed against that commit. |
| Vendored C libraries | Notices remain inside `src/thirdparty/`. Seven of those files differ from current Client3 and were overlaid from the export. | Terms D.3 for those files. |
| No AGPL source copy found | `rsc-c-vita` `src/` has no QuickJS symbols. No code line of 80 characters or more from that tree appears in this export. | AGPL-3.0 does not attach on this evidence. The embedding follows the same idea, which is not a copy of that program. |
| Project files are not verbatim Client3 copies | Recounted against Client3 `53c4cf8`: 178 export files have no Client3 path. 91 existing files match after newline normalization and were left as upstream blobs. 66 files differ in content and were overlaid. | The 178 can be described as this project's files. The 66 still contain Client3 code. |
| No blanket license | There is still no root `LICENSE`. | Terms D.6. Adding one would claim the right to license the Client3 files. |

## Unresolved

| Item | Why it remains open | What would close it |
| --- | --- | --- |
| 75 application files and 16 thirdparty files match Client3 after newline normalization | Those blobs were left untouched. Client3 still has no LICENSE, so a match is not a grant. | A license on Client3, or a written grant from the copyright holder. |
| 59 application files and 7 thirdparty files that share a Client3 path and differ in content | The files contain upstream code plus local changes. `platform/vita.c` is in this set. GitHub's fork button does not license the modifications. | The same grant, applied to the modified files. They stay attributed and are not relicensed. |
| Combined tree | The 202 project files compile against the Client3 headers. They are not byte copies, and distributing the whole tree still depends on the right to post the Client3 portion. | The same Client3 grant. Removing those files was not done. |
| Whole-bundle commit | Only `Environment.ts` was pinned to `e1dea19f`. Local `embedded/` and Barrows modules mean the bundles are not an upstream release tarball. | A recorded build recipe naming that commit plus the local patches. The bundle bytes were not rewritten. |
| node-forge package version | The bundles vendor node-forge without a version string. The notice uses the upstream BSD text current on 2 November 2026. | The package version from the machine that ran esbuild, if it is still known. |

## Local asset-free patch

A commit on `migration/vita-build-244` removes `rom/cache/` and `rom/SCC1_Florestan.sf2` from that branch, adds `rom/Roboto/NOTICE`, and stores an empty password in `rom/config.ini`. Client3's own history is unchanged, and the removed blobs remain reachable from parent commits. The files that still need a Client3 grant are listed in `docs/CLIENT3-REDISTRIBUTION.md`. The build inputs that are still missing are listed in `docs/BUILD-DEPS.md`.

## Confirmed permissions

None. The README credits conversations and reference repositories. Those notes are not a license and not a written grant. `rsc-c-vita` being AGPL-3.0 would be a permission with conditions if its code had been copied. No copied code was found, so that license is not being accepted for this tree and is not being rejected as a future obligation if a copy turns up later.

## Client3 result

Checked again on 2 November 2026 against Client3 `53c4cf8`: `lesleyrs/Client3` returns no license from the GitHub license API, and its README does not grant reuse. The earlier 73/61 count was a byte comparison of the export. Recounted with newline normalization against this commit, 91 files match and 66 differ. That is a provenance fact. It is not, by itself, a decision that GitHub must remove the repository. Terms D.3 and D.5 leave the poster responsible for having rights and for terms that allow a public fork. The official fork was created because that was requested. Publishing the modified files does not add a license.
