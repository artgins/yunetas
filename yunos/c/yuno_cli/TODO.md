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
  - Render en dos pasadas con `mvwchgat` (Pass 1 escribe texto con
    `A_NORMAL`, Pass 2 aplica `A_NORMAL|pair` a todas las filas y
    `A_REVERSE|pair` sobre la seleccionada) — vía que usan las
    librerías `menu`/`form` de ncurses. No resolvió el problema.
  - **`del_panel` + `delwin` + `newwin` + `new_panel` + `top_panel` en
    cada render** (ventana nueva cada frame, cero estado residual en
    celdas). Tampoco resolvió el problema — lo que descarta que sea
    una optimización de diff sobre el estado de celda del popup.

  Fichero afectado: `modules/c/console/src/c_editline.c`, función
  `completion_render_popup()`.

  Hipótesis restantes (tras descartar estado de celda):
  1. **Panel composición bajo el popup**: la celda col 0 es renderizada
     por un panel inferior (stdscr o el display window) cuando nuestro
     popup se marca como top. Investigar si `update_panels` está
     recomponiendo mal el stack al instalar el nuevo panel. Probar
     mover el `top_panel` antes de dibujar y forzar `update_panels` +
     `doupdate` entre el del_panel y el new_panel.
  2. **Terminfo / emulador específico**: algún cap del terminfo del
     usuario provoca que la secuencia de reposicionamiento a col 0
     (típicamente `\r` = carriage return) no resetee los atributos
     antes del siguiente char. Probar con otro `TERM` (xterm-256color,
     screen-256color, linux).
  3. **Bug en la rama A_REVERSE del propio ncurses sobre chars tipo
     espacio**: escribir el primer char como no-espacio (p.ej. `'|'`)
     y ver si el artifact se mueve / desaparece. Eso confirmaría que
     el problema se dispara solo con el char-bg.

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
