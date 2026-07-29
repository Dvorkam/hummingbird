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

// A short, log-friendly reason. NEVER shown to the page: per spec a blocked
// fetch rejects with an opaque failure, and saying why would leak exactly the
// cross-origin information CORS exists to withhold.
std::string_view describe(Decision decision);

}  // namespace Hummingbird::Core::Cors
