// Transient presentation state only. Saved dialogue/quests remain text/data.
import { VOICES } from './voice-data.js';
export class DialogueVoice {
    constructor(world, warn) {
        this.world = world;
        this.warn = warn;
        this.current = null;
        this.completed = null;
        this.warned = new Set();
    }
    silence() {
        if (this.current?.valid) this.current.stopAudio();
        this.current = null;
    }
    stop() {
        this.silence();
        this.completed = null;
    }
    show(actor, index) {
        const id = VOICES[actor]?.[index];
        if (!id) {
            this.stop();
            return;
        }
        if (this.completed === id) return;
        if (this.current?.id !== id) this.stop();
        const emitter = this.world.entity(id);
        if (!emitter?.valid) return; // Keep subtitles; no stale entity handles.
        const info = emitter.audio;
        if (!info?.ready) {
            if (info?.state === 'failed') this.failure(id, info.error);
            return; // Retry after native resource publication, without a timer.
        }
        try {
            if (emitter.playAudio()) {
                this.current = emitter;
                this.completed = id;
            }
        } catch (error) {
            this.failure(id, error.message);
        }
    }
    failure(id, error) {
        if (this.warned.has(id)) return;
        this.warned.add(id);
        this.warn('Dialogue audio unavailable; subtitles remain: ' + (error || id));
    }
}
