#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Networking[] = R"JS(
export class NetworkEntity {
    constructor(id, generation) {
        this.id = String(id);
        this.generation = generation;
    }
    get valid() {
        return call("networkValid", this.id, this.generation);
    }
    get entity() {
        return entity(call("networkEntry", this.id, this.generation).entity);
    }
    get owner() {
        return call("networkEntry", this.id, this.generation).owner;
    }
    set owner(value) {
        call("networkOwner", this.id, this.generation, value);
    }
    get state() {
        return call("networkEntry", this.id, this.generation).state;
    }
    set state(value) {
        call("networkState", this.id, this.generation, value);
    }
    forget() {
        return call("networkForget", this.id, this.generation);
    }
}
export const network = {
    host: (options = {}) => call("networkHost", options),
    join: (options = {}) => call("networkJoin", options),
    close: () => call("networkClose"),
    replicate: (value, options = {}) => {
        if (!(value instanceof Entity))
            throw new TypeError("Entity required");
        const id = call("networkReplicate", value.id, options);
        return new NetworkEntity(id, call("networkInfo").generation);
    },
    sendInput: values => call("networkSendInput", values),
    input: peer => call("networkInput", String(peer)),
    setInterest: (position, radius) => call("networkInterest", position, radius),
    get mode() {
        return call("networkInfo").mode;
    },
    get status() {
        return call("networkInfo").status;
    },
    get error() {
        return call("networkInfo").error;
    },
    get peerId() {
        return call("networkInfo").peerId;
    },
    get port() {
        return call("networkInfo").port;
    },
    get launch() {
        return call("networkInfo").launch;
    },
    get stats() {
        return call("networkInfo").stats;
    },
    get peers() {
        return call("networkPeers").map(p => ({
            ...p,
            player: p.player ? new LocalPlayer(p.player) : null
        }));
    },
    get entities() {
        const generation = call("networkInfo").generation;
        return call("networkEntities").map(e => new NetworkEntity(e.id, generation));
    }
};
)JS";
} // namespace ScriptBindings::LibraryParts
