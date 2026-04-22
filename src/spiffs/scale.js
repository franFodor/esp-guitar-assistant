const TEST_MODE = false;

const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

const SCALES = {
    "Major":            [0, 2, 4, 5, 7, 9, 11],
    "Natural Minor":    [0, 2, 3, 5, 7, 8, 10],
    "Pentatonic Major": [0, 2, 4, 7, 9],
    "Pentatonic Minor": [0, 3, 5, 7, 10],
    "Blues":            [0, 3, 5, 6, 7, 10],
    "Dorian":           [0, 2, 3, 5, 7, 9, 10],
    "Mixolydian":       [0, 2, 4, 5, 7, 9, 10],
};

// MIDI for each open string: index 0 = low E2, index 5 = high E4
const OPEN_MIDI   = [40, 45, 50, 55, 59, 64];
const WINDOW_SIZE = 5;
const MAX_FRET    = 15;                          // enough for all 5 pentatonic positions
const MAX_START   = MAX_FRET - WINDOW_SIZE + 1; // = 11

let selectedRoot     = "A";
let selectedScale    = "Pentatonic Minor";
let positions        = [];   // list of position start frets, computed from scale
let positionIdx      = 0;    // which position we're currently showing
let isListening      = false;
let pollInterval     = null;
let lastDetectedNote = null;

function getPitchClass(noteName) {
    return noteName.replace(/\d+$/, '');
}

function getNoteWithOctave(string, fret) {
    const midi = OPEN_MIDI[string] + fret;
    return NOTE_NAMES[midi % 12] + (Math.floor(midi / 12) - 1);
}

function getScaleNotes() {
    const rootIdx = NOTE_NAMES.indexOf(selectedRoot);
    return SCALES[selectedScale].map(i => NOTE_NAMES[(rootIdx + i) % 12]);
}

// Positions are determined by where each scale note first appears on the low E string.
// This matches standard guitar pedagogy (e.g. A minor pentatonic → 5 box positions).
function computePositions() {
    const scaleNotes = getScaleNotes();
    const lowEOpen   = OPEN_MIDI[0] % 12; // 4 = E

    const frets = scaleNotes.map(note => {
        const noteIdx = NOTE_NAMES.indexOf(note);
        const fret    = (noteIdx - lowEOpen + 12) % 12;
        return fret === 0 ? 1 : fret; // fret 0 = open string → show from fret 1
    });

    return [...new Set(frets)]
        .sort((a, b) => a - b)
        .filter(f => f <= MAX_START);
}

function updatePositionLabel() {
    const start = positions[positionIdx] ?? 1;
    const total = positions.length;
    document.getElementById('position-label').textContent =
        `Position ${positionIdx + 1} / ${total}  (frets ${start}–${start + WINDOW_SIZE - 1})`;
    document.getElementById('btn-prev').disabled = positionIdx <= 0;
    document.getElementById('btn-next').disabled = positionIdx >= positions.length - 1;
}

function renderScaleBadges() {
    const notes     = getScaleNotes();
    const container = document.getElementById('scale-notes-badges');
    container.innerHTML = '';
    notes.forEach((note, i) => {
        const badge = document.createElement('span');
        badge.className = 'scale-note' + (i === 0 ? ' root' : ' interval');
        badge.textContent = note;
        container.appendChild(badge);
    });
}

function renderFretboard() {
    const scaleNotes = getScaleNotes();
    const start      = positions[positionIdx] ?? 1;
    const fretboard  = document.getElementById('fretboard');
    fretboard.innerHTML = '';

    fretboard.style.gridTemplateColumns = `32px repeat(${WINDOW_SIZE}, 72px)`;
    fretboard.style.minWidth = (32 + WINDOW_SIZE * 72) + 'px';

    for (let string = 5; string >= 0; string--) {
        const xoCell = document.createElement('div');
        xoCell.className = 'fret-xo';

        // Only show open-string notes when the window starts at fret 1
        if (start === 1) {
            const openFull  = getNoteWithOctave(string, 0);
            const openPitch = getPitchClass(openFull);
            if (scaleNotes.includes(openPitch)) {
                const circle = document.createElement('div');
                circle.className = 'fret-circle fret-xo-circle scale-note-circle';
                if (openPitch === selectedRoot) circle.classList.add('scale-root-circle');
                circle.dataset.note = openFull;
                circle.textContent  = openPitch;
                xoCell.appendChild(circle);
            }
        }
        fretboard.appendChild(xoCell);

        for (let f = 0; f < WINDOW_SIZE; f++) {
            const fret = start + f;
            const cell = document.createElement('div');
            cell.className  = 'fret-cell';
            cell.style.width = '100%';
            if (string === 0) cell.classList.add('last-row');
            if (string === 0) {
                // Show fret number at standard marker frets (3,5,7,9,12,15)
                const markers = new Set([3, 5, 7, 9, 12, 15]);
                if (markers.has(fret)) {
                    cell.innerHTML = `<span class='fret-label'>${fret}</span>`;
                }
            }

            const noteFull  = getNoteWithOctave(string, fret);
            const notePitch = getPitchClass(noteFull);
            if (scaleNotes.includes(notePitch)) {
                const circle = document.createElement('div');
                circle.className = 'fret-circle scale-note-circle';
                if (notePitch === selectedRoot) circle.classList.add('scale-root-circle');
                circle.dataset.note = noteFull;
                circle.textContent  = notePitch;
                cell.appendChild(circle);
            }

            fretboard.appendChild(cell);
        }
    }

    updatePositionLabel();
}

function highlightDetected(detectedNote) {
    document.querySelectorAll('.scale-note-circle').forEach(el => {
        el.classList.remove('fret-circle-correct');
    });

    const display = document.getElementById('detected-note-display');

    if (!detectedNote) {
        display.textContent = '--';
        display.style.color = 'var(--fg)';
        return;
    }

    const pitchClass = getPitchClass(detectedNote);
    const inScale    = getScaleNotes().includes(pitchClass);

    display.textContent = detectedNote;
    display.style.color = inScale ? 'var(--green)' : 'var(--red)';

    if (inScale) {
        document.querySelectorAll(`.scale-note-circle[data-note="${detectedNote}"]`).forEach(el => {
            el.classList.add('fret-circle-correct');
        });
    }
}

async function poll() {
    let j;
    if (TEST_MODE) {
        j = { note: "A2", frequency: 110.0, cents: 0.0 };
    } else {
        try {
            const r = await fetch('/api/note');
            j = await r.json();
        } catch (e) { return; }
    }

    if (!j || j.frequency <= 0) return;

    if (j.note !== lastDetectedNote) {
        lastDetectedNote = j.note;
        highlightDetected(j.note);
    }
}

function toggleListening() {
    isListening = !isListening;
    const btn   = document.getElementById('listen-btn');

    if (isListening) {
        btn.textContent = 'Stop Listening';
        btn.classList.add('listening');
        pollInterval = setInterval(poll, 50);
    } else {
        btn.textContent = 'Start Listening';
        btn.classList.remove('listening');
        clearInterval(pollInterval);
        pollInterval     = null;
        lastDetectedNote = null;
        highlightDetected(null);
    }
}

function populateDropdowns() {
    const rootDropdown = document.getElementById('root-dropdown');
    NOTE_NAMES.forEach(note => {
        const opt = document.createElement('option');
        opt.value = note; opt.textContent = note;
        if (note === selectedRoot) opt.selected = true;
        rootDropdown.appendChild(opt);
    });

    const scaleDropdown = document.getElementById('scale-dropdown');
    Object.keys(SCALES).forEach(name => {
        const opt = document.createElement('option');
        opt.value = name; opt.textContent = name;
        if (name === selectedScale) opt.selected = true;
        scaleDropdown.appendChild(opt);
    });

    rootDropdown.addEventListener('change', e => {
        selectedRoot     = e.target.value;
        lastDetectedNote = null;
        positions        = computePositions();
        // Jump to the position closest to the root on the low E string
        const lowEOpen   = OPEN_MIDI[0] % 12;
        const rootFret   = (NOTE_NAMES.indexOf(selectedRoot) - lowEOpen + 12) % 12 || 1;
        positionIdx      = positions.reduce((best, f, i) =>
            Math.abs(f - rootFret) < Math.abs(positions[best] - rootFret) ? i : best, 0);
        renderScaleBadges();
        renderFretboard();
    });

    scaleDropdown.addEventListener('change', e => {
        selectedScale    = e.target.value;
        lastDetectedNote = null;
        positions        = computePositions();
        positionIdx      = Math.min(positionIdx, positions.length - 1);
        renderScaleBadges();
        renderFretboard();
    });
}

$(document).ready(function() {
    fetch('/api/mode', { method: 'POST', body: 'note' }).catch(() => {});

    fetch('nav.html')
        .then(r => r.text())
        .then(data => {
            $('#navbar').html(data);
            $('#nav-scale').addClass('active').append('<span class="sr-only">(current)</span>');
        });

    positions   = computePositions();
    // Start at the root position (position whose start fret == root on low E)
    const lowEOpen = OPEN_MIDI[0] % 12;
    const rootFret = (NOTE_NAMES.indexOf(selectedRoot) - lowEOpen + 12) % 12 || 1;
    positionIdx    = positions.reduce((best, f, i) =>
        Math.abs(f - rootFret) < Math.abs(positions[best] - rootFret) ? i : best, 0);

    populateDropdowns();
    renderScaleBadges();
    renderFretboard();

    document.getElementById('listen-btn').addEventListener('click', toggleListening);

    document.getElementById('btn-prev').addEventListener('click', () => {
        if (positionIdx > 0) { positionIdx--; lastDetectedNote = null; highlightDetected(null); renderFretboard(); }
    });
    document.getElementById('btn-next').addEventListener('click', () => {
        if (positionIdx < positions.length - 1) { positionIdx++; lastDetectedNote = null; highlightDetected(null); renderFretboard(); }
    });
});
