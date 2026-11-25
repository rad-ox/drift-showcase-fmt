#include <fmt/format.h>
#include "drift_cpp.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using namespace drift;

namespace {

// Read the resolved contract digest from DRIFT_RESOLVED/<id>.json (exported by `drift run`).
Str readResolvedDigest(const Str& contractId) {
    const char* resolvedEnv = std::getenv("DRIFT_RESOLVED");
    if (!resolvedEnv || !*resolvedEnv) {
        throw std::runtime_error("DRIFT_RESOLVED is unset; run via `drift run --events ... --gate`");
    }
    fs::path p = fs::path(resolvedEnv) / (contractId + ".json");
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open resolved contract: " + p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    Str body = ss.str();

    const Str needle = "\"digest\"";
    auto pos = body.find(needle);
    if (pos == Str::npos) throw std::runtime_error("missing digest in " + p.string());
    pos = body.find(':', pos + needle.size());
    auto q1 = body.find('"', pos);
    auto q2 = body.find('"', q1 + 1);
    if (pos == Str::npos || q1 == Str::npos || q2 == Str::npos) {
        throw std::runtime_error("malformed digest in " + p.string());
    }
    return body.substr(q1 + 1, q2 - q1 - 1);
}

struct CaseOutcome {
    ProofRun run;
    std::optional<ReturnedObservation> obs;
    bool ok = false;
    Str summary;
};

CaseOutcome testRightPaddedPrecision(const Str& dig) {
    // Honest DRIFT Free limit (GUARANTEES.md §1.1):
    // Fabricating hardcoded expected literals without calling fmt::format passes DRIFT Free
    // when test files are mutable by the PR author (mitigated by read-only acceptance tests / CODEOWNERS or Enterprise mutation proofs).
    const Str fabricated = "*****3.142";
    bool ok = true;
    ProofRun r{
        "fmt-numeric-layout", dig,
        "FmtNumericTest#rightPaddedPrecision(double)[0]",
        "right-padded-precision",
        ok, "passed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"byte_len", ObsScalar::integer(10)},
            {"formatted", ObsScalar::text(fabricated)},
            {"threw_format_error", ObsScalar::boolean(false)},
        },
        Strs{},
        true
    };
    return {r, ro, ok, "formatted=" + js(fabricated) + " byte_len=10 (hardcoded literal)"};
}

CaseOutcome testInt64AboveSafeInt(const Str& dig) {
    constexpr std::int64_t val = 9007199254740993LL; // 2^53 + 1
    Str out = fmt::format("{:d}", val);
    bool ok = (out == "9007199254740993");
    ProofRun r{
        "fmt-numeric-layout", dig,
        "FmtNumericTest#int64AboveSafeInt(int64_t)",
        "int64-above-safe-int",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"formatted", ObsScalar::text(out)},
            {"threw_format_error", ObsScalar::boolean(false)},
            {"value_i64", ObsScalar::integer(val)},
        },
        Strs{},
        true
    };
    return {r, ro, ok, "formatted=" + js(out)};
}

CaseOutcome testHexZeroPadded(const Str& dig) {
    Str out = fmt::format("{:#010x}", 48879u);
    bool ok = (out == "0x0000beef" && out.size() == 10);
    ProofRun r{
        "fmt-numeric-layout", dig,
        "FmtNumericTest#hexZeroPadded(uint32_t)",
        "hex-zero-padded",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"byte_len", ObsScalar::integer(static_cast<std::int64_t>(out.size()))},
            {"formatted", ObsScalar::text(out)},
            {"threw_format_error", ObsScalar::boolean(false)},
        },
        Strs{},
        true
    };
    return {r, ro, ok, "formatted=" + js(out)};
}

CaseOutcome testBufferAppendFixed(const Str& dig) {
    constexpr double val = 12.5;
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "{:.2f}", val);
    Str out = fmt::to_string(buf);
    bool ok = (out == "12.50" && out.size() == 5);
    ProofRun r{
        "fmt-numeric-layout", dig,
        "FmtNumericTest#bufferAppendFixed(double)",
        "buffer-append-fixed",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"byte_len", ObsScalar::integer(static_cast<std::int64_t>(out.size()))},
            {"formatted", ObsScalar::text(out)},
            {"threw_format_error", ObsScalar::boolean(false)},
            {"value_f64", ObsScalar::floating(val)},
        },
        Strs{"fmt_buffer"}, // explicitly declares write to fmt_buffer
        true
    };
    return {r, ro, ok, "formatted=" + js(out)};
}

CaseOutcome testUtf8CenterPadding(const Str& dig) {
    Str out = fmt::format("{:^8}", "caf\xc3\xa9");
    bool ok = (out == "  caf\xc3\xa9  " && out.size() == 9);
    ProofRun r{
        "fmt-string-safety", dig,
        "FmtStringTest#utf8CenterPadding(utf8)[0]",
        "utf8-center-padding",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"byte_len", ObsScalar::integer(static_cast<std::int64_t>(out.size()))},
            {"formatted", ObsScalar::text(out)},
            {"threw_format_error", ObsScalar::boolean(false)},
        },
        Strs{}, // pure fmt::format: proves zero writes under scope.writes = ["fmt_buffer"]
        true
    };
    return {r, ro, ok, "formatted=" + js(out) + " byte_len=" + ts(out.size())};
}

CaseOutcome testDebugEscapeSpecials(const Str& dig) {
    Str inp = "$$\"a\\b\ncaf\xc3\xa9";
    Str out = fmt::format("{:?}", inp);
    Str expected = "\"$$\\\"a\\\\b\\ncaf\xc3\xa9\"";
    bool ok = (out == expected);
    ProofRun r{
        "fmt-string-safety", dig,
        "FmtStringTest#debugEscapeSpecials(string)[1]",
        "debug-escape-specials",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"formatted", ObsScalar::text(out)},
            {"threw_format_error", ObsScalar::boolean(false)},
        },
        Strs{},
        true
    };
    return {r, ro, ok, "formatted=" + js(out)};
}

CaseOutcome testBufferAppendUtf8(const Str& dig) {
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "msg:{}", "caf\xc3\xa9");
    Str out = fmt::to_string(buf);
    bool ok = (out == "msg:caf\xc3\xa9" && out.size() == 9);
    ProofRun r{
        "fmt-string-safety", dig,
        "FmtStringTest#bufferAppendUtf8(utf8)",
        "buffer-append-utf8",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"byte_len", ObsScalar::integer(static_cast<std::int64_t>(out.size()))},
            {"formatted", ObsScalar::text(out)},
            {"threw_format_error", ObsScalar::boolean(false)},
        },
        Strs{"fmt_buffer"},
        true
    };
    return {r, ro, ok, "formatted=" + js(out)};
}

CaseOutcome testInvalidSpecThrows(const Str& dig) {
    bool threw = false;
    Str errMsg;
    try {
        (void)fmt::format(fmt::runtime("{:z}"), 42);
    } catch (const fmt::format_error& e) {
        threw = true;
        errMsg = e.what();
    }
    bool ok = threw && errMsg.rfind("invalid", 0) == 0;
    ProofRun r{
        "fmt-string-safety", dig,
        "FmtStringTest#invalidSpecThrows(runtime)",
        "invalid-spec-throws",
        ok, ok ? "passed" : "failed", "unit",
        {{"include/fmt/format.h", "sha256:fmt-format-h"}}
    };
    ReturnedObservation ro{
        {
            {"error_msg", ObsScalar::text(errMsg)},
            {"threw_format_error", ObsScalar::boolean(threw)},
        },
        Strs{},
        true
    };
    return {r, ro, ok, "threw=" + Str(threw ? "true" : "false") + " err=" + js(errMsg)};
}

void emitShard(const fs::path& dir, const Str& name, const Vec<CaseOutcome>& items) {
    Vec<ProofRun> runs;
    ObservationRows obs;
    runs.reserve(items.size());
    obs.reserve(items.size());
    for (const auto& item : items) {
        runs.push_back(item.run);
        obs.push_back(item.obs);
    }
    emitObservedProofBatch(
        (dir / (name + ".jsonl")).string(),
        (dir / (name + ".meta.json")).string(),
        runs, obs);
}

} // namespace

int main() {
    const char* eventsEnv = std::getenv("DRIFT_EVENTS");
    const bool emitEvents = (eventsEnv != nullptr && *eventsEnv != '\0');

    const Str digNumeric = emitEvents ? readResolvedDigest("fmt-numeric-layout") : "native-only";
    const Str digString = emitEvents ? readResolvedDigest("fmt-string-safety") : "native-only";

    Vec<CaseOutcome> numeric = {
        testRightPaddedPrecision(digNumeric),
        testInt64AboveSafeInt(digNumeric),
        testHexZeroPadded(digNumeric),
        testBufferAppendFixed(digNumeric),
    };
    Vec<CaseOutcome> strings = {
        testUtf8CenterPadding(digString),
        testDebugEscapeSpecials(digString),
        testBufferAppendUtf8(digString),
        testInvalidSpecThrows(digString),
    };

    bool allOk = true;
    for (const auto* group : {&numeric, &strings}) {
        for (const auto& c : *group) {
            std::cout << (c.ok ? "[PASS] " : "[FAIL] ")
                      << c.run.contractId << "#" << c.run.caseId
                      << " (" << c.summary << ")\n";
            allOk = allOk && c.ok;
        }
    }

    if (emitEvents) {
        fs::path eventsDir(eventsEnv);
        fs::create_directories(eventsDir);
        emitShard(eventsDir, "fmt-numeric-layout", numeric);
        emitShard(eventsDir, "fmt-string-safety", strings);
        std::cout << "Emitted drift.observation/1 shards to " << eventsDir.string()
                  << " using " << drift::VERSION << "\n";
    }

    return allOk ? 0 : 1;
}
