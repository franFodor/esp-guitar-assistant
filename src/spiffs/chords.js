// Chord definitions: [mute/open/fret, finger number]
// null = not played, 0 = open, 1+ = fret number; finger: 1-4
// notes: array of note names that make up the chord
const CHORDS = {
    "A major": {
        positions: [
            [null, null], // E
            [0, null],    // A
            [2, 1],       // D
            [2, 2],       // G
            [2, 3],       // B
            [0, null]     // e
        ],
        notes: ["A", "C#", "E"]
    },
    "E major": {
        positions: [
            [0, null],    // E
            [2, 2],       // A
            [2, 3],       // D
            [1, 1],       // G
            [0, null],    // B
            [0, null]     // e
        ],
        notes: ["E", "G#", "B"]
    },
    "D major": {
        positions: [
            [null, null], // E
            [null, null], // A
            [0, null],    // D
            [2, 1],       // G
            [3, 3],       // B
            [2, 2]        // e
        ],
        notes: ["D", "F#", "A"]
    },
    "C major": {
        positions: [
            [null, null], // E
            [3, 3],       // A
            [2, 2],       // D
            [0, null],    // G
            [1, 1],       // B
            [0, null]     // e
        ],
        notes: ["C", "E", "G"]
    },
    "G major": {
        positions: [
            [3, 2],       // E
            [2, 1],       // A
            [0, null],    // D
            [0, null],    // G
            [0, null],    // B
            [3, 3]        // e
        ],
        notes: ["G", "B", "D"]
    },
    "A minor": {
        positions: [
            [null, null], // E
            [0, null],    // A
            [2, 2],       // D
            [2, 3],       // G
            [1, 1],       // B
            [0, null]     // e
        ],
        notes: ["A", "C", "E"]
    },
    "F major": {
        positions: [
            [1, 1],       // E
            [3, 3],       // A
            [3, 4],       // D
            [2, 2],       // G
            [1, 1],       // B
            [1, 1]        // e
        ],
        notes: ["F", "A", "C"]
    }

};