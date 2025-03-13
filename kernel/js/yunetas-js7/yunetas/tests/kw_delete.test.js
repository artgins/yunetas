import { describe, test, expect } from 'vitest';
import { kw_delete } from "yunetas";

// Mock functions to simulate dependencies
function kw_find_path(gobj, kw, path, create) {
    return path.split('`').reduce((obj, key) => obj?.[key], kw);
}

function json_object_del(obj, key) {
    if (obj && key in obj) {
        delete obj[key];
    }
}

describe('kw_delete', () => {
    test('should delete an existing key at the root level', () => {
        const kw = { key1: 'value1', key2: 'value2' };
        kw_delete(null, kw, 'key1');
        expect(kw).not.toHaveProperty('key1');
        expect(kw).toHaveProperty('key2', 'value2');
    });

    test('should delete a nested key using path', () => {
        const kw = { nested: { key1: 'value1', key2: 'value2' } };
        kw_delete(null, kw, 'nested`key1');
        expect(kw.nested).not.toHaveProperty('key1');
        expect(kw.nested).toHaveProperty('key2', 'value2');
    });

    test('should do nothing if the key does not exist', () => {
        const kw = { key1: 'value1' };
        kw_delete(null, kw, 'key2');
        expect(kw).toHaveProperty('key1', 'value1');
    });

    test('should handle deletion at deep nested levels', () => {
        const kw = { a: { b: { c: { d: 'value' } } } };
        kw_delete(null, kw, 'a`b`c`d');
        expect(kw.a.b.c).not.toHaveProperty('d');
    });

    test('should not throw when called on an empty object', () => {
        const kw = {};
        expect(() => kw_delete(null, kw, 'key1')).not.toThrow();
    });
});
