// External script for the example.dev/m7 demo (story 7.0.1). Fetched via
// <script src="assets/stub/pages/m7.js"> and executed between the two inline
// scripts on the page — in document order.
(function () {
    var step = document.getElementById("step-external");
    if (step) {
        step.textContent = "2. external m7.js: fetched as a separate file and executed [OK]";
    }
    var last = document.getElementById("last-writer");
    if (last) {
        last.textContent = "Last script to run: external m7.js";
    }
})();
