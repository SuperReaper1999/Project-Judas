// Durable district ledgers belong to their own ordinary script instances, so the
// engine can suspend/capture them independently. This is a transient merged view
// for interaction code, never stored in Player.state or serialized as a Proxy.
import { saves, scenes } from 'judas';
export const housingRegions = new Map();
const names = ['objects', 'containers', 'doors', 'boards'];
export function emptyDistrictHousing() {
    return Object.fromEntries(names.map((name) => [name, {}]));
}
export function housingView(root) {
    const tables = new Map();
    for (const name of names) {
        const entries = () => [
            root[name],
            ...[...housingRegions.values()].map(
                (owner) => owner.state.housing[name],
            ),
        ];
        const containing = (key) =>
            entries().find((table) => Object.hasOwn(table, key));
        const destination = (key) => {
            const e = typeof key === 'string' ? saves.resolve(key) : null;
            return e?.valid
                ? housingRegions.get(scenes.owner(e))?.state.housing[name] ||
                      root[name]
                : root[name];
        };
        tables.set(
            name,
            new Proxy(
                {},
                {
                    get(_, key) {
                        return containing(key)?.[key];
                    },
                    set(_, key, value) {
                        (containing(key) || destination(key))[key] = value;
                        return true;
                    },
                    deleteProperty(_, key) {
                        const table = containing(key);
                        if (table) delete table[key];
                        return true;
                    },
                    ownKeys() {
                        return [
                            ...new Set(
                                entries().flatMap((table) =>
                                    Object.keys(table),
                                ),
                            ),
                        ];
                    },
                    getOwnPropertyDescriptor(_, key) {
                        const table = containing(key);
                        return table
                            ? {
                                  value: table[key],
                                  enumerable: true,
                                  configurable: true,
                                  writable: true,
                              }
                            : undefined;
                    },
                },
            ),
        );
    }
    return new Proxy(root, {
        get(target, key) {
            return tables.get(key) || target[key];
        },
    });
}
