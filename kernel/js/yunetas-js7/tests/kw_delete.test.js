import { describe, test, expect } from 'vitest';
import {
    kw_delete
} from "yunetas";

// Test suite for kw_delete function
describe('kw_delete', () => {
    test('should delete an existing key', () => {
        const obj = { key1: 'value1', key2: 'value2' };
        kw_delete(obj, 'key1');
        expect(obj).not.toHaveProperty('key1');
        expect(obj).toHaveProperty('key2', 'value2');
    });

    test('should do nothing if the key does not exist', () => {
        const obj = { key1: 'value1' };
        kw_delete(obj, 'key2');
        expect(obj).toHaveProperty('key1', 'value1');
    });

    test('should handle nested objects', () => {
        const obj = { nested: { key1: 'value1' } };
        kw_delete(obj.nested, 'key1');
        expect(obj.nested).not.toHaveProperty('key1');
    });

    test('should not throw when called on empty object', () => {
        const obj = {};
        expect(() => kw_delete(obj, 'key1')).not.toThrow();
    });

    test('should return undefined when key does not exist', () => {
        const obj = { key1: 'value1' };
        expect(kw_delete(obj, 'key2')).toBeUndefined();
    });

    test('should delete and return the value of the deleted key', () => {
        const obj = { key1: 'value1' };
        const deletedValue = kw_delete(obj, 'key1');
        expect(deletedValue).toBe('value1');
        expect(obj).not.toHaveProperty('key1');
    });

    test('should work with non-string keys', () => {
        const obj = { 1: 'one', true: 'boolean' };
        kw_delete(obj, 1);
        expect(obj).not.toHaveProperty('1');
        expect(obj).toHaveProperty('true', 'boolean');
    });
});
