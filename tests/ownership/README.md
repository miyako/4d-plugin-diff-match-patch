# Ownership regressions

A synthetic 4D engine (`engine_harness.cpp`) replaces the plugin-API callback and links the real plugin sources (`4DPlugin-Diff-Match-Patch.cpp`, `support/4DPlugin-JSON.cpp`, `C_TEXT.cpp`, the bundled, unmodified `4D Plugin API/4DPluginAPI.c` and the macOS DMP sources). It tracks every engine-owned string, picture, object and handle, and runs under ASan/UBSan.

```sh
tests/ownership/run.sh . fixed        # every test must pass
D=$(mktemp -d); git archive <base-sha> | tar -x -C "$D"
tests/ownership/run.sh "$D" baseline  # named defect_*/guard_* tests must fail, control_* must pass
```

Contracts enforced:

- `PA_CreateVariable(eVK_Unistring/eVK_Picture)` allocates an empty value; a following shallow `PA_SetStringVariable`/`PA_SetPictureVariable` leaks it. Create with `eVK_Undefined` and set once.
- `PA_GetObjectProperty` returns an owned value (deep copy). Read `PA_GetLastError()` first; on `eER_NoErr`, `PA_ClearVariable` it regardless of its kind.
- `PA_SetObjectProperty` deep-copies; the caller still clears its own variable.

Limits: macOS only; engine semantics are modelled, not a real 4D engine; the Windows STL path is not exercised.
