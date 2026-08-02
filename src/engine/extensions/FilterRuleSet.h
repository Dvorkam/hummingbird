#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/net/RequestFilter.h"

namespace Hummingbird::Engine {

// Reading a static filter ruleset file (story 9.4.1).
//
// The format is MV3's `declarativeNetRequest` rule shape, trimmed to what the
// matcher supports:
//
//   [
//     { "id": 1,
//       "condition": { "requestDomains": ["doubleclick.net"],
//                      "urlFilter": "/ads/",
//                      "resourceTypes": ["image", "script"],
//                      "domainType": "thirdParty" },
//       "action": { "type": "block" } }
//   ]
//
// `action` is accepted and required to be `block`, even though block is the only
// thing the engine can do. Making the file say so keeps it honest against MV3
// and means adding `allow` or `redirect` later does not silently reinterpret
// every rule already written.
struct RuleSetParseResult {
    std::vector<Core::FilterRule> rules;
    // One message per rule that was rejected, naming the index. Rejected rules
    // are skipped rather than failing the whole file: one typo in a fifty-rule
    // list should cost that rule, not the list. But it is never silent — a rule
    // that does not load is a rule that does not block, and the difference is
    // invisible from the outside.
    std::vector<std::string> warnings;
    // Set only when the file could not be read as a rule array at all.
    std::string fatal_error;

    bool ok() const { return fatal_error.empty(); }
};

RuleSetParseResult parse_filter_rule_set(std::string_view json);

}  // namespace Hummingbird::Engine
