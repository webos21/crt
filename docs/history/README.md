# Monthly history archive

`HISTORY.md` at the repository root retains October 2026's detailed completed
work. Earlier months are summarized here, preserving decisions, root causes,
validation evidence, and limitations rather than every debugging step.

| Month | Source and scope |
| --- | --- |
| [July 2026](HISTORY-2026-07.md) | Git-log backfill: 117 commits, July 27–31. The original work log has no July entries. |
| [August 2026](HISTORY-2026-08.md) | Original work log, August 2–31: shell/PAL/porting, imported C++, graphics foundations. |
| [September 2026](HISTORY-2026-09.md) | Original work log, September 1–30: isolated SDKs, graphics/media, and UI Tranches 0–3. |
| [October 2026](../../HISTORY.md) | Detailed active-month record. |

## Exact source and recovery

The immutable pre-archive source is commit
`4e5eead68048723c37e46c22d80bca43915ac093`:

- [Original August record, lines 12620–23242](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md#L12620-L23242).
- [Original September record, lines 1108–12619](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md#L1108-L12619).

From a checkout containing that commit, recover the complete source without
changing the working tree:

```sh
git show 4e5eead68048723c37e46c22d80bca43915ac093:HISTORY.md > /tmp/crt-history-before-archive.md
```

Select a month (the source is reverse chronological):

```sh
sed -n '/^## 2026-08/,$p' /tmp/crt-history-before-archive.md
sed -n '/^## 2026-09/,/^## 2026-08/{ /^## 2026-08/q; p; }' /tmp/crt-history-before-archive.md
```

Read the complete dated entry, including later corrections, before reusing a
root-cause claim. For actual reproduction, inspect the implementation revision
named by that entry and its recipes/presets, then build in a separate checkout
with the recorded host/device/toolchain prerequisites. The archive snapshot is
a source citation, not a claim that October code reproduces every earlier bug.
Source archives without Git can use the immutable links above. Historical
commands and paths in that original file intentionally retain their old names.

## Maintenance

Archive a completed month only after comparing its summary with the detailed
record. Preserve the final cause/decision, rejected or superseded explanations,
regression or acceptance entry points, host/configuration limits, and an exact
source commit. Keep current status in the appropriate live document; these
monthly records describe the state and evidence at the end of their month.
