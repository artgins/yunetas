/***********************************************************************
 *          c_yui_map.js
 *
 *          Map Manager
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_create,
    log_error,
    is_array,
    createElement2,
    clean_name,
    kw_has_key,
    is_object,
    json_size,
    json_deep_copy,
    gobj_read_pointer_attr,
    gobj_parent,
    gobj_subscribe_event,
    gobj_read_attr,
    gobj_send_event,
    gobj_read_bool_attr,
    gobj_find_service,
    gobj_create_service,
    gobj_name,
} from "yunetas";

import "maplibre-gl/dist/maplibre-gl.css"; // Import MapLibre styles
import maplibregl from "maplibre-gl"; // Import MapLibre JavaScript

import {
    EditControl,
    MarkerControl
} from "./lib_maplibre.js";

import "./c_yui_map.css"; // Must be in index.js ?

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_MAP";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_JSON,     "devices",          "[]", null, "List of devices"),
SDATA(data_type_t.DTP_JSON,     "map_settings",     0,  {
    style: "https://tiles.openfreemap.org/styles/liberty",
    center: [-3.7038, 40.4168],
    zoom: 9.5,
    scrollZoom: true,
    // sprite: 'http://localhost:8029/static/app/images/devices/sprite/device_sprite', // without file extension
},   "Map settings"),
SDATA(data_type_t.DTP_REAL,     "default_longitude",0,  -3.7038, "Default longitude"),
SDATA(data_type_t.DTP_REAL,     "default_latitude", 0,  40.4168, "Default latitude"),
SDATA(data_type_t.DTP_POINTER,  "$map",             0,  null,    "External HTML container"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "map",   "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fa-solid fa-location-dot", "Icon class"),
SDATA(data_type_t.DTP_INTEGER,  "width",            0,  400,    "Width of the map"),
SDATA(data_type_t.DTP_INTEGER,  "height",           0,  400,    "Height of the map"),
SDATA(data_type_t.DTP_BOOLEAN,  "dimensions_with_parent_observer", 0, false, "Observe parent dimensions"),
SDATA(data_type_t.DTP_INTEGER,  "timeout_retry",    0,  5,      "Timeout retry in seconds"),
SDATA(data_type_t.DTP_INTEGER,  "timeout_idle",     0,  5,      "Idle timeout in seconds"),
SDATA_END()
];

let PRIVATE_DATA = {
    xmap: null,
    geojson: null,
    resizeObserver: null,
    width: 0,      // map size, data got from resize observer
    height: 0,
    markers: {},
    draggedFeature: null,
};

let __gclass__ = null;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    let priv = gobj.priv;

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    let map_settings = json_deep_copy(gobj_read_attr(gobj, "map_settings"));

    Object.assign(map_settings, {
        container: gobj_read_attr(gobj, "$map"),
    });

    /*-----------------------------*
     *  Wrap event handlers
     *-----------------------------*/
    priv.onClick = (ev) => {
        onClick(gobj, ev);
    };
    priv.onMove = (ev) => {
        onMove(gobj, ev);
    };
    priv.onUp = (ev) => {
        onUp(gobj, ev);
    };
    priv.xmouseDown = (ev) => {
        xmouseDown(gobj, ev);
    };
    priv.xtouchStart = (ev) => {
        xtouchStart(gobj, ev);
    };

    /*-----------------------------*
     *      Create the Map
     *-----------------------------*/
    const map = priv.xmap = new maplibregl.Map(map_settings);

    /*-----------------------------*
     *      Controls
     *-----------------------------*/
    // priv.xmap.setRenderWorldCopies(false);
    map.addControl(new maplibregl.NavigationControl());
    map.addControl(new MarkerControl(gobj, {}), 'top-right');
    map.addControl(new EditControl(gobj, {showMarkerDrag: true}), 'top-right');

    priv.marker_user_position = new maplibregl.GeolocateControl({
        positionOptions: {
            enableHighAccuracy: true
        },
        trackUserLocation: false,
        showUserLocation: true
    });
    map.addControl(
        priv.marker_user_position,
        'top-right'
    );

    /*-----------------------------*
     *  Event handlers
     *-----------------------------*/
    map.on('load', () => {
        gobj_send_event(gobj, "EV_MAP_ON_LOAD", {}, gobj);
    });
    map.on('zoomend', () => {
        // if(priv.marker_user_position) {
        //     priv.marker_user_position._updateMarker(null);
        // }
    });

    /*-----------------------------*
     *  Watch resizer native
     *-----------------------------*/
    /* global ResizeObserver */
    if(gobj_read_bool_attr(gobj, "dimensions_with_parent_observer")) {
        // Create a ResizeObserver
        let $map = gobj_read_attr(gobj, "$map");
        const resizeObserver = priv.resizeObserver = new ResizeObserver(entries => {
            for (let entry of entries) {
                const { width, height } = entry.contentRect; // New dimensions
                priv.width = width;
                priv.height = height;

                if(height) { // update only when is not zero
                    $map.style.width = priv.width + 'px';
                    $map.style.height = priv.height + 'px';
                }

                // Con el timeout se ve de blanco a mapa pintado demasiado
                // gobj.clear_timeout();
                // gobj.set_timeout(20);
            }
        });

        // Observe the element
        resizeObserver.observe($map.parentNode);
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let __yui_main__ = gobj_find_service("__yui_main__", true);
    if(__yui_main__) {
        gobj_subscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
    }
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    let priv = gobj.priv;

    if(priv.resizeObserver) {
        priv.resizeObserver.disconnect();
        priv.resizeObserver = null;
    }
    if(priv.xmap) {
        priv.xmap.remove();
        priv.xmap = null;
    }
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *
 ************************************************************/
function decod_coordinates(gobj, coordinates)
{
    let longitude = 0;
    let latitude = 0;

    if(is_array(coordinates) && coordinates.length >=2) {
        /*
         *  We use GeoJSON order: [lng,lat]
         *
         *  "geometry": {
         *      "type": "Point",
         *      "coordinates": [0, 0]
         *  }
         *
         *  WARNING Google Maps is [lat, lng]
         */
        longitude = coordinates[0];
        latitude = coordinates[1];
    } else if(is_object(coordinates)) {
        try {
            coordinates = coordinates.geometry.coordinates;
            longitude = coordinates[0];
            latitude = coordinates[1];
        } catch (e) {
            log_error(e);
        }
    }

    if (latitude > 90 || latitude < -90) {
        // TODO check form, avoid to save invalid coordinates
        latitude = 0;
        longitude = 0;
    }

    longitude = parseFloat(longitude);
    latitude = parseFloat(latitude);
    if(isNaN(longitude)) {
        longitude = 0;
    }
    if(isNaN(latitude)) {
        latitude = 0;
    }
    return {
        lng: longitude,
        lat: latitude
    };
}

/************************************************************
 *
 ************************************************************/
function get_coordinates(gobj, device)
{
    /*
     *  Coordinates can be set in device.settings or in device
     *  If devices.settings are null then get it from device
     */
    let coordinates = decod_coordinates(gobj, device.settings.coordinates);
    if(coordinates.lng === 0 && coordinates.lat === 0) {
        coordinates = decod_coordinates(gobj, device.coordinates);
    }
    if(coordinates.lng === 0 && coordinates.lat === 0) {
        coordinates.lng = gobj_read_attr(gobj, "default_longitude");
        coordinates.lat = gobj_read_attr(gobj, "default_latitude");
    }

    return coordinates;
}

/************************************************************
 *
 ************************************************************/
function center_map(gobj, set)
{
    let priv = gobj.priv;

    const devices = gobj_read_attr(gobj, "devices");
    const bounds = new maplibregl.LngLatBounds();

    if(json_size(devices)===0) {
        return;
    }

    for(let device of devices) {
        let coords = get_coordinates(gobj, device);
        bounds.extend(coords);
    }

    priv.xmap.fitBounds(bounds, {
        padding: 50, // Padding around the edges in pixels
        animate: true, // Smooth transition to the new view
        maxZoom: 15    // Optional: Limit zoom level
    });
}

/************************************************************
 *
 ************************************************************/
function get_marker_id(id)
{
    return 'marker-' + clean_name(id);
}

/************************************************************
 *
 ************************************************************/
function devices2geojson(gobj, devices)
{
    let features = [];
    let geojson = {
        "type": "FeatureCollection",
        "features": features
    };

    for(let device of devices) {
        if(device.hide) {
            continue;
        }
        let coords = get_coordinates(gobj, device);

        // device.image = "http://localhost:8029/static/app/images/devices/enchufe.png";

        let feature = {
            "type": "Feature",
            "id": device.id,
            "geometry": {
                "type": "Point",
                "coordinates": [coords.lng, coords.lat] // GeoJSON order: lng,lat
            },
            "properties": {
                id: device.id,
                name: device.name,
                image: device.image,
                connected: device.connected,
                gobj_service_name: device.gobj_service_name,
            }
        };
        features.push(feature);
    }

    return geojson;
}

/************************************************************
 *  Called from map.on('load',)
 ************************************************************/
function load_devices(gobj)
{
    let priv = gobj.priv;

    let map = priv.xmap;
    let devices = gobj_read_attr(gobj, "devices");
    const geojson_data = priv.geojson = devices2geojson(gobj, devices);

    map.addSource('devices', {
        type: 'geojson',
        data: geojson_data,
        cluster: true,
        clusterMaxZoom: 14, // Max zoom to cluster points on
        clusterRadius: 50,  // Radius of each cluster when clustering points (defaults to 50)
        clusterProperties: {
            connected: ['+', ['case', ['get', 'connected'], 1, 0]]
        }
    });

    // Add cluster circles
    map.addLayer({
        id: 'clusters',
        type: 'circle',
        source: 'devices',
        filter: ['has', 'point_count'],
        paint: {
            'circle-color': [
                'case',
                ['==', ['get', 'connected'], ['get', 'point_count']], 'green',
                'red'
            ],
            'circle-radius': [
                'step',
                ['get', 'point_count'],
                20, 10, 30, 50, 40
            ]
        }
    });

    // Add cluster count labels
    map.addLayer({
        id: 'cluster-count',
        type: 'symbol',
        source: 'devices',
        filter: ['has', 'point_count'],
        layout: {
            'text-field': '{point_count_abbreviated}',
            'text-font': ['Noto Sans Regular'],
            'text-size': 14
        }
    });

    // Add unclustered points
    map.addLayer({
        id: 'unclustered-text',
        type: 'symbol',
        source: 'devices',
        filter: ['!', ['has', 'point_count']],
        layout: {
            'text-field': ['get', 'name'],
            'text-font': ['Noto Sans Regular'],
            'text-size': 12,
            'text-offset': [0, -2.5],
            'text-anchor': 'top'
        },
        paint: {
            'text-color': [
                'case',
                ['get', 'connected'], 'green',
                'red'
            ]
        }
    });

    map.addLayer({
        id: 'unclustered-point',
        type: 'circle',
        source: 'devices',
        filter: ['!', ['has', 'point_count']],
        paint: {
            'circle-color': [
                'case',
                ['get', 'connected'], 'green',
                'red'
            ],
            'circle-radius': 14,
            'circle-stroke-width': 1,
            'circle-stroke-color': '#fff'
        }
    });

    center_map(gobj);

    // inspect a cluster on click
    map.on('click', 'clusters', async (e) => {
        const features = map.queryRenderedFeatures(e.point, {
            layers: ['clusters']
        });
        const clusterId = features[0].properties.cluster_id;
        const zoom = await map.getSource('devices').getClusterExpansionZoom(clusterId);
        map.easeTo({
            center: features[0].geometry.coordinates,
            zoom
        });
    });

    map.on('mouseenter', 'clusters', () => {
        map.getCanvas().style.cursor = 'pointer';
    });
    map.on('mouseleave', 'clusters', () => {
        map.getCanvas().style.cursor = '';
    });

    // Handle mouseenter to show popup
    map.on('mouseenter', 'unclustered-point', (e) => {
        // Change the cursor to a pointer
        map.getCanvas().style.cursor = 'pointer';
    });

    // Handle mouseleave to hide popup
    map.on('mouseleave', 'unclustered-point', () => {
        map.getCanvas().style.cursor = '';
    });

    // Handle click event to show popup
    map.on('click', 'unclustered-point', priv.onClick);

    return 0;
}

/************************************************************
 *  Find the device in devices
 ************************************************************/
function find_device(gobj, id)
{
    let devices = gobj_read_attr(gobj, "devices");
    for(let i = 0; i < devices.length; i++) {
        let device = devices[i];
        if(device.id === id) {
            return device;
        }
    }

    log_error(`find_device: device not found ${id}`);
    return null;
}

/************************************************************
 *  Event handler for click
 ************************************************************/
function onClick(gobj, e)
{
    let priv = gobj.priv;

    const map = priv.xmap;

    // Get feature properties and coordinates
    const coordinates = e.features[0].geometry.coordinates.slice();
    const properties = e.features[0].properties;

    const gobj_service = gobj_find_service(
        properties.gobj_service_name,
        true
    );
    if(gobj_service) {
        let name = clean_name(gobj_name(gobj_service));
        let window_service_name = `window-map-${name}`;

        let popupContent = createElement2(
            ['div',
                {
                    class: 'xmap columns m-0 p-0 is-flex-wrap-wrap',
                },
                gobj_read_attr(gobj_service, "$container")
            ]
        );

        let gobj_window = gobj_create_service(
            window_service_name,
            "C_YUI_WINDOW",
            {
                $parent: document.getElementById('top-layer'),
                width: 700,
                height: 500,
                auto_save_size_and_position: false,
                center: true,
                showMax: false,
                content_size: true,
                body: popupContent
            },
            gobj
        );

    } else {
        // Initialize a popup
        const popup = new maplibregl.Popup({
            closeButton: true,
            closeOnClick: true, // Automatically close when clicking elsewhere
            closeOnMove: true,
            anchor: 'center',
            maxWidth: 'none',
        });

        const popupContent = `
            <b>${properties.name}</b><br>Id: ${properties.id}
        `;
        popup.setLngLat(coordinates).setHTML(popupContent).addTo(map);
    }
}

/************************************************************
 *  Event handlers for dragging
 ************************************************************/
function onMove(gobj, e)
{
    let priv = gobj.priv;

    const coords = e.lngLat;

    // Update the Point feature in `geojson` coordinates
    // and call setData to the source layer `point` on it.
    let device = find_device(gobj, priv.draggedFeature.properties.id);
    if(device) {
        device.settings.coordinates = [coords.lng, coords.lat];
        gobj_send_event(gobj, "EV_REFRESH", {}, gobj);
    }
}

function onUp(gobj, e)
{
    let priv = gobj.priv;

    const coords = e.lngLat;
    const map = priv.xmap;

    // Unbind mouse/touch events
    map.off('mousemove', priv.onMove);
    map.off('touchmove', priv.onMove);

    let coordinates = {};
    coordinates.latitude = coords.lat;
    coordinates.longitude = coords.lng;

    const gobj_service = gobj_find_service(
        priv.draggedFeature.properties.gobj_service_name,
        true
    );
    if(gobj_service) {
        gobj_send_event(gobj_service, "EV_SET_COORDINATES", coordinates, gobj);
    }
}

function xmouseDown(gobj, e)
{
    let priv = gobj.priv;

    // Prevent the default map drag behavior.
    e.preventDefault();

    // Save what feature is moving
    priv.draggedFeature = e.features[0];

    const map = priv.xmap;
    map.on('mousemove', priv.onMove);
    map.once('mouseup', priv.onUp);
}

function xtouchStart(gobj, e)
{
    let priv = gobj.priv;

    if (e.points.length !== 1) {
        return;
    }

    // Prevent the default map drag behavior.
    e.preventDefault();

    // Save what feature is moving
    priv.draggedFeature = e.features[0];

    const map = priv.xmap;
    map.on('touchmove', priv.onMove);
    map.once('touchend', priv.onUp);
}




                /***************************
                 *      Actions
                 ***************************/




/************************************************************
 *
 ************************************************************/
function ac_edit_map(gobj, event, kw, src)
{
    let priv = gobj.priv;

    const map = priv.xmap;
    if(kw_has_key(kw, 'enable_moving_markers')) {
        const set = kw.enable_moving_markers;

        if(set) {
            map.on('mousedown', 'unclustered-point', priv.xmouseDown);
            map.on('touchstart', 'unclustered-point', priv.xtouchStart);
            map.off('click', 'unclustered-point', priv.onClick);

        } else {
            map.off('mousedown', 'unclustered-point', priv.xmouseDown);
            map.off('touchstart', 'unclustered-point', priv.xtouchStart);
            map.on('click', 'unclustered-point', priv.onClick);
        }
    }
    return 0;
}

/************************************************************
 *  Orders (internal click) from control buttons in the map
 ************************************************************/
function ac_control_map(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(kw_has_key(kw, 'center_map')) {
        const set = kw.center_map;
        center_map(gobj, set);
    }

    if(kw_has_key(kw, 'user_location')) {
        const set = kw.user_location;
        if(set) {
            if(navigator.geolocation) {
                navigator.geolocation.getCurrentPosition(
                    (position) => {
                        const latitude = position.coords.latitude;
                        const longitude = position.coords.longitude;
                        priv.maker_user_location = new maplibregl.Marker()
                            .setLngLat([longitude, latitude])
                            .addTo(priv.xmap);
                        priv.xmap.setCenter([longitude, latitude]);
                    },
                    (error) => {

                    }
                );
            }

        } else {
            if(priv.maker_user_location) {
                priv.maker_user_location.remove();
                priv.maker_user_location = undefined;
            }
        }
        center_map(gobj);
    }

    return 0;
}

/************************************************************
 *  Event from map when is ready (loaded)
 ************************************************************/
function ac_map_on_load(gobj, event, kw, src)
{
    load_devices(gobj);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let devices = gobj_read_attr(gobj, "devices");

    let map = priv.xmap;
    if (map.isStyleLoaded()) {
        const geojson_data = devices2geojson(gobj, devices);
        map.getSource('devices').setData(geojson_data);
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_select(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(gobj_read_bool_attr(gobj, "dimensions_with_parent_observer")) {
        let $map = gobj_read_attr(gobj, "$map");
        $map.style.width = priv.width + 'px';
        $map.style.height = priv.height + 'px';
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    if(!gobj_read_bool_attr(gobj, "dimensions_with_parent_observer")) {
        // TODO
    }

    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
};

/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", [
            ["EV_EDIT_MAP",                 ac_edit_map,            null],
            ["EV_CONTROL_MAP",              ac_control_map,         null],
            ["EV_MAP_ON_LOAD",              ac_map_on_load,         null],
            ["EV_REFRESH",                  ac_refresh,             null],
            ["EV_SELECT",                   ac_select,              null],
            ["EV_SHOW",                     ac_show,                null],
            ["EV_HIDE",                     ac_hide,                null],
            ["EV_RESIZE",                   ac_resize,              null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_EDIT_MAP",                 0],
        ["EV_CONTROL_MAP",              0],
        ["EV_MAP_ON_LOAD",              0],
        ["EV_REFRESH",                  0],
        ["EV_SELECT",                   0],
        ["EV_SHOW",                     0],
        ["EV_HIDE",                     0],
        ["EV_RESIZE",                   0]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        0   // gclass_flag
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_yui_map()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_map };
