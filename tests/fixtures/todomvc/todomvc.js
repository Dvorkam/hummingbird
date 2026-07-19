// A pinned, self-contained vanilla-JS TodoMVC controller used as an end-to-end
// regression fixture (story 7.5.1). It follows the standard TodoMVC DOM contract
// (.todoapp / .new-todo / .todo-list / .toggle / .destroy / .filters / hash
// routes / .todo-count) but keeps state in memory instead of localStorage, which
// the engine does not yet provide. Swapping in the upstream app awaits
// localStorage + fail-soft missing-API handling (T-JS-GLOBAL-ISOLATION-1 / 7.5.2).
(function () {
  var todos = [];
  var nextId = 1;
  var route = 'all'; // all | active | completed

  var listEl = document.querySelector('.todo-list');
  var newInput = document.querySelector('.new-todo');
  var countEl = document.querySelector('.todo-count');
  var mainEl = document.querySelector('.main');
  var footerEl = document.querySelector('.footer');

  function activeCount() {
    var n = 0;
    for (var i = 0; i < todos.length; i++) {
      if (!todos[i].completed) n++;
    }
    return n;
  }

  function isVisible(t) {
    if (route === 'active') return !t.completed;
    if (route === 'completed') return t.completed;
    return true;
  }

  function render() {
    listEl.innerHTML = '';
    for (var i = 0; i < todos.length; i++) {
      var t = todos[i];
      if (!isVisible(t)) continue;

      var li = document.createElement('li');
      li.setAttribute('data-id', '' + t.id);
      if (t.completed) li.className = 'completed';

      var view = document.createElement('div');
      view.className = 'view';

      var cb = document.createElement('input');
      cb.setAttribute('type', 'checkbox');
      cb.className = 'toggle';
      if (t.completed) cb.checked = true;

      var label = document.createElement('label');
      label.textContent = t.title;

      var del = document.createElement('button');
      del.className = 'destroy';
      del.textContent = 'x';

      view.appendChild(cb);
      view.appendChild(label);
      view.appendChild(del);
      li.appendChild(view);
      listEl.appendChild(li);
    }

    var left = activeCount();
    countEl.textContent = left + (left === 1 ? ' item left' : ' items left');

    var hasTodos = todos.length > 0;
    mainEl.className = hasTodos ? 'main' : 'main hidden';
    footerEl.className = hasTodos ? 'footer' : 'footer hidden';

    var links = document.querySelectorAll('.filters a');
    for (var j = 0; j < links.length; j++) {
      var href = links[j].getAttribute('href');
      var r = href === '#/active' ? 'active' : (href === '#/completed' ? 'completed' : 'all');
      links[j].className = (r === route) ? 'selected' : '';
    }
  }

  function addTodo(title) {
    title = title.trim();
    if (!title) return;
    todos.push({ id: nextId++, title: title, completed: false });
    render();
  }

  function todoById(id) {
    for (var i = 0; i < todos.length; i++) {
      if (todos[i].id === id) return todos[i];
    }
    return null;
  }

  newInput.addEventListener('keydown', function (e) {
    if (e.key === 'Enter') {
      addTodo(newInput.value);
      newInput.value = '';
    }
  });

  // Delegated: a checkbox toggle fires `change`; a destroy button fires `click`.
  listEl.addEventListener('change', function (e) {
    var cb = e.target;
    if (!cb.classList || !cb.classList.contains('toggle')) return;
    var li = cb.closest('li');
    var t = todoById(parseInt(li.getAttribute('data-id'), 10));
    if (t) t.completed = cb.checked;
    render();
  });
  listEl.addEventListener('click', function (e) {
    var btn = e.target;
    if (!btn.classList || !btn.classList.contains('destroy')) return;
    var li = btn.closest('li');
    var id = parseInt(li.getAttribute('data-id'), 10);
    todos = todos.filter(function (t) { return t.id !== id; });
    render();
  });

  document.querySelector('.toggle-all').addEventListener('change', function (e) {
    var c = e.target.checked;
    for (var i = 0; i < todos.length; i++) todos[i].completed = c;
    render();
  });

  document.querySelector('.clear-completed').addEventListener('click', function () {
    todos = todos.filter(function (t) { return !t.completed; });
    render();
  });

  function applyRoute() {
    var h = location.hash;
    route = h === '#/active' ? 'active' : (h === '#/completed' ? 'completed' : 'all');
    render();
  }
  window.addEventListener('hashchange', applyRoute);

  applyRoute(); // initial render
})();
