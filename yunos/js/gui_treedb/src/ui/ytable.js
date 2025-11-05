/*
Options Handling:

    id: If provided in the options, it sets the table's id.
    className: If provided, it adds the specified classes to the table.
        If table is not included in the className, it is added.
        If className is not provided, the default classes table
        is-bordered is-striped is-hoverable are used.
    on_header_select: A callback that is called when a header is selected or deselected.
    on_cell_select: A callback that is called when a cell is selected or deselected.

Callbacks:

    The toggleHeader method calls the on_header_select callback with the header's field name and
    selection state if the callback is defined.
    The toggleCell method calls the on_cell_select callback with the cell's row index, column ID,
    value, and selection state if the callback is defined.

// Example usage

    <div id="mytable"></div>

    // Example usage
        const $container = document.getElementById('mytable');
        const columns = [
            { id: 'name', header: 'Name', fillspace: 2 },
            { id: 'age', header: 'Age' },
            { id: 'email', header: 'Email', fillspace: 3 }
        ];

        const options = {
            id: 'example-table',
            className: 'is-narrow',
            height: '400px',
            width: '100%',
            on_header_select: (columnId, isSelected, $header) => {
                console.log(`Header ${columnId} is ${isSelected ? 'selected' : 'deselected'}`);
            },
            on_cell_select: (rowIndex, columnId, isSelected, value, $cell) => {
                console.log(`Cell at row ${rowIndex}, column ${columnId} with value "${value}" is ${isSelected ? 'selected' : 'deselected'}`);
            },
        };

        const table = createYTable($container, columns, options);

        const data = [
            { name: 'John Doe', age: 30, email: 'john@example.com' },
            { name: 'Jane Smith', age: 25, email: 'jane@example.com' },
            { name: 'Sam Green', age: 35, email: 'sam@example.com' },
            { name: 'Alice Brown', age: 28, email: 'alice@example.com' },
            { name: 'Bob White', age: 32, email: 'bob@example.com' },
            { name: 'Charlie Black', age: 45, email: 'charlie@example.com' },
            { name: 'Diane Blue', age: 38, email: 'diane@example.com' },
            { name: 'Eve Red', age: 22, email: 'eve@example.com' },
            { name: 'Frank Gray', age: 27, email: 'frank@example.com' },
            { name: 'Grace Green', age: 29, email: 'grace@example.com' },
            { name: 'Hank Purple', age: 41, email: 'hank@example.com' },
            { name: 'Ivy Pink', age: 26, email: 'ivy@example.com' },
            { name: 'Jack Yellow', age: 34, email: 'jack@example.com' },
            { name: 'Kate Orange', age: 33, email: 'kate@example.com' },
            { name: 'Leo Cyan', age: 36, email: 'leo@example.com' },
            { name: 'Mia Magenta', age: 39, email: 'mia@example.com' }
        ];

        table.loadData(data);

 */

import {
    duplicate_objects,
    is_pure_number,
}  from "yunetas";

import "./ytable.css"; // Must be in index.js ?

let default_options = {
    id: undefined,
    width: "100%",
    height: 400,
    minWidth: undefined,
    minHeight: undefined,

    wrapper_width: undefined,
    wrapper_height: undefined,

    table_class: undefined,

    with_footer: false,
    show_rowIndex: false,
    fillspace_rowIndex: 1.2,

    selectable_header_cell: false,
    selectable_row_cell: false,

    on_cell_select: null,   // => (rowIndex, columnId, isSelected, value, $cell)
    on_header_select: null, // => (columnId, isSelected, $header);
};

class YTable {
    constructor($container, columns, options_ = {}) {
        this.$container = $container;
        this.columns = columns;
        this.options = duplicate_objects(default_options, options_);

        /*
         *  See and check the available options
         */
        this.wrapper_width = parse_size(this.options.wrapper_width);
        this.wrapper_height = parse_size(this.options.wrapper_height);

        this.table_width = parse_size(this.options.width || 600);
        this.table_height = parse_size(this.options.height  || 400);
        this.table_minWidth = parse_size(this.options.minWidth);
        this.table_minHeight = parse_size(this.options.minHeight);


        this.with_footer = this.options.with_footer;
        this.table_id = this.options.id;
        this.table_class = this.options.table_class;

        // Ensure fillspace has a default value of 1 if not provided
        this.columns.forEach(col => {
            if (typeof col.fillspace === 'undefined') {
                col.fillspace = 1;
            }
        });

        this.totalFillspace = this.columns.reduce((sum, col) => sum + col.fillspace, 0);
        // Add extra internal columns
        if(this.options.show_rowIndex) {
            this.totalFillspace += this.options.fillspace_rowIndex;
        }
        this.createTable();
    }

    createTable() {
        this.wrapper = document.createElement('div');
        this.table = document.createElement('table');
        this.thead = document.createElement('thead');
        this.tbody = document.createElement('tbody');
        if(this.with_footer) {
            this.tfoot = document.createElement('tfoot');
        } else {
            this.tfoot = null;
        }

        /*--------------------------*
         *      <div> wrapper
         *--------------------------*/
        this.wrapper.className = 'ytable-wrapper';

        /*--------------------------*
         *      <table>
         *--------------------------*/
        if (this.table_id) {
            this.table.id = this.table_id;
        }

        // Set table classes
        if (this.table_class) {
            let table_class = this.table_class;
            if(!table_class.includes('table')) {
                table_class = `table ${table_class}`;
            }
            if(!table_class.includes('ytable')) {
                table_class = `ytable ${table_class}`;
            }
            this.table.className = table_class;
        } else {
            this.table.className = 'ytable table is-bordered is-striped is-hoverable is-fullwidth';
        }

        /*--------------------------*
         *      <thead>
         *--------------------------*/
        const theadRow = document.createElement('tr');
        if(this.options.show_rowIndex) {
            const th = document.createElement('th');
            th.textContent = "rowIndex";
            th.dataset.id = "rowIndex";
            th.style.width = `${(this.options.fillspace_rowIndex / this.totalFillspace) * 100}%`;
            theadRow.appendChild(th);
        }

        this.columns.forEach(col => {
            const th = document.createElement('th');
            th.textContent = col.header;
            th.dataset.id = col.id;
            th.dataset.formatter = col.formatter;
            th.style.width = `${(col.fillspace / this.totalFillspace) * 100}%`;
            th.addEventListener('click', (evt) => this.on_click_header(col.id, evt));
            theadRow.appendChild(th);
        });
        this.thead.appendChild(theadRow);

        /*--------------------------*
         *      <tfoot>
         *--------------------------*/
        if(this.with_footer) {
            const tfootRow = document.createElement('tr');
            if(this.options.show_rowIndex) {
                const td = document.createElement('td');
                td.textContent = "rowIndex";
                td.dataset.id = "rowIndex";
                td.style.width = `${(this.options.fillspace_rowIndex / this.totalFillspace) * 100}%`;
                tfootRow.appendChild(td);
            }
            this.columns.forEach(col => {
                const td = document.createElement('td');
                td.textContent = `${col.header}`;   // TODO use foot col definition
                td.dataset.id = col.id;
                td.dataset.formatter = col.formatter;
                td.style.width = `${(col.fillspace / this.totalFillspace) * 100}%`;
                td.addEventListener('click', (evt) => this.on_click_footer(col.id, evt));
                tfootRow.appendChild(td);
            });
            this.tfoot.appendChild(tfootRow);
        }

        /*--------------------------*
         *      Make tree
         *--------------------------*/
        this.table.appendChild(this.thead);
        this.table.appendChild(this.tbody);
        if(this.with_footer) {
            this.table.appendChild(this.tfoot);
        }
        this.wrapper.appendChild(this.table);
        this.$container.appendChild(this.wrapper);

        /*--------------------------*
         *      Set styles
         *--------------------------*/
        this.wrapper_width = parse_size(this.options.wrapper_width);
        this.wrapper_height = parse_size(this.options.wrapper_height);

        if(this.wrapper_width) {
            this.wrapper.style.width = this.wrapper_width;
            this.wrapper.style.overflowX = 'auto';
        }
        if(this.wrapper_height) {
            this.wrapper.style.height = this.wrapper_height;
            this.wrapper.style.overflowY = 'auto';
        }

        this.table.style.width = this.table_width;
        if(this.table_minWidth) {
            this.table.style.minWidth = this.table_minWidth;
        }
        if(this.table_minHeight) {
            this.table.style.minHeight = this.table_minHeight;
        }


        this.tbody.style.display = 'block';
        this.tbody.style.height = this.table_height;
        this.tbody.style.width = this.table_width;
        this.tbody.style.overflowY = 'auto';

        this.thead.style.display = 'table';
        this.thead.style.width = this.table_width;
        if(this.with_footer) {
            this.tfoot.style.display = 'table';
            this.tfoot.style.width = this.table_width;
        }
    }

    /*----------------------------------*
     *          API
     *----------------------------------*/
    destroy() {
        /*
         *  WARNING things to do:
         *      - remove all DOM event handlers
         *      - remove elements from the DOM
         *      - delete from this.
         */
        this.clearData();
        this.wrapper.remove();

        delete this.table;
        delete this.thead;
        delete this.tbody;
        delete this.tfoot;
        delete this.wrapper;
    }

    loadData(data) {
        // TODO repon this.clearData();

        // Step 1: Create a temporary tbody
        let tempTbody = document.createElement('tbody');
        tempTbody.style.display = 'block';
        tempTbody.style.height = this.table_height;
        tempTbody.style.width = this.table_width;
        tempTbody.style.overflowY = 'auto';

        data.forEach((record, rowIndex) => {
            const tr = document.createElement('tr');
            if(this.options.show_rowIndex) {
                const td = document.createElement('td');
                td.textContent = rowIndex + 1;
                td.dataset.id = "rowIndex";
                td.style.width = `${(this.options.fillspace_rowIndex / this.totalFillspace) * 100}%`;
                td.classList.add('is-size-7');
                tr.appendChild(td);
            }

            this.columns.forEach(col => {
                const td = document.createElement('td');
                if(col.formatter) {
                    td.innerHTML = col.formatter(record[col.id], rowIndex, col.id).trim();
                } else {
                    td.textContent = record[col.id];
                }
                td.dataset.id = col.id;
                td.dataset.rowIndex = rowIndex;
                td.style.width = `${(col.fillspace / this.totalFillspace) * 100}%`;
                td.style.overflow = 'hidden';
                td.style.textOverflow = 'ellipsis';
                td.style.whiteSpace = 'pre-wrap';
                // TODO td.addEventListener('click', (evt) => this.on_click_cell(rowIndex, col.id, evt));
                tr.appendChild(td);
            });
            tempTbody.appendChild(tr);
            this._checkOverflow(tr);
        });

        let realTbody = this.tbody;
        realTbody.parentNode.replaceChild(tempTbody, realTbody);
        this.tbody = tempTbody;
    }

    _checkOverflow(row) {
        row.querySelectorAll('td').forEach(td => {
            if (td.scrollHeight > td.clientHeight) {
                td.classList.add('overflow');
            }
        });
    }

    clearData() {
        this.tbody.innerHTML = '';
        // this.tbody.replaceChildren();
    }

    toggleHeader(columnId) {
        const header = this.thead.querySelector(`th[data-id="${columnId}"]`);
        if (header) {
            const isSelected = header.classList.toggle('is-success');
            if (this.options.on_header_select) {
                this.options.on_header_select(columnId, isSelected, header);
            }
        }
    }

    selectHeader(columnId) {
        const header = this.thead.querySelector(`th[data-id="${columnId}"]`);
        if (header) {
            header.classList.add('is-success');
            if (this.options.on_header_select) {
                this.options.on_header_select(columnId, true, header);
            }
        }
    }

    deselectHeader(columnId) {
        const header = this.thead.querySelector(`th[data-id="${columnId}"]`);
        if (header) {
            header.classList.remove('is-success');
            if (this.options.on_header_select) {
                this.options.on_header_select(columnId, false, header);
            }
        }
    }

    selectAllHeaders() {
        const headers = this.thead.querySelectorAll('th');
        headers.forEach(header => {
            header.classList.add('is-success');
            if (this.options.on_header_select) {
                this.options.on_header_select(header.dataset.id, true, header);
            }
        });
    }

    deselectAllHeaders() {
        const headers = this.thead.querySelectorAll('th');
        headers.forEach(header => {
            header.classList.remove('is-success');
            if (this.options.on_header_select) {
                this.options.on_header_select(header.dataset.id, false, header);
            }
        });
    }

    toggleCell(rowIndex, columnId) {
        const row = this.tbody.children[rowIndex];
        if (row) {
            const cell = row.querySelector(`td[data-id="${columnId}"]`);
            if (cell) {
                const isSelected = cell.classList.toggle('is-info');
                if (this.options.on_cell_select) {
                    this.options.on_cell_select(rowIndex, columnId, isSelected, cell.textContent, cell);
                }
            }
        }
    }

    selectCell(rowIndex, columnId) {
        const row = this.tbody.children[rowIndex];
        if (row) {
            const cell = row.querySelector(`td[data-id="${columnId}"]`);
            if (cell) {
                cell.classList.add('is-info');
                if (this.options.on_cell_select) {
                    this.options.on_cell_select(rowIndex, columnId, true, cell.textContent, cell);
                }
            }
        }
    }

    deselectCell(rowIndex, columnId) {
        const row = this.tbody.children[rowIndex];
        if (row) {
            const cell = row.querySelector(`td[data-id="${columnId}"]`);
            if (cell) {
                cell.classList.remove('is-info');
                if (this.options.on_cell_select) {
                    this.options.on_cell_select(rowIndex, columnId, false, cell.textContent, cell);
                }
            }
        }
    }

    getSelectedHeaders() {
        const selectedHeaders = [];
        const headers = this.thead.querySelectorAll('th.is-success');
        headers.forEach(header => {
            selectedHeaders.push(header.dataset.id);
        });
        return selectedHeaders;
    }

    getSelectedCells() {
        const selectedCells = [];
        for (let rowIndex = 0; rowIndex < this.tbody.children.length; rowIndex++) {
            const row = this.tbody.children[rowIndex];
            const cells = row.querySelectorAll('td.is-info');
            cells.forEach(cell => {
                selectedCells.push({
                    rowIndex: rowIndex,
                    field: cell.dataset.id,
                    value: cell.textContent
                });
            });
        }
        return selectedCells;
    }

    /*----------------------------------*
     *          Events
     *----------------------------------*/
    on_click_header(column_id, evt) {
        // TODO use a custom function to process click, toggle header must be an option
        if(this.options.selectable_header_cell) {
            this.toggleHeader(column_id);
        }
    }

    on_click_cell(rowIndex, column_id, evt) {
        // TODO use a custom function to process click, toggle header must be an option
        if(this.options.selectable_row_cell) {
            this.toggleCell(rowIndex, column_id);
        }
    }

    on_click_footer(column_id, evt) {
    }

}

function parse_size(n) {
    if(is_pure_number(n)) {
        return `${n}px`;
    } else {
        return n;
    }
}

// Factory function to create YTable instance
function createYTable($container, columns, options) {
    return new YTable($container, columns, options);
}

export {YTable, createYTable};
