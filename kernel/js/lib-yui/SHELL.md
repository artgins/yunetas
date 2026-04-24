# `C_YUI_SHELL` + `C_YUI_NAV` — Shell declarativo

Sistema de presentación y navegación de nivel aplicación para GUIs Yuneta.
Sustituye al tándem `C_YUI_MAIN` + `C_YUI_ROUTING` (ambos siguen existiendo
y pueden convivir) por un par de GClasses guiadas por un JSON al estilo
de la configuración de Yuneta.

- Construido con **Vite** (igual que el resto de `lib-yui`).
- Apoyado en **Bulma** (`.menu`, `.tabs`, `.level`, `.navbar`, helpers
  `is-hidden-*`). No introduce framework JS; todo es DOM + GObj.
- Diseñado para integrarse en `libyui` al finalizar la validación.

---

## 1. Objetivos

1. **Declarativo**: el reparto de la pantalla y el árbol de menús se
   describen en un JSON, no en JavaScript imperativo.
2. **Responsive por zonas**: poder decir *"el menú principal va en
   `left` en desktop y en `bottom` en móvil"* sin duplicar la definición
   del menú ni romper el estado interno de los paneles.
3. **Navegación de 2 niveles**: opciones principales + subopciones,
   mapeadas a rutas hash (`#/primary/secondary`).
4. **Render pluggable por zona**: la misma opción de menú debe poder
   pintarse distinto según dónde caiga (icono+label vertical en `left`;
   icono sobre label en `bottom`; tabs horizontales en `top-sub`; lista
   vertical en `right`; etc.).
5. **Cada vista es un gobj con su `$container`**: el shell no sabe qué
   hay dentro — sólo lo monta, lo muestra y lo oculta. La navegación es
   *"mostrar un gobj en su zona y ocultar el que había"*.
6. **Ciclo de vida configurable por opción**: `eager` / `keep_alive` /
   `lazy_destroy`, para equilibrar coste de reconstrucción vs. RAM.
7. **Sin regresiones sobre `lib-yui`**: los componentes existentes
   (`C_YUI_MAIN`, `C_YUI_ROUTING`, `C_YUI_TABS`, etc.) no se tocan.

---

## 2. Modelo

Tres conceptos ortogonales:

- **Layer** — plano apilado en Z (ocupa toda la pantalla). Planos
  definidos:
  - `base` — layout principal (zonas).
  - `overlay` — drawer off-canvas, dropdowns grandes.
  - `popup` — tooltips / menús contextuales.
  - `modal` — diálogos bloqueantes.
  - `notification` — toasts.
  - `loading` — spinner global.
- **Zone** — región dentro del `base`. Hay 7 zonas fijas distribuidas
  en un CSS grid:
  ```
  +------------------- top --------------------+
  +----------------- top-sub ------------------+
  | left | ............ center .......| right |
  +---------------- bottom-sub ----------------+
  +------------------ bottom ------------------+
  ```
  Una zona puede *hospedar* (`host`) un menú, una toolbar o una *stage*.
  Se oculta por breakpoint con el operador `show_on`.
- **Stage** — zona marcada como contenedora de gobjs ruteados. La más
  común es `main` montada en `center`. Puede haber varias (ej. un
  `right` configurado como stage para un panel de detalle independiente).

### Qué va en cada zona

| Zona         | Uso típico                                            |
|--------------|-------------------------------------------------------|
| `top`        | toolbar fija (logo, tema, idioma, usuario)            |
| `top-sub`    | submenú en modo `tabs` en móvil                       |
| `left`       | menú principal `vertical` en desktop                  |
| `center`     | **stage** principal: gobj activo                      |
| `right`      | submenú `vertical` en desktop, o stage secundario     |
| `bottom-sub` | toolbar secundaria o tabs en móvil                    |
| `bottom`     | menú principal `icon-bar` en móvil                    |

---

## 3. JSON de configuración

Esquema mínimo:

```json
{
  "shell": {
    "zones":  { ... cómo se usan las zonas ...  },
    "stages": { ... qué stages existen ...      }
  },
  "menu": {
    "primary": { "render": { ... }, "items": [ ... ] }
  },
  "toolbar": { "zone": "top", "items": [ ... ] }
}
```

### 3.1 `shell.zones`

Cada zona puede declarar:

- `host`: qué contenido recibe. Valores:
  - `"menu.<id>"` — renderiza ese menú en esta zona.
  - `"stage.<name>"` — la zona es stage de gobjs ruteados.
  - `"toolbar"` — hospedará la toolbar definida en `toolbar`.
- `show_on`: expresión de breakpoints Bulma. Formas aceptadas:
  - `"mobile"`, `"tablet"`, `"desktop"`, `"widescreen"`, `"fullhd"`
  - `">=desktop"`, `"<tablet"`, `"<=tablet"`, `">mobile"`, `">fullhd"` (→ ∅)
  - Combinable con `|`: `"mobile|tablet"`, `">=desktop|mobile"`

El shell traduce la expresión a un conjunto de **clases CSS propias** que
ocultan la zona por breakpoint. Bulma sólo define helpers "hasta"
(`is-hidden-tablet`, `is-hidden-desktop`) — para poder decir *"sólo
oculto en tablet"* `lib-yui` añade estas clases en `c_yui_shell.css`:

| Clase                          | Oculta en                   |
|--------------------------------|-----------------------------|
| `.yui-hidden-mobile`           | `<769 px`                   |
| `.yui-hidden-tablet-only`      | `769–1023 px`               |
| `.yui-hidden-desktop-only`     | `1024–1215 px`              |
| `.yui-hidden-widescreen-only`  | `1216–1407 px`              |
| `.yui-hidden-fullhd`           | `≥1408 px`                  |

No se esperan clases fuera de esta tabla en `show_on`. El parser es puro
y está cubierto por `tests/shell_show_on.test.mjs` (`npm test` en
`lib-yui/`).

Ejemplo:

```json
"zones": {
  "top":     { "host": "toolbar",        "show_on": ">=tablet" },
  "left":    { "host": "menu.primary",   "show_on": ">=desktop" },
  "bottom":  { "host": "menu.primary",   "show_on": "<desktop"  },
  "top-sub": { "host": "menu.secondary", "show_on": "<desktop"  },
  "right":   { "host": "menu.secondary", "show_on": ">=desktop" },
  "center":  { "host": "stage.main" }
}
```

> La misma opción del menú principal se instancia dos veces (en `left`
> y en `bottom`) y se muestra/oculta por CSS. Se evita mover nodos al
> cruzar breakpoints, que rompería el estado interno.

### 3.2 `shell.stages`

```json
"stages": {
  "main": { "zone": "center", "default_route": "/dash/ov" }
}
```

- `zone` — zona hospedadora (debe existir).
- `default_route` — ruta inicial si el hash está vacío.

Una stage es inferida automáticamente si una zona declara
`"host": "stage.<name>"`, por lo que `shell.stages` puede omitirse
cuando sólo hay una stage principal con nombre `main` en `center`.

### 3.3 `menu.<id>`

```json
"primary": {
  "render": {
    "left":   { "layout": "vertical", "icon_pos": "left", "show_label": true },
    "bottom": { "layout": "icon-bar", "icon_pos": "top",  "show_label": true }
  },
  "items": [ ... ]
}
```

- `render[zone]` — cómo se pinta ese menú cuando aparece en `zone`.
  - `layout`: `vertical` | `icon-bar` | `tabs` | `drawer` | `submenu` |
    `accordion`.
  - `icon_pos`: `left` | `right` | `top` | `bottom`.
  - `show_label`: booleano.
  - Shortcut: en `submenu.render` se acepta la cadena directa del
    layout (`"tabs"`) en vez de un objeto.
- `items[]` — opciones.

### 3.4 `toolbar`

```json
"toolbar": {
  "zone": "top",
  "aria_label": "App toolbar",
  "items": [
    { "id": "burger", "icon": "icon-menu",
      "aria_label": "Open menu",
      "action": { "type": "drawer", "op": "toggle", "menu_id": "quick" } },
    { "id": "home",   "icon": "icon-home",  "name": "Home",
      "action": { "type": "navigate", "route": "/dash/ov" } },
    { "id": "user",   "icon": "icon-user",  "align": "end",
      "action": { "type": "event", "event": "EV_OPEN_USER_MENU" } }
  ]
}
```

- `zone` — zona hospedadora. Por defecto, la primera zona que declare
  `"host": "toolbar"` en `shell.zones`.
- `items[].action.type`:
  - `"navigate"` → `{ route }` — delega en el shell (respeta `use_hash`).
  - `"drawer"`   → `{ op: "toggle" | "open" | "close", menu_id? }` —
    abre/cierra el nav con `layout:"drawer"` que coincide con `menu_id`.
  - `"event"`    → `{ event, kw? }` — `gobj_publish_event` desde el shell.
- `items[].align`: `"start"` (por defecto) o `"end"` (alinea a la derecha).
- `aria_label` por item se usa como `aria-label` del `<button>`.

### 3.5 `items[]` — estructura de una opción

```json
{
  "id": "ov",
  "name": "Overview",
  "icon": "icon-eye",
  "route": "/dash/ov",
  "disabled": false,
  "badge": "3",
  "target": {
    "stage": "main",
    "gclass": "C_DASHBOARD_OVERVIEW",
    "kw": { "refresh_ms": 5000 },
    "lifecycle": "keep_alive"
  },
  "submenu": {
    "render": { "top-sub": "tabs", "right": "vertical" },
    "default": "/dash/ov/overview",
    "items": [ ... ]
  }
}
```

- `route` — ruta hash a asociar.
- `target` — qué mostrar al activar la ruta:
  - `gclass` — GClass a instanciar.
  - `kw` — atributos iniciales.
  - `name` — nombre del gobj (opcional, se deriva de la ruta).
  - `gobj` — alternativo: usar un gobj preexistente por nombre.
  - `stage` — dónde montarlo (por defecto `main`).
  - `lifecycle`: `eager` | `keep_alive` | `lazy_destroy`.
- `submenu` — si se declara, el item pasa a ser *contenedor*: su ruta
  sola redirige al primer sub-item (o a `submenu.default`). Los
  sub-items declaran su propio `target`.

---

## 4. Ciclo de vida

Al activar una ruta, el shell:

1. Busca la entrada en su índice `route → { item, parent_item, target, stage }`.
2. Oculta el `$container` del gobj previo en esa stage (`is-hidden`).
3. Si el previo tenía `lifecycle: "lazy_destroy"`, lo destruye.
4. Si el nuevo no existe aún, lo crea con `target.gclass` + `target.kw`,
   lo añade al DOM de la stage y lo arranca.
5. Quita `is-hidden` del nuevo, publica `EV_ROUTE_CHANGED` y sincroniza
   el hash.

Modos de `lifecycle`:

| Modo            | Creación    | Al salir        | Para qué sirve                              |
|-----------------|-------------|-----------------|---------------------------------------------|
| `keep_alive`    | 1ª visita   | oculto (conserva estado) | vistas pesadas o con formulario a medio     |
| `lazy_destroy`  | 1ª visita   | destruido                | vistas ocasionales, evita acumular RAM      |
| `eager`         | al arrancar | oculto                   | vistas que deben "estar ahí" desde el inicio |

Default recomendado: `keep_alive`.

---

## 5. Navegación

- **Hash routing de 2 niveles**: el shell instala un listener de
  `hashchange`. Cualquier click en `C_YUI_NAV` cambia `window.location.hash`
  y el shell responde.
- **Programática**: `yui_shell_navigate(shell_gobj, "/dash/ov")`.
- **Evento saliente**: `EV_ROUTE_CHANGED` (con `route`, `item`,
  `parent_item`, `stage`). Los navs lo consumen para marcar el activo;
  las vistas pueden suscribirse para reaccionar.

---

## 6. GClasses

### `C_YUI_SHELL`

| Atributo         | Tipo        | Descripción                                              |
|------------------|-------------|----------------------------------------------------------|
| `config`         | JSON        | El JSON descrito arriba                                  |
| `default_route`  | string      | Fallback si hash vacío y no hay `stages.*.default_route` |
| `current_route`  | string      | Lectura: ruta activa                                     |
| `use_hash`       | bool        | Si `true`, sincroniza `window.location.hash`             |
| `mount_element`  | HTMLElement | Dónde montar el shell (default `document.body`)          |
| `translate`      | function    | `(key) => string` — i18n opcional; aplicado a `item.name`, labels de toolbar y `aria-label` |
| `$container`     | HTMLElement | Raíz del shell                                           |

Eventos publicados:
- `EV_ROUTE_CHANGED` — `{ route, item, parent_item, stage }`.

Helpers públicos (import desde `@yuneta/lib-yui`):
- `yui_shell_navigate(shell, route)` — navegación programática.
- `yui_shell_open_drawer(shell, menu_id?)`,
  `yui_shell_close_drawer(shell, menu_id?)`,
  `yui_shell_toggle_drawer(shell, menu_id?)` — actúan sobre el
  `C_YUI_NAV` con `layout:"drawer"` cuyo `menu_id` coincida (todos si
  `menu_id` se omite). Con `Escape` el shell los cierra por defecto.

### `C_YUI_NAV`

Se instancia desde el shell (uno por par *menú, zona*). Usuario final
no suele crearlo directamente. El nav **no** navega: publica la
intención y el shell enruta.

Eventos publicados:
- `EV_NAV_CLICKED` — `{ route, item_id, zone, level }`. El shell está
  suscrito y decide si cambiar el hash o llamar a `navigate_to` directo.

Atributos de interés: `menu_items`, `zone`, `layout`, `icon_pos`,
`show_label`, `level` (`primary` | `secondary`), `shell`, `translate`.

---

## 7. Integración en una app

```js
import {
    register_c_yui_shell,
    register_c_yui_nav,
} from "@yuneta/lib-yui";
import "@yuneta/lib-yui/src/c_yui_shell.css";
import "bulma/css/bulma.css";
import app_config from "./app_config.json";

register_c_yui_shell();
register_c_yui_nav();
/*  también: register_c_<tu_vista>() para cada gclass referenciada en target.gclass */

gobj_create_default_service(
    "shell",
    "C_YUI_SHELL",
    { config: app_config, use_hash: true },
    yuno
);
```

Cada GClass vista debe exponer un atributo `$container` con un
`HTMLElement` raíz; el shell se encarga de añadirlo a la stage y
gestionar su visibilidad.

---

## 8. Solución frente al planteamiento original

| Requisito                                           | Cómo se cubre                                              |
|-----------------------------------------------------|------------------------------------------------------------|
| Declarativo, estilo JSON Yuneta                     | Atributo `config` con JSON de `shell`/`menu`/`toolbar`     |
| Layers + zonas de trabajo                           | 6 layers fijas + 7 zonas en grid                           |
| Menú principal con 2 niveles                        | `menu.primary.items[].submenu.items[]`                     |
| Principal en `left` en desktop y `bottom` en móvil  | `"host": "menu.primary"` en ambas, con `show_on` contrario |
| Icono+nombre; distinto según zona                   | `render[zone]` con `layout` + `icon_pos` + `show_label`    |
| Submenú como tabs **o** como submenú               | `submenu.render[zone]` a `"tabs"` / `"vertical"` / etc.    |
| Toolbar fija en top o bottom                        | `shell.zones.top.host = "toolbar"` (o `bottom`)            |
| Construido con helpers Bulma                        | `.menu`, `.tabs`, `.level`, `is-hidden-*`, CSS nuestro sólo para `icon-bar`, `drawer`, `accordion` y los hiders por breakpoint |
| Compatible con Vite                                 | Mismo flujo que el resto de `lib-yui`                      |
| Integrable luego en libyui                          | Exportado desde `index.js`                                 |

---

## 9. Test app

Ver `test-app/README.md`. Resumen:

```
cd kernel/js/lib-yui/test-app
npm install
npm run dev
```

Ejercita todos los caminos: breakpoints, navegación por click y por
hash, y el contraste `keep_alive` vs `lazy_destroy` (visible en el
contador `instance #` que pinta `C_TEST_VIEW`).

---

## 10. Estado y plan de retirada

Este corte cierra **todas** las promesas del diseño original. Pendientes
son los dos consumidores históricos (`C_YUI_MAIN` y `C_YUI_ROUTING`) que
todavía se usan desde otras partes de `lib-yui`:

### Implementado ✓

- Zonas + `show_on` con los operadores `>=`, `<=`, `<`, `>`, enumeración y
  `|`. Parser puro testeado (`npm test`).
- Inferencia automática de la stage `main` desde `"host": "stage.<name>"`.
- Los 6 layouts de menú (`vertical`, `icon-bar`, `tabs`, `drawer`,
  `submenu`, `accordion`), con auto-expansión de la rama activa en
  accordion al cambiar ruta.
- Drawer off-canvas: se monta en el layer `overlay` (no dentro del grid
  de zonas), cierre por click en backdrop y por `Escape`, API pública
  `yui_shell_{open,close,toggle}_drawer`.
- `lifecycle: eager | keep_alive | lazy_destroy`, con el primero
  preinstanciando las vistas al arrancar.
- **Toolbar declarativa** (`toolbar.items[]` con acciones `navigate`,
  `drawer`, `event`).
- Un único router: el nav publica `EV_NAV_CLICKED`, el shell enruta.
- Hook de i18n `translate: (key) => string` aplicado a labels y
  `aria-label`.
- Accesibilidad: `role="navigation"` en navs, `role="dialog"` +
  `aria-modal` en drawers, `aria-expanded` / `aria-controls` en
  accordion, `aria-disabled` + `tabindex="-1"` en ítems deshabilitados,
  `:focus-visible` en todo control interactivo.
- Fail ruidoso cuando no hay ruta: `log_error` + placeholder visible en
  la stage, en lugar de pantalla en blanco.
- Contrato duro: si una vista no expone `$container`, el shell registra
  error y destruye el gobj a medio construir.

### Plan de retirada de `C_YUI_MAIN` / `C_YUI_ROUTING`

Los siguientes puntos son los bloqueantes antes de borrar cada uno:

1. **Modales y notificaciones** — `C_YUI_MAIN` expone `display_*`,
   gestor de diálogos volátiles y toasts. El shell ya crea los layers
   `modal`, `notification` y `loading` en `build_ui`, pero todavía no
   hay API declarativa para ellos. Primer paso: mover `display_error`,
   `display_info`, `display_confirm` a helpers del shell que pinten en
   `layers.notification` / `layers.modal`.
2. **Grid de widgets / `C_YUI_MAIN.layout`** — revisar qué consumidores
   de `lib-yui` lo usan (`grep register_c_yui_main`). Cada uno pasa al
   shell declarando sus zonas.
3. **`C_YUI_ROUTING`** — el hash routing ya está en el shell, pero
   `C_YUI_TABS` y algún otro componente todavía lo usan. Se retira
   cuando cada uno se reescribe para suscribirse a `EV_ROUTE_CHANGED`
   del shell en lugar de a `EV_ROUTING_CHANGED` de `C_YUI_ROUTING`.

Checklist antes de borrar cualquiera de los dos gclasses:

- [ ] `grep -r register_c_yui_main  kernel/ utils/ yunos/` vacío.
- [ ] `grep -r register_c_yui_routing kernel/ utils/ yunos/` vacío.
- [ ] Helpers equivalentes a `display_*` disponibles sobre el shell.
- [ ] Consumidores de `EV_ROUTING_CHANGED` migrados a
      `EV_ROUTE_CHANGED` del shell.
