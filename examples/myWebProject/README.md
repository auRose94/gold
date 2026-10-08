# gold/myWebProject
### A full-stack web example

[![License](https://img.shields.io/badge/license-Apache%202-blue)](LICENSE)

What is this?

This is an example full-stack web service built with the
[gold framework](https://github.com/auRose94/gold) web module. Routes are
C++ handlers registered on an Express-like `server`, pages are HTML built
with the gold HTML element builders, and documents persist in the
file-backed `dataStore` (`./data`, created on first run; uploads land in
`./upload`). It uses [JQuery](https://jquery.com/),
[JQueryUI](https://jqueryui.com/), [Bootstrap](https://getbootstrap.com/),
[Bootstrap-Table](https://bootstrap-table.com/),
[Luxon](https://moment.github.io/luxon/), and
[FontAwesome](https://fontawesome.com/) as static frontend assets served
from `js/` and `css/`.

## Build and run

```sh
cmake -S . -B build -DGOLD_BUILD_EXAMPLES=ON
cmake --build build --target MyWebProject
./build/examples/myWebProject/MyWebProject
```

The server listens on `127.0.0.1:8080` by default; open
`http://127.0.0.1:8080` in a browser.

All code not in 3rdParty or explicitly stated otherwise (like files in js
or css) are Apache version 2.