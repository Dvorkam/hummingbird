// Interactive todo demo for Milestone 7. Exercises the DOM + event pipeline:
// keydown on an input, DOM construction, click delegation, and classList restyle.
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

  function addTodo(text) {
    var li = document.createElement("li");
    li.className = "todo";

    var label = document.createElement("span");
    label.className = "label";
    label.appendChild(document.createTextNode(text));

    var del = document.createElement("button");
    del.className = "delete";
    del.appendChild(document.createTextNode("x"));

    li.appendChild(label);
    li.appendChild(del);
    list.appendChild(li);
  }

  // Enter in the field adds the typed task, then clears the field.
  input.addEventListener("keydown", function (e) {
    if (e.key === "Enter") {
      var text = input.value;
      if (text && text.length > 0) {
        addTodo(text);
        input.value = "";
        refreshCount();
      }
    }
  });

  // One delegated click listener on the list handles every item: the delete
  // button removes its row; clicking anywhere else on a row toggles it done.
  list.addEventListener("click", function (e) {
    var del = e.target.closest(".delete");
    if (del) {
      var row = del.closest("li");
      if (row) {
        list.removeChild(row);
        refreshCount();
      }
      return;
    }
    var li = e.target.closest("li");
    if (li) {
      li.classList.toggle("done");
      refreshCount();
    }
  });

  // Seed a couple of tasks so the list is not empty on first load.
  addTodo("Click a task to cross it off");
  addTodo("Type below and press Enter to add");
  refreshCount();
})();
