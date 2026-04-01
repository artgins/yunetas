/*
 *
 *  This file has sample functions copied from bulma documentation source code.
 *
 */

function themes() {
    // MODALS
    // Functions to open and close a modal
    function openModal($el) {
        $el.classList.add("is-active");
    }

    function closeModal($el) {
        $el.classList.remove("is-active");
    }

    function closeAllModals() {
        (document.querySelectorAll(".modal") || []).forEach(($modal) => {
            closeModal($modal);
        });
    }

    (document.querySelectorAll(".js-modal-trigger") || []).forEach(($trigger) => {
        const modal = $trigger.dataset.target;
        const $target = document.getElementById(modal);

        $trigger.addEventListener("click", () => {
            openModal($target);
        });
    });

    (
        document.querySelectorAll(
            ".modal-background, .modal-close, .modal-card-head .delete, .modal-card-foot .button"
        ) || []
    ).forEach(($close) => {
        const $target = $close.closest(".modal");

        $close.addEventListener("click", () => {
            closeModal($target);
        });
    });

    document.addEventListener("keydown", (event) => {
        if (event.key === "Escape") {
            closeAllModals();
        }
    });

    // DROPDOWNS
    const $clickableDropdowns = document.querySelectorAll(
        ".dropdown:not(.is-hoverable)"
    );

    if ($clickableDropdowns.length > 0) {
        $clickableDropdowns.forEach(($dropdown) => {
            $dropdown.querySelector("button").addEventListener("click", (event) => {
                event.stopPropagation();
                $dropdown.classList.toggle("is-active");
            });
        });

        document.addEventListener("click", () => {
            closeDropdowns();
        });
    }

    function closeDropdowns() {
        $clickableDropdowns.forEach(($el) => {
            $el.classList.remove("is-active");
        });
    }

    // THEMES
    const STORAGE_KEY = "bulma-theme";
    const SYSTEM_THEME = "system";
    const DEFAULT_THEME = "light";

    const state = {
        chosenTheme: SYSTEM_THEME, // light|dark|system
        appliedTheme: DEFAULT_THEME, // light|dark
        OSTheme: null, // light|dark|null
    };

    const $themeCycle = document.getElementById("js-cycle");
    const $themeSwitchers = document.querySelectorAll(".js-themes button");
    const $darkmodes = document.querySelectorAll(".js-darkmode");

    const updateThemeUI = () => {
        if (state.appliedTheme === "light") {
            $themeCycle.className = "bd-cycle js-burger is-sun";
        } else {
            $themeCycle.className = "bd-cycle js-burger is-moon";
        }

        $themeSwitchers.forEach((el) => {
            const swatchTheme = el.dataset.scheme;

            if (state.chosenTheme === swatchTheme) {
                el.classList.add("is-active");
            } else {
                el.classList.remove("is-active");
            }
        });
    };

    const setTheme = (theme, save = true) => {
        state.chosenTheme = theme;
        state.appliedTheme = theme;

        if (theme === SYSTEM_THEME) {
            state.appliedTheme = state.OSTheme;
            document.documentElement.removeAttribute("data-theme");
            window.localStorage.removeItem(STORAGE_KEY);
        } else {
            document.documentElement.setAttribute("data-theme", theme);

            if (save) {
                window.localStorage.setItem(STORAGE_KEY, theme);
            }
        }

        updateThemeUI();
    };

    const toggleTheme = () => {
        if (state.appliedTheme === "light") {
            setTheme("dark");
        } else {
            setTheme("light");
        }
    };

    const detectOSTheme = () => {
        if (!window.matchMedia) {
            // matchMedia method not supported
            return DEFAULT_THEME;
        }

        if (window.matchMedia("(prefers-color-scheme: dark)").matches) {
            // OS theme setting detected as dark
            return "dark";
        } else if (window.matchMedia("(prefers-color-scheme: light)").matches) {
            return "light";
        }

        return DEFAULT_THEME;
    };

    // On load, check if any preference was saved
    const localTheme = window.localStorage.getItem(STORAGE_KEY);
    state.OSTheme = detectOSTheme();

    if (localTheme) {
        setTheme(localTheme, false);
    } else {
        setTheme(SYSTEM_THEME);
    }

    // Event listeners
    $themeSwitchers.forEach((el) => {
        el.addEventListener("click", () => {
            const theme = el.dataset.scheme;
            setTheme(theme);
        });
    });

    $darkmodes.forEach((el) => {
        el.addEventListener("click", (e) => {
            e.preventDefault();
            toggleTheme();
        });
    });

    window
        .matchMedia("(prefers-color-scheme: dark)")
        .addEventListener("change", (event) => {
            const theme = event.matches ? "dark" : "light";
            state.OSTheme = theme;
            setTheme(theme);
        });

    // BURGERS
    const $burgers = document.querySelectorAll(".js-burger");

    $burgers.forEach((el) => {
        el.addEventListener("click", (e) => {
            e.preventDefault();
            const targetID = el.dataset.target;
            const $target = document.getElementById(targetID);
            el.classList.toggle("is-active");
            $target.classList.toggle("is-active");
            e.stopPropagation();
        });
    });

    const onClickOutside = (menuSelector) => {
        document.addEventListener("click", (e) => {
            const menus = document.querySelectorAll(menuSelector);

            menus.forEach((el) => {
                if (!el.contains(e.target) && el.classList.contains("is-active")) {
                    el.classList.remove("is-active");
                }
            });
        });
    };

    onClickOutside(".js-menu");
}

export {
    themes
};
