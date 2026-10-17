// Deliberate lab defect. Change 0 to 2, save, check sources, then reload scripts.
export const calibration = { gain: 0 };
export function calibratedDistance(phase) {
    const gain = calibration.gain;
    if (gain <= 0) throw new RangeError('Calibration gain must be positive. Edit Assets/scripts/calibration.js:2 to gain: 2, then deliberately reload.');
    const distance = Math.sin(phase) * gain;
    return distance;
}
