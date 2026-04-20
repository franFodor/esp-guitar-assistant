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

// MIDI for each open string: string 0 = low E2, string 5 = high E4
const OPEN_MIDI = [40, 45, 50, 55, 59, 64];

let selectedRoot     = "A";
let selectedScale    = "Pentatonic Minor";
let isListening      = false;
let pollInterval     = null;
let lastDetectedNote = null;

function getPitchClass(noteName) {
    return noteName.replace(/\d+$/, '');
}

// Returns full note name with octave, e.g. "A2", "D#3"
function getNoteWithOctave(string, fret) {
    const midi = OPEN_MIDI[string] + fret;
    return NOTE_NAMES[midi % 12] + (Math.floor(midi / 12) - 1);
}

function getScaleNotes() {
    const rootIdx = NOTE_NAMES.indexOf(selectedRoot);
    return SCALES[selectedScale].map(i => NOTE_NAMES[(rootIdx + i) % 12]);
}

function renderScaleBadges() {
    const notes = getScaleNotes();
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
    const fretboard  = document.getElementById('fretboard');
    fretboard.innerHTML = '';

    for (let string = 5; string >= 0; string--) {
        const xoCell = document.createElement('div');
        xoCell.className = 'fret-xo';

        // Open string (fret 0)
        const openFull  = getNoteWithOctave(string, 0);
        const openPitch = getPitchClass(openFull);
        if (scaleNotes.includes(openPitch)) {
            const circle = document.createElement('div');
            circle.className = 'fret-circle fret-xo-circle scale-note-circle';
            if (openPitch === selectedRoot) circle.classList.add('scale-root-circle');
            circle.dataset.note = openFull;   // "A2" — used for octave-exact matching
            circle.textContent  = openPitch;  // "A"  — display only pitch class
            xoCell.appendChild(circle);
        }
        fretboard.appendChild(xoCell);

        for (let fret = 0; fret < 12; fret++) {
            const cell = document.createElement('div');
            cell.className = 'fret-cell';
            if (string === 0) cell.classList.add('last-row');
            if (string === 0 && (fret === 0 || fret === 2 || fret === 4 || fret === 6 || fret === 8 || fret === 11)) {
                cell.innerHTML = `<span class='fret-label'>${fret + 1}</span>`;
            }

            const noteFull  = getNoteWithOctave(string, fret + 1);
            const notePitch = getPitchClass(noteFull);
            if (scaleNotes.includes(notePitch)) {
                const circle = document.createElement('div');
                circle.className = 'fret-circle scale-note-circle';
                if (notePitch === selectedRoot) circle.classList.add('scale-root-circle');
                circle.dataset.note = noteFull;   // "A3" — octave-exact
                circle.textContent  = notePitch;  // "A"
                cell.appendChild(circle);
            }

            fretboard.appendChild(cell);
        }
    }
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
        // Match exact note+octave — only the fret positions for this specific A2/A3/etc. light up
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
    const btn = document.getElementById('listen-btn');

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
        selectedRoot = e.target.value;
        lastDetectedNote = null;
        renderScaleBadges();
        renderFretboard();
    });

    scaleDropdown.addEventListener('change', e => {
        selectedScale = e.target.value;
        lastDetectedNote = null;
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

    populateDropdowns();
    renderScaleBadges();
    renderFretboard();

    document.getElementById('listen-btn').addEventListener('click', toggleListening);
});
