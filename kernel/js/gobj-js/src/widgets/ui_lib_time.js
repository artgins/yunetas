/***********************************************************************
 *          ui_lib_time.js
 *
 *          Time Helpers
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /**************************************************************
     *  Uso:
     *
        popup_calendar_from_to(
            self,           // GObj to send event
            "mical",        // Nombre calendario
            from_t,         // value for from, UTC time
            to_t,           // value for to, UTC time
            near_node,      // Show popup near HTML node!
            {},             // Overwride properties of calendars
            event           // Event to send to GObj
        );

     **************************************************************/
    function popup_calendar_from_to(
        self,           // GObj to send event
        name,           // Nombre calendario
        from_t,         // value for from, UTC time
        to_t,           // value for to, UTC time
        near_node,      // Show popup near HTML node!
        properties,     // Overwride properties of calendars
        event           // Event to send to GObj
    )
    {

        var calendar_from = {
            select: true,
            icons: false,
            borderless: true,
            type: "day",
            weekNumber: false,
            weekHeader: true,
            events: own_events
        };

        var calendar_to = {
            select: true,
            icons: false,
            borderless: true,
            type: "day",
            weekNumber: false,
            weekHeader: true,
            events: own_events
        };

        if(is_object(properties)) {
            __extend_dict__(calendar_from, properties);
            __extend_dict__(calendar_to, properties);
        }

        __extend_dict__(calendar_from,
            {
                view: "calendar",
                id: name + "from_t",
                name: "from_t"
            }
        );
        __extend_dict__(calendar_to,
            {
                view: "calendar",
                id: name + "to_t",
                name: "to_t"
            }
        );

        var popup_calendar = {
            view: "popup",
            id: name,
            modal: true,
            on: {
                "onHide": function() {
                    this.close();
                }
            },
            body: {
                view: "form",
                padding:5,
                margin: 10,
                rules: {
                    "from_t": webix.rules.isNotEmpty,
                    "to_t": webix.rules.isNotEmpty
                },
                rows: [
                    {
                        cols: [
                            {
                                rows: [
                                    {view: "template", type: "header", height: 40,
                                        template: t("from")
                                    },
                                    calendar_from
                                ]
                            },
                            {
                                rows: [
                                    {view: "template", type: "header", height: 40,
                                        template: t("to")
                                    },
                                    calendar_to
                                ]
                            }
                        ]
                    },
                    {
                        borderless:false,
                    },
                    {
                        cols: [
                            {width:30},
                            {
                                view: "button",
                                name: "cancel",
                                label: t("cancel"),
                                css: "webix_tertiary",
                                click: function(id, e) {
                                    this.getTopParentView().close();
                                }
                            },
                            {width:30},
                            {
                                view: "button",
                                name: "submit",
                                label: t("confirm"),
                                css: "webix_primary",
                                click: function(id, e) {
                                    if(this.getFormView().validate()) {
                                        var values = this.getFormView().getValues();
                                        self.gobj_send_event(
                                            event,
                                            {
                                                from_t: values.from_t.getTime()/1000,
                                                to_t: values.to_t.getTime()/1000,
                                            },
                                            self
                                        );
                                        this.getTopParentView().close();
                                    } else {
                                        display_warning_message(t("select dates please"));
                                    }
                                }
                            },
                            {width:30}
                        ]
                    }
                ]
            }
        };

        if(!near_node) {
            popup_calendar.position = "center";
        }

        var $popup_calendar = webix.ui(popup_calendar);
        if(near_node) {
            $popup_calendar.show(near_node, {pos:"top"});
        } else {
            $popup_calendar.show();
        }

        $$(name + "from_t").selectDate(new Date(from_t * 1000), true);
        $$(name + "to_t").selectDate(new Date(to_t * 1000), true);
    }

    /************************************************************
     *   Own events function
     ************************************************************/
    function own_events(date, isOutside)
    {
        var day = date.getDay();
        if (day==0 || day == 6) {
            return "webix_cal_event";
        }
        if(isOutside) {
            return "webix_cal_day_outside";
        }
        // En daterange usan meses distintos, aquí partimos del mismo mes en los dos calendarios,
        // hay que pensarlo bien.
        //         var desde = $$("desde").getValue();
        //         var hasta = $$("hasta").getValue();
        //         if(desde && hasta) {
        //             var date = redondea_date(date, "day").getTime();
        //             var date_desde = webix.Date.strToDate("%d/%m/%Y")(desde).getTime();
        //             var date_hasta = webix.Date.strToDate("%d/%m/%Y")(hasta).getTime();
        //             if(date >= date_desde && date <=date_hasta) {
        //                 return "webix_cal_range"
        //             }
        //         }

        return "undefined";
    }

    /******************************************************************
     *  t: UTC time
     *  range: intervalo de unidades (hora, semana, mes, año)
     *
     *  Redondea t a la unidad (hora, semana, mes, año)
     *  y devuelve el intervalo (inicial y final) de 'range' unidades
     *
     *  Ej: f(12:30:23, 2) return (12:00, 14:00)
     *
     *  Los valores devueltos start,end son UTC time
     ******************************************************************/
    function get_hours_range(t, range)
    {
        var start = new Date(t * 1000);
        start.setMinutes(0);
        start.setSeconds(0);
        start.setMilliseconds(0);

        var end = webix.Date.add(new Date(start), range, "hour");

        return {
            start: start/1000,
            end: end/1000
        }
    }

    /********************************************
     *  t: UTC time
     *  range: intervalo de unidades
     *
     *  Sobre cualquier hora t del dia,
     *  retorna el rango del dia por dias completos
     *
     *  Retorna UTC time en start,end
     ********************************************/
    function get_days_range(t, range)
    {
        var start = new Date(t * 1000);
        start.setHours(0);
        start.setMinutes(0);
        start.setSeconds(0);
        start.setMilliseconds(0);

        var end = webix.Date.add(new Date(start), range, "day");
        end -= 1000;

        return {
            start: start/1000,
            end: end/1000
        }
    }

    /********************************************
     *  t: UTC time
     *  range: intervalo de unidades
     *
     *  Sobre cualquier dia t de la semana,
     *  retorna el rango de la semana, por semanas completas
     *
     *  WARNING Nuestras semanas empiezan en LUNES !!!
     *
     *  Retorna UTC time en start,end
     ********************************************/
    function get_weeks_range(t, range)
    {
        var start = new Date(t * 1000);

        var wday = start.getDay();
        if(wday == 1) {
            // Estamos en lunes
        } else if(wday > 1) {
            // Dentro de la semana
            start = webix.Date.add(new Date(start), -wday + 1, "day");
        } else if(wday == 0) {
            // Domingo
            start = webix.Date.add(new Date(start), -6, "day");
        }

        start.setHours(0);
        start.setMinutes(0);
        start.setSeconds(0);
        start.setMilliseconds(0);

        var end = webix.Date.add(new Date(start), range, "week");
        end -= 1000;

        return {
            start: start/1000,
            end: end/1000
        }
    }

    /********************************************
     *  t: UTC time
     *  range: intervalo de unidades
     *
     *  Sobre cualquier dia t del mes,
     *  retorna el rango del mes, por meses completos
     *
     *  Retorna UTC time en start,end
     ********************************************/
    function get_months_range(t, range)
    {
        var start = new Date(t * 1000);

        start.setMonth(start.getMonth(), 1);
        start.setHours(0);
        start.setMinutes(0);
        start.setSeconds(0);
        start.setMilliseconds(0);

        var end = webix.Date.add(new Date(start), range, "month");
        end -= 1000;

        return {
            start: start/1000,
            end: end/1000
        }
    }

    /********************************************
     *  t: UTC time
     *  range: intervalo de unidades
     *
     *  Sobre cualquier dia t del año,
     *  retorna el rango del año, por años completos
     *
     *  Retorna UTC time en start,end
     ********************************************/
    function get_years_range(t, range)
    {
        var start = new Date(t * 1000);
        start.setMonth(0, 1);
        start.setHours(0);
        start.setMinutes(0);
        start.setSeconds(0);
        start.setMilliseconds(0);

        var end = webix.Date.add(new Date(start), range, "year");
        end -= 1000;

        return {
            start: start/1000,
            end: end/1000
        }
    }

    /********************************************
     *  Return UTC time of today
     ********************************************/
    function get_today()
    {
        var start = new Date();
        start.setHours(0);
        start.setMinutes(0);
        start.setSeconds(0);
        start.setMilliseconds(0);

        return start/1000;
    }

    /********************************************
     *  Return UTC time of right now
     ********************************************/
    function get_now()
    {
        var start = new Date();
        start.setMilliseconds(0);

        return start/1000;
    }

    /********************************************
     *  t: UTC time
     *  mask: format of moment.js
     ********************************************/
    function display_time(t, mask)
    {
        if(!mask) {
            mask = "DD MMM YYYY";
        }
        return moment.unix(t).format(mask);
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.popup_calendar_from_to = popup_calendar_from_to;
    exports.get_days_range = get_days_range;
    exports.get_weeks_range = get_weeks_range;
    exports.get_months_range = get_months_range;
    exports.get_years_range = get_years_range;
    exports.get_today = get_today;
    exports.get_now = get_now;
    exports.display_time = display_time;

})(this);
