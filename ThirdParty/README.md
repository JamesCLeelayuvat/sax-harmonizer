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
