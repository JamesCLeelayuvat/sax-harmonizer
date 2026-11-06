# Third-party libraries

Vendored copies (no git submodules). Both are header-only and MIT licensed.

| Library | Source | Commit |
|---|---|---|
| signalsmith-stretch | https://github.com/Signalsmith-Audio/signalsmith-stretch | 57b93f4 (v1.3.2) |
| signalsmith-linear | https://github.com/Signalsmith-Audio/linear | 547f4a6 |

Header search paths (set in `NewProject.jucer`, Debug and Release):

```
../../ThirdParty/signalsmith-stretch/include
../../ThirdParty/signalsmith-linear/include
```

Usage:

```cpp
#include <signalsmith-stretch/signalsmith-stretch.h>
```

To update, re-download both repos into these folders, delete their `.git` directories, and update the table above.

## STK (benchmark only)

`stk/` is the subset of The Synthesis ToolKit in C++ (Perry R. Cook and Gary P. Scavone, MIT-style license in `stk/LICENSE`) that `stk::Saxofony` needs. It generates the input tones for `Tests/SystemBenchmark` and isn't compiled into the app. Source: https://github.com/thestk/stk, tag 5.0.1 (1cb2c81). Unmodified.
