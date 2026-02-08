const DARK_SCOPE_SELECTOR = ".hb-dark-scope";

const DARK_MODE_CSS = `
${DARK_SCOPE_SELECTOR},
${DARK_SCOPE_SELECTOR} * {
  background-color: #151821 !important;
  color: #e6e8ef !important;
  border-color: #3f455f !important;
}
${DARK_SCOPE_SELECTOR} a {
  color: #8fc7ff !important;
}
${DARK_SCOPE_SELECTOR} code,
${DARK_SCOPE_SELECTOR} pre {
  background-color: #1f2433 !important;
}
`;

const injectedTabs = new Set();

function injectDarkModeForTab(tab) {
  if (!tab || typeof tab.id !== "number") return;
  if (injectedTabs.has(tab.id)) return;

  const ok = browser.scripting.insertCSS({
    tabId: tab.id,
    cssText: DARK_MODE_CSS
  });
  if (ok) {
    injectedTabs.add(tab.id);
  }
}

browser.tabs.onCreated.addListener(injectDarkModeForTab);
browser.tabs.onActivated.addListener(injectDarkModeForTab);
browser.tabs.onNavigated.addListener(injectDarkModeForTab);

const active = browser.tabs.active();
if (active) {
  injectDarkModeForTab(active);
}

console.log("dark-mode extension loaded (scoped)");
