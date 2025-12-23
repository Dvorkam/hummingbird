#include "platform/StubNetwork.h"

void StubNetwork::shutdown() {
    if (m_stopping.exchange(true, std::memory_order_relaxed)) return;
    join_all();
}

void StubNetwork::join_all() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lg(m_threads_mutex);
        threads.swap(m_threads);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

void StubNetwork::get(const std::string& url, std::function<void(std::string)> callback) {
    if (m_stopping.load(std::memory_order_relaxed)) {
        if (callback) callback({});
        return;
    }

    auto cb = std::move(callback);

    std::thread worker([url, cb = std::move(cb), this]() mutable {
        if (m_stopping.load(std::memory_order_relaxed)) {
            if (cb) cb({});
            return;
        }

        std::string body;
        if (url == "http://example.dev" || url == "https://example.dev") {
            body = R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Hummingbird CSS Demo</title>
    <style>
      body { margin: 20px; padding: 10px; color: #444; }
      h1, h2, .title { margin: 10px; }
      #lead { font-size: 18px; }
      p { margin: 6px; }
    </style>
  </head>
  <body>
    <h1 class="title">Example Domain</h1>
    <p id="lead"><strong>Typed CSS demo.</strong> This page exercises selector lists, ids, and colors.</p>
    <h2>Elements</h2>
    <p>Inline <em>emphasis</em> and <code>code</code> samples.</p>
    <p class="hidden">You should not see this paragraph.</p>
    <ul>
      <li>List item one</li>
      <li>List item two</li>
    </ul>
    <blockquote>Simple blockquote to show default styling.</blockquote>
    <hr>
    <p>This domain is for use in illustrative examples.</p>
  </body>
</html>
)HTML";
        } else {
            body = "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
        }

        if (cb) cb(std::move(body));
    });

    {
        std::lock_guard<std::mutex> lg(m_threads_mutex);
        if (m_stopping.load(std::memory_order_relaxed)) {
            if (worker.joinable()) worker.join();
            return;
        }
        m_threads.emplace_back(std::move(worker));
    }
}
