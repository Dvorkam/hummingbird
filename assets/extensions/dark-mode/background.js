const DARK_MODE_CSS = `
html,
body,
body * {
  background-color: #151821 !important;
  color: #e6e8ef !important;
  border-color: #3f455f !important;
}
a {
  color: #8fc7ff !important;
}
code,
pre {
  background-color: #1f2433 !important;
}
img,
svg,
video,
canvas {
  background-color: transparent !important;
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

console.log("dark-mode extension loaded (global)");
