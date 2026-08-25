# Pre-generated `parser.c`/`parser.h`

`src/xkbcomp/parser.y` is libxkbcommon's only real generated-file
requirement (confirmed by reading `meson.build` directly: `keywords.c` is
also gperf-generated but ships pre-committed in the upstream repo itself,
and no other `.c`/`.h` the core library needs goes through a code generator
at all). Upstream's own Meson build always regenerates `parser.c`/
`parser.h` from `parser.y` via a real `bison`/`byacc` invocation at build
time -- this project has no such host-tool dependency anywhere else
(`tools/build_wayland.py`'s own docstring is explicit about that), and no
Meson to drive the regeneration in the first place, so pinning to a real,
pre-generated output committed alongside the port is the right fix here
(matching how several real yacc/bison-based open-source projects ship a
pre-generated parser precisely to spare downstream builds a bison
dependency), not a new bison-as-host-tool port (out of proportion to what
this one grammar file actually needs).

Generated 2026-08-25 by the user directly (this session's own sandboxed
WSL environment had no `bison`/interactive `sudo` to install one; the user
ran `sudo apt-get install -y bison` in their own terminal, then this
generation command was run against it):

```
bison --defines=parser.h -o parser.c -p _xkbcommon_ src/xkbcomp/parser.y
```

`bison (GNU Bison) 3.8.2` (Ubuntu `resolute`), against
`libxkbcommon` commit `dd642359f8d43c09968e34ca7f1eb1121b2dfd70`
(the real, `git ls-remote`-confirmed dereferenced commit of the annotated
tag `xkbcommon-1.9.2` -- see `../recipe.json`'s own `source.expected_commit`
and its own notes for the same pinning discipline `libcrtgfx/third_party/
wayland/recipe.json` already established), the exact command real upstream
`meson.build`'s own `yacc_gen = generator(bison, ...)` uses (confirmed by
reading it directly, not assumed): `--defines=@OUTPUT1@ -o @OUTPUT0@
-p _xkbcommon_ @INPUT@`.

**Re-pin discipline**: if `libcrtgfx/third_party/xkbcommon/recipe.json`'s
own pinned commit ever moves, these two files need regenerating from the
new commit's own `src/xkbcomp/parser.y` with the identical command above --
`tools/build_xkbcommon.py` does not do this itself (no bison dependency,
by design), so a stale `parser.c` here would silently keep building against
an old grammar. Diff the new commit's own `parser.y` against the one this
generation was run against first; if it's unchanged, these two files don't
need touching even though the rest of the port is being re-pinned.
