#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/net/HttpHeaders.h"
#include "core/net/Origin.h"

namespace Hummingbird::Core::Cors {

// Cross-Origin Resource Sharing, story 9.2.1.
//
// CORS answers one question: may the page READ this response? A cross-origin
// request usually still reaches the server — the browser sends it and then
// refuses to hand the answer to script unless the server opted in. That is why
// enforcement lives on the RESPONSE path, and why a blocked response must be
// discarded whole: leaking a status code alone would turn any site into a port
// scanner for the user's private network.
//
// Deliberately a pure-function module with no I/O: every decision here is a
// function of the request and the response headers, which makes the matrix of
// cases testable without a server.
//
// SCOPE (M9): fetch() only. Top-level navigations are NOT subject to CORS — you
// may follow a link anywhere — and subresource loads (<img>, <script>) have
// their own rules that M9 does not touch.

// Whether a request carries the user's ambient credentials (cookies).
// Mirrors the Fetch standard's request credentials mode; the default for
// fetch() is SameOrigin, which is why a cross-origin fetch does not carry
// cookies unless the page asks for it.
enum class Credentials {
    Omit,
    SameOrigin,
    Include,
};

// Why a request was refused. Kept specific because "CORS failed" is the single
// least actionable error message on the web, and the engine can do better in a
// log even though the spec requires the *page* be told nothing.
enum class Decision {
    Allowed,
    // No Access-Control-Allow-Origin at all: the server never opted in.
    MissingAllowOrigin,
    // Present, but names a different origin.
    OriginMismatch,
    // `*` cannot authorize a credentialed request: the server must name the
    // origin explicitly, or it has not thought about who it is answering.
    WildcardWithCredentials,
    // Credentialed request to a server that did not send
    // Access-Control-Allow-Credentials: true.
    CredentialsNotAllowed,
    // Preflight only: the method or a header was not in the allow list.
    MethodNotAllowed,
    HeaderNotAllowed,
};

// True when the two URLs share scheme, host and port — in which case CORS does
// not apply at all. A URL with no tuple origin (opaque) is never same-origin
// with anything, including itself.
bool is_same_origin(std::string_view request_url, std::string_view document_url);

// A "simple" request needs no preflight: the browser could have made it with a
// plain form, so allowing it adds no new capability. Everything else asks
// permission first with an OPTIONS request.
bool is_simple_request(std::string_view method, const HttpHeaders& headers);

// Whether `name` is a CORS-safelisted request header — one a form could already
// send, so it never triggers a preflight on its own.
bool is_safelisted_request_header(std::string_view name, std::string_view value);

// Checks a cross-origin response's Access-Control-* headers against the request
// that produced it. `origin` is the initiating document's origin, serialized.
Decision check_response(const HttpHeaders& response_headers, std::string_view origin, Credentials credentials);

// Checks a preflight (OPTIONS) response, which must additionally allow the real
// request's method and any non-safelisted headers it intends to send.
Decision check_preflight(const HttpHeaders& response_headers, std::string_view origin, Credentials credentials,
                         std::string_view method, const HttpHeaders& request_headers);

// The request headers that need preflight approval: everything not safelisted.
// Lowercased, in sorted order, so the Access-Control-Request-Headers value is
// deterministic (which matters for the preflight cache key in 9.2.2).
std::vector<std::string> headers_needing_preflight(const HttpHeaders& headers);

// --- response header exposure (story 9.2.4) ----------------------------------
//
// The other direction from check_response: that asks what the SERVER allows for
// the request, this asks which response headers the PAGE may observe. They are
// separate questions, and the second is the one that is easy to forget — a
// permitted response still must not hand over everything it carries.

// The seven headers any cross-origin response exposes without being asked.
// Chosen because a page could learn them by other means anyway.
bool is_safelisted_response_header(std::string_view name);

// Headers script may NEVER read, whatever the server says — not via
// Access-Control-Expose-Headers, not via `*`. `Set-Cookie` is the whole reason
// this category exists: reading it would hand a page another origin's session.
bool is_forbidden_response_header(std::string_view name);

// Removes fields that Fetch never exposes to script, including for a
// same-origin response. Cookie processing must happen before this boundary.
HttpHeaders filter_forbidden_response_headers(const HttpHeaders& response_headers);

// The subset of `response_headers` a cross-origin page may read: the safelist,
// plus anything Access-Control-Expose-Headers names, minus the forbidden set.
// Only call this for cross-origin responses. Same-origin responses need only
// `filter_forbidden_response_headers`.
HttpHeaders filter_exposed_headers(const HttpHeaders& response_headers, Credentials credentials);

// A short, log-friendly reason. NEVER shown to the page: per spec a blocked
// fetch rejects with an opaque failure, and saying why would leak exactly the
// cross-origin information CORS exists to withhold.
std::string_view describe(Decision decision);

}  // namespace Hummingbird::Core::Cors
