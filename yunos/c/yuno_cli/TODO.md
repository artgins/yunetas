# ycli — TODO

Pendientes conocidos, ordenados por alcance.

## Bugs

- [ ] **Celda negra en col 0 al deseleccionar una fila del popup de TAB**

  Descripción: tras pulsar TAB en `ycli`, el popup muestra la lista de
  candidatos con la fila actual en reverso (fondo negro). Al mover la
  selección con TAB/flecha, la fila que se deselecciona queda con fondo
  blanco excepto la **primera celda (col 0)**, que retiene el fondo negro
  de su estado anterior. Sólo aparece con ncurses (ycli / mqtt_tui); el
  path stdout de ycommand no sufre esto.

  Qué se probó sin éxito:
  - `wclear` + `wbkgd` + `touchwin` antes de cada render.
  - Escritura celda a celda con `mvwaddch(char | color_pair | A_REVERSE?)`
    en lugar de `mvwaddnstr` con `wattron/wattroff` (evita la
    optimización de ncurses que omite celdas cuyo char no cambia).
  - Combinar `wnoutrefresh` + `update_panels` + `doupdate`.
  - `wattrset(A_NORMAL)` explícito antes del write para asegurar que no
    queda ningún bit colgado.

  Fichero afectado: `modules/c/console/src/c_editline.c`, función
  `completion_render_popup()`.

  Hipótesis restantes a investigar:
  1. Interacción entre el `wbkgdset` del popup y el
     `wbkgdset`/`wbkgd` de la ventana bajo él (workarea) al componer
     paneles — alguna celda concreta queda con atributos heredados del
     panel inferior.
  2. Terminfo concreto del terminal del usuario puede estar reportando
     un capability que hace que ncurses salte esa celda.
  3. Último recurso: **destruir y recrear** la ventana del popup en
     cada render (`del_panel` + `delwin` + `newwin` + `new_panel` +
     `top_panel`). Es caro (~1 ms extra por tecla) pero debería
     eliminar cualquier estado residual. Probablemente lo correcto si
     ninguna de las anteriores da con la raíz.

  Workaround actual: el bug es puramente cosmético; la selección
  lógica es correcta y el `Enter` commit a buffer funciona igual.

## Nice-to-have

- [ ] **Hint "(fetching commands…)" mientras el cache remoto vuelve**

  La primera pulsación de `TAB` tras abrir una ventana recién
  conectada puede caer en "sin candidatos" silencioso si la respuesta
  de `list-gobj-commands` no ha llegado aún. Con un callback de hints
  que consulte `priv->pending_cache_fetches[conn_key]` se puede
  devolver "(fetching commands…)" en gris hasta que entre la
  respuesta.

- [ ] **Completion de valores no-boolean**

  Hoy `cli_completion_cb` sólo ofrece `true`/`false` tras `param=`
  para parámetros boolean. Se podría extender a enums declarados en
  el schema (`choices`), rutas de fichero (`type="path"`), etc.
  Misma estructura que ycommand.

- [ ] **`!prefix` history expansion**

  bash re-ejecuta el último comando que empieza por `prefix` (además
  de `!N` y `!!`). Se añadiría al bloque de expansión de historia en
  `ac_command`.
