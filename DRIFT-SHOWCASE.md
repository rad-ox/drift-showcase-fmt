# DRIFT Real-World PR Showcase — `{fmt}` (C++20 / CMake)

This showcase demonstrates **[DRIFT Free (`v0.11.0`)](https://github.com/rad-ox/drift/releases/tag/v0.11.0)** verifying real C++20 code in live GitHub Actions pull-request checks on **[`{fmt}`](https://github.com/fmtlib/fmt)** (based on upstream commit `22b7d55ac8d105edc8461e8b84ac66eefe4c802e`, MIT License), using the single-header, zero-dependency C++20 bridge **`drift_cpp.hpp` (`drift.cpp@0.11.0`)**.

- **Zero-dependency C++20 bridge:** `#include "drift_cpp.hpp"` installed directly from the SHA256-verified DRIFT `v0.11.0` release (`-I"$HOME/.local/share/drift/bridges"`) — zero vendored copies, zero external JSON libraries.
- **Typed `drift.observation/1` emission:** `drift::ObsScalar` (`text`, `integer` with lossless `int64_t` `i64` encoding above `±(2^53 - 1)`, `floating` with exact IEEE-754 `f64.bits`, `boolean`) and `drift::emitObservedProofBatch(...)` writing shards to `DRIFT_EVENTS`.
- **Fail-closed `obs.frame_missing` on `scope.writes`:** Both contracts declare `[scope] writes = ["fmt_buffer"]`, separating buffer-mutating cases (`writes = ["fmt_buffer"]`) from pure formatting cases (`writes = []`). Under `v0.11.0`, omitting the `frame` field on a narrower `writes = []` case fails closed (`obs.frame_missing` / `inconclusive` → `red`), preventing silent write-frame bypasses.
- **Important:** All demo PRs target this repository's `main` branch only — never upstream `fmtlib/fmt`.

## Hardened `v0.11.0` Policy & Local Verification

`.drift/config.toml` is initialized with `drift init rad-ox . --observed` (`pack = "core@1"`, `proof = "verified"`, `require_observations = true`).

Build `{fmt}` with CMake, compile `tests/drift_fmt_test.cpp` against the installed `drift_cpp.hpp`, and run both the native gate and the independent MIT Python verifier:

```bash
cmake -S . -B build -DFMT_TEST=OFF -DFMT_DOC=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fmt -j

g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  -I include -I "$HOME/.local/share/drift/bridges" \
  tests/drift_fmt_test.cpp build/libfmt.a -o build/drift_fmt_test

# Single-command execution + contract gate
drift run --events .drift-work/events --gate -- ./build/drift_fmt_test

# Independent MIT Python verifier parity check
drift gate .drift/contracts --events .drift-work/events --json > .drift-work/gate.json
python3 ~/.local/share/drift/verifier/drift_verify.py \
  .drift/contracts \
  --config .drift/config.toml \
  --events .drift-work/events \
  --compare-gate .drift-work/gate.json \
  --json
```

## Contract Maps (`drift map --as mermaid`)

### `fmt-numeric-layout`

```mermaid
flowchart TD
  %% drift.flow-map/1 map=d67886336243 view=cases nodes=6/6 edges=5/5
  N1["N1 fmt-numeric-layout"]
  N2[("N2 fmt_buffer")]
  C1("C1 buffer-append-fixed#58; input=#34;12.5#34;, pattern=#34;#123;#58;.2f#125;#34; -#62; byte_len=5, forma")
  C2("C2 hex-zero-padded#58; input=#34;48879#34;, pattern=#34;#123;#58;#35;010x#125;#34; -#62; byte_len=10, forma")
  C3("C3 int64-above-safe-int#58; input=#34;9007199254740993#34;, pattern=#34;#123;#58;d#125;#34; -#62; format")
  C4("C4 right-padded-precision#58; input=#34;3.1415926#34;, pattern=#34;#123;#58;#42;#62;10.3f#125;#34; -#62; byte_")
  N1 -- "E1 case" --> C1
  C1 -- "E2 writes" --> N2
  N1 -- "E3 case" --> C2
  N1 -- "E4 case" --> C3
  N1 -- "E5 case" --> C4
  MAP(["map d67886336243 6/6 nodes 5/5 edges"])
```

### `fmt-string-safety`

```mermaid
flowchart TD
  %% drift.flow-map/1 map=96fc0498e6d0 view=cases nodes=6/6 edges=5/5
  N1["N1 fmt-string-safety"]
  N2[("N2 fmt_buffer")]
  C1("C1 buffer-append-utf8#58; input=#34;café#34;, pattern=#34;msg#58;#123;#125;#34; -#62; byte_len=9, format")
  C2("C2 debug-escape-specials#58; input=#34;#36;#36;#92;#34;a#92;#92;b#92;ncafé#34;, pattern=#34;#123;#58;?#125;#34; -#62; formatt")
  C3("C3 invalid-spec-throws#58; input=#34;42#34;, pattern=#34;#123;#58;z#125;#34; -#62; error_msg=#123;#34;prefix#34;#58;#34;")
  C4("C4 utf8-center-padding#58; input=#34;café#34;, pattern=#34;#123;#58;#94;8#125;#34; -#62; byte_len=9, format")
  N1 -- "E1 case" --> C1
  C1 -- "E2 writes" --> N2
  N1 -- "E3 case" --> C2
  N1 -- "E4 case" --> C3
  N1 -- "E5 case" --> C4
  MAP(["map 96fc0498e6d0 6/6 nodes 5/5 edges"])
```

## Demo Pull Requests (against `main`)

Every push and PR runs two parallel GitHub Actions checks so you can inspect the exact contrast directly in the PR Checks panel:

1. **`Native C++20 Tests`** — Builds `{fmt}` with CMake and executes `./build/drift_fmt_test` directly without the DRIFT gate.
2. **`DRIFT v0.11.0 Gate + Independent MIT Verifier`** — Installs the SHA256-verified `v0.11.0` release, builds `./build/drift_fmt_test` against `$HOME/.local/share/drift/bridges/drift_cpp.hpp`, runs `drift run --events .drift-work/events --gate -- ./build/drift_fmt_test`, and confirms exact verdict parity with `drift_verify.py --compare-gate .drift-work/gate.json --json`.

| PR | Branch | Scenario | Native C++20 Tests | DRIFT v0.11.0 Gate + MIT Verifier |
|:---:|---|---|:---:|:---:|
| `#1` | `demo/false-green` | Introduces a real off-by-one padding regression in `include/fmt/format.h` (`write_padded`) and weakens two native assertions (`!out.empty()`) so the C++20 binary exits `0` — while omitting `frame` on a pure `writes = []` case (`utf8-center-padding`). DRIFT catches both: **`obs_red`** re-judge on `right-padded-precision` and **`obs.frame_missing`** fail-closed on `utf8-center-padding` (`parity.ok = true`) | `PASS` | **`FAIL`** (`obs_red` + `obs.frame_missing`; verifier parity `true`) |
| `#2` | `demo/valid-fix` | Legitimate behavior-preserving refactor in `include/fmt/format.h` extracting `compute_padding(...)` while keeping all numeric, UTF-8, and write-frame contracts intact | `PASS` | `PASS` (`8/8 verified`; verifier parity `true`) |
| `#3` | `demo/free-limit` | **Honest DRIFT Free limit:** keeps the `write_padded` regression unfixed and passes hardcoded expected literals directly into `ObsScalar` without invoking `fmt::format` — showing what DRIFT Free alone does **not** block without read-only acceptance tests (`CODEOWNERS` / CI boundary) or Enterprise mutation proofs ([`GUARANTEES.md §1.1`](https://github.com/rad-ox/drift/blob/main/GUARANTEES.md)) | `PASS` | `PASS` (not blocked; honest Free boundary) |
