// Ad Block Lite (story 9.4.2) — the working proof of 9.4.1's request filter.
//
// This file is nearly empty ON PURPOSE, and that is the point of the feature
// rather than a shortcut. The blocking lives entirely in `rules.json`, which the
// browser reads at startup and matches natively in C++. No extension code runs
// on the request path, so this blocker cannot slow a request down, cannot hang
// one, and keeps working identically whether or not this script does anything.
//
// It also means the rules survive a restart with no persistence anywhere: they
// are DECLARED, not registered, so they are simply read again next run.
//
// If you are looking for the list, it is in `rules.json`. If you are looking for
// the matcher, it is `src/core/net/RequestFilter.h`. There is deliberately
// nothing in between.
//
// The one thing worth doing here is saying so out loud in the log, so that a
// silent extension is distinguishable from a broken one.
console.log("[ad-block-lite] active; rules are declared in rules.json and matched natively");
