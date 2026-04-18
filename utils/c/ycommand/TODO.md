# ycommand — TODO

Ideas para mejorar la experiencia de usuario, ordenadas por alcance.
Todas son complementarias a lo que ya hay (ver README.md y `!help`).

## Pequeñas (polish)

- [ ] **Highlight del match en Ctrl+R**
  En `refreshSearchLine()` de `modules/c/console/src/c_editline.c`, resaltar
  en bold el trozo del match que coincide con el patrón:
  `(reverse-i-search)'yuno': kill-`**`yuno`**` id=foo`.

- [ ] **Ctrl+C cancela el wait asíncrono**
  Si un comando remoto tarda, Ctrl+C debería abortar el wait y devolver
  al prompt en vez de obligar a matar el proceso. Requiere interceptar
  Ctrl+C (`0x03`) en `c_ycommand.c::on_read_cb` solo cuando hay un comando
  asíncrono en vuelo (`priv->pending_commands` / estado `ST_WAIT_*`).

- [ ] **Multi-línea con `\`**
  Si la línea termina en `\`, no ejecutar; seguir leyendo y concatenar.
  Útil para `kw={...}` largos. Tratarlo en `ac_enter` de editline o en
  `ac_command` tras leer el texto.

## Medianas

- [ ] **`!connect <role>[^<name>]` / `!disconnect`**
  Cambiar de yuno en mitad de la sesión sin reiniciar ycommand. Requiere
  destruir el `C_IEVENT_CLI` actual, crear uno nuevo con los atributos
  actualizados (`yuno_role`, `yuno_name`, `yuno_service`), resetear el
  prompt y re-warmar el commands cache.

- [ ] **`!set var=value` + expansión `$var`**
  Variables simples de sesión. `!vars` para listarlas, `!unset var` para
  quitarlas. La expansión se aplica en `exec_one_command()` antes de
  `replace_cli_vars`. Útil con `!source` para templates.

- [ ] **`!echo <texto>`** y **`!sleep <segundos>`**
  Utilities para scripts: `!echo` imprime el argumento (útil como
  progress output), `!sleep` pausa antes de despachar el siguiente
  comando de la cola.

## Grandes

- [ ] **Auto-suggestion fish-shell style**
  Mientras tecleas, mostrar en gris al final la entrada más reciente del
  historial que coincide con el prefijo actual; Right-arrow la acepta.
  Hay que coordinar con el hint de parámetros actual para no pelearse
  por el mismo espacio a la derecha del cursor.

- [ ] **JSON pretty-print coloreado**
  En modo `*cmd` (form), colorear keys en cyan, strings en verde,
  números en amarillo. `print_json` actualmente escupe JSON plano.
  Hay que envolver un formateador propio.

- [ ] **`!diff`**
  Comparar la respuesta del comando actual con la del comando anterior
  del mismo nombre, mostrando solo los cambios. Útil para monitorizar
  `stats`, `list-yunos`, etc. en loop manual.

## Rechazadas / ya cubiertas

- `!source <file>` — hecho.
- Ejecución batch desde stdin / `;` chaining / `-cmd` ignore-fail — hecho.
- Scripting con formato JSON — ya cubierto por `ybatch`, no duplicar.
- Comandos del yuno (`__yuno__`) en el caché de completion — descartado:
  requieren `service=__yuno__` explícito, confundirían la completion.
- Persistencia en disco del commands cache — descartado: cada yuno tiene
  comandos distintos y queremos que ycommand pueda saltar entre agent,
  controlcenter, etc. sin invalidaciones.

---

Consultar `!help` dentro de ycommand para la lista de atajos de teclado y
comandos locales actualmente soportados.
