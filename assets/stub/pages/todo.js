// Interactive todo demo for Milestone 7. Exercises the DOM + event pipeline:
// a checkbox per task, DOM construction, click delegation, classList restyle,
// and hash-routed filters (window.location + hashchange).
(function () {
  var input = document.getElementById("new-todo");
  var list = document.getElementById("list");
  var count = document.getElementById("count");

  function refreshCount() {
    var items = list.getElementsByTagName("li");
    var done = 0;
    for (var i = 0; i < items.length; i++) {
      if (items[i].classList.contains("done")) {
        done++;
      }
    }
    count.textContent = done + " of " + items.length + " done";
  }

  // Hash-routed filter: show all / only active / only completed tasks. Driven by
  // the URL fragment, so it survives across the hashchange event.
  function applyFilter() {
    var hash = location.hash || "#/all";
    var items = list.getElementsByTagName("li");
    for (var i = 0; i < items.length; i++) {
      var done = items[i].classList.contains("done");
      var show = hash === "#/active" ? !done : hash === "#/completed" ? done : true;
      if (show) {
        items[i].classList.remove("hidden");
      } else {
        items[i].classList.add("hidden");
      }
    }
    var links = document.getElementsByClassName("filter");
    for (var j = 0; j < links.length; j++) {
      if (links[j].getAttribute("href") === hash) {
        links[j].classList.add("selected");
      } else {
        links[j].classList.remove("selected");
      }
    }
  }

  function update() {
    refreshCount();
    applyFilter();
  }

  function addTodo(text, done) {
    var li = document.createElement("li");
    li.className = done ? "todo done" : "todo";

    var box = document.createElement("input");
    box.setAttribute("type", "checkbox");
    box.className = "toggle";
    if (done) {
      box.checked = true;
    }

    var label = document.createElement("span");
    label.className = "label";
    label.appendChild(document.createTextNode(text));

    var del = document.createElement("button");
    del.className = "delete";
    del.appendChild(document.createTextNode("x"));

    li.appendChild(box);
    li.appendChild(label);
    li.appendChild(del);
    list.appendChild(li);
  }

  // Enter in the field adds the typed task, then clears the field.
  input.addEventListener("keydown", function (e) {
    if (e.key === "Enter") {
      var text = input.value;
      if (text && text.length > 0) {
        addTodo(text, false);
        input.value = "";
        update();
      }
    }
  });

  // Toggling a task's checkbox fires `change`, which bubbles here (delegation):
  // sync the row's done state to the checkbox.
  list.addEventListener("change", function (e) {
    var li = e.target.closest("li");
    if (!li) {
      return;
    }
    if (e.target.checked) {
      li.classList.add("done");
    } else {
      li.classList.remove("done");
    }
    update();
  });

  // A delegated click listener removes a row when its delete button is clicked.
  list.addEventListener("click", function (e) {
    var del = e.target.closest(".delete");
    if (del) {
      var row = del.closest("li");
      if (row) {
        list.removeChild(row);
        update();
      }
    }
  });

  // Re-filter whenever the URL fragment changes (All / Active / Completed links).
  window.addEventListener("hashchange", applyFilter);

  // Seed a couple of tasks, then apply the current filter + count.
  addTodo("Tick the checkbox to complete a task", false);
  addTodo("This one starts completed", true);
  update();
})();
