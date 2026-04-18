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

  Intentos anteriores que NO lo resolvieron:
  - `wclear` + `wbkgd` + `touchwin` antes de cada render.
  - Escritura celda a celda con `mvwaddch(char | color_pair | A_REVERSE?)`.
  - Combinar `wnoutrefresh` + `update_panels` + `doupdate`.
  - `wattrset(A_NORMAL)` explícito.
  - Render en dos pasadas con `mvwchgat` (pass 1 contenido, pass 2
    atributos). Normalmente `mvwchgat` rompe el diff-de-chars, pero la
    celda col 0 seguía retornando al estado anterior tras deseleccionar.

  **Fix aplicado (actualizado)**: **destruir y recrear la ventana del
  popup en cada render**. Al entrar `completion_render_popup()`:
  `del_panel` + `delwin` del popup activo, luego `newwin` + `new_panel`
  + `top_panel` con la geometría (py/px/h/w) cacheada en el
  `PRIVATE_DATA` del editline. No queda estado residual entre frames,
  así que ninguna optimización de diff puede salvar una celda stale.
  Coste aproximado: ~1 newwin/panel por tecla mientras el popup cicla;
  imperceptible en uso interactivo.

  Fichero afectado: `modules/c/console/src/c_editline.c`, función
  `completion_render_popup()` + campos `completion_{py,px,h,w}` en
  `PRIVATE_DATA`.

  El render de dos pasadas con `mvwchgat` se mantiene por si sirve de
  belt-and-suspenders; con la ventana fresca en cada frame ya no es
  estrictamente necesario.

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
