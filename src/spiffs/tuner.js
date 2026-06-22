const noteToMidi = {
    "C2": 36, "C#2": 37, "D2": 38, "D#2": 39, "E2": 40, "F2": 41, "F#2": 42,
    "G2": 43, "G#2": 44, "A2": 45, "A#2": 46, "B2": 47,
    "C3": 48, "C#3": 49, "D3": 50, "D#3": 51, "E3": 52, "F3": 53, "F#3": 54,
    "G3": 55, "G#3": 56, "A3": 57, "A#3": 58, "B3": 59,
    "C4": 60, "C#4": 61, "D4": 62, "D#4": 63, "E4": 64, "F4": 65, "F#4": 66,
    "G4": 67, "G#4": 68, "A4": 69, "A#4": 70, "B4": 71
};

const CUSTOM_NOTES = Object.keys(noteToMidi);
const CUSTOM_DEFAULT = ["E2", "A2", "D3", "G3", "B3", "E4"];

const tunings = {
    "Standard": ["E2", "A2", "D3", "G3", "B3", "E4"],
    "D Standard": ["D2", "G2", "C3", "F3", "A3", "D3"],
    "Drop D": ["D2", "A2", "D3", "G3", "B3", "E4"]
};

let customTuning  = JSON.parse(localStorage.getItem('customTuning')  || 'null') || [...CUSTOM_DEFAULT];
let customApplied = localStorage.getItem('customTuningApplied') === 'true';
tunings['Custom'] = [...customTuning];

let currentTuning = "Standard";
let lastDetectedTime = null;
const SILENCE_TIMEOUT_MS = 5000;

let tuneStringIdx = -1;
let tuneStart = null;
let tunedStrings = new Set();

const tuningNames = Object.keys(tunings);
let selectedTuning = tuningNames[0];

function noteToFrequency(noteName) {
    const midiNote = noteToMidi[noteName];
    return A4 * Math.pow(2, (midiNote - 69) / 12);
}

function findBestMatchingString(detectedNote, detectedFreq) {
    const tuning = tunings[currentTuning];
    const detectedPitch = detectedNote.replace(/\d+$/, '');
    let bestIdx = -1;
    let bestDiff = Infinity;
    tuning.forEach((stringNote, idx) => {
        if (stringNote.replace(/\d+$/, '') === detectedPitch) {
            const diff = Math.abs(noteToFrequency(stringNote) - detectedFreq);
            if (diff < bestDiff) { bestDiff = diff; bestIdx = idx; }
        }
    });
    return bestIdx;
}

function resetStringIndicators() {
    const tuning = tunings[currentTuning];
    tuning.forEach((_, idx) => {
        const el = document.getElementById('string-' + idx);
        if (el) el.className = 'string-indicator';
    });
    tuneStringIdx = -1;
    tuneStart = null;
    tunedStrings.clear();
}

function selectTuning(tuningName) {
    currentTuning = tuningName;
    selectedTuning = tuningName;
    updateTuningDisplay();
}

function updateTuningDisplay() {
    tuneStringIdx = -1;
    tuneStart = null;
    tunedStrings.clear();

    if (currentTuning === 'Custom') {
        const labels = ['6', '5', '4', '3', '2', '1'];

        const appliedHtml = `<div id="custom-applied-view" class="text-center"${customApplied ? '' : ' style="display:none"'}>` +
            customTuning.map((note, idx) =>
                `<span class="string-indicator" id="string-${idx}">${note}</span>`
            ).join(' | ') +
            '</div>';

        let editorHtml = `<div id="custom-editor-view" class="custom-tuning-grid"${customApplied ? ' style="display:none"' : ''}>`;
        customTuning.forEach((note, idx) => {
            const opts = CUSTOM_NOTES.map(n =>
                `<option value="${n}"${n === note ? ' selected' : ''}>${n}</option>`
            ).join('');
            editorHtml += `<div class="custom-string-col">
                <div class="custom-string-label">${labels[idx]}</div>
                <select class="custom-string-select" data-string="${idx}">${opts}</select>
            </div>`;
        });
        editorHtml += '</div>';

        const buttonsHtml = `<div class="text-center mt-2">
            <button id="custom-apply-btn" class="btn"${customApplied ? ' style="display:none"' : ''}>Apply</button>
            <button id="custom-edit-btn" class="btn btn-secondary"${customApplied ? '' : ' style="display:none"'}>Edit</button>
        </div>`;

        document.getElementById('tuning-display').innerHTML = appliedHtml + editorHtml + buttonsHtml;

        document.querySelectorAll('.custom-string-select').forEach(sel => {
            sel.addEventListener('change', function() {
                const idx = parseInt(this.dataset.string);
                customTuning[idx] = this.value;
                tunings['Custom'] = [...customTuning];
                localStorage.setItem('customTuning', JSON.stringify(customTuning));
                const span = document.getElementById('string-' + idx);
                if (span) span.textContent = this.value;
                tuneStringIdx = -1;
                tuneStart = null;
                tunedStrings.clear();
            });
        });

        document.getElementById('custom-apply-btn').addEventListener('click', function() {
            customApplied = true;
            localStorage.setItem('customTuningApplied', 'true');
            document.getElementById('custom-applied-view').style.display = '';
            document.getElementById('custom-editor-view').style.display = 'none';
            this.style.display = 'none';
            document.getElementById('custom-edit-btn').style.display = 'inline-block';
            tuneStringIdx = -1;
            tuneStart = null;
            tunedStrings.clear();
        });

        document.getElementById('custom-edit-btn').addEventListener('click', function() {
            customApplied = false;
            localStorage.setItem('customTuningApplied', 'false');
            document.getElementById('custom-applied-view').style.display = 'none';
            document.getElementById('custom-editor-view').style.display = '';
            this.style.display = 'none';
            document.getElementById('custom-apply-btn').style.display = 'inline-block';
        });

        return;
    }

    const tuning = tunings[currentTuning];
    const html = '<div class="text-center">' +
        tuning.map((note, idx) =>
            '<span class="string-indicator" id="string-' + idx + '">' + note + '</span>'
        ).join(' | ') +
        '</div>';
    document.getElementById('tuning-display').innerHTML = html;
}

function renderTuningDropdown() {
    const dropdown = document.getElementById('tuning-dropdown');
    dropdown.innerHTML = '';
    ["Standard", "D Standard", "Drop D", "Custom"].forEach(name => {
        const option = document.createElement('option');
        option.value = name;
        option.textContent = name;
        dropdown.appendChild(option);
    });
    dropdown.value = selectedTuning;
    updateTuningDisplay();
}

function handleTuningDropdownChange(e) {
    selectTuning(e.target.value);
}

function processNote(j) {
    if (j.frequency <= 0) {
        tuneStringIdx = -1;
        tuneStart = null;
        return;
    }

    lastDetectedTime = Date.now();
    document.getElementById("note").textContent = j.note;

    let tuneMessage = j.cents < -10 ? "Tune up" : j.cents > 10 ? "Tune down" : "In tune";
    document.getElementById("cents").textContent = j.cents.toFixed(1) + " cents - " + tuneMessage;

    const clamped = Math.max(-50, Math.min(50, j.cents));
    document.getElementById("needle").style.transform = `rotate(${clamped}deg)`;

    const matchedIdx = findBestMatchingString(j.note, j.frequency);
    const inTune = Math.abs(j.cents) <= 10;
    const tuning = tunings[currentTuning];

    tuning.forEach((_, idx) => {
        const el = document.getElementById('string-' + idx);
        if (!el) return;

        if (idx === matchedIdx) {
            if (inTune) {
                if (tuneStringIdx !== idx) {
                    tuneStringIdx = idx;
                    tuneStart = Date.now();
                }
                if (Date.now() - tuneStart >= 1000) {
                    if (!tunedStrings.has(idx)) {
                        tunedStrings.add(idx);
                        playDing();
                    }
                    el.className = 'string-indicator tuned';
                }
            } else {
                tuneStringIdx = -1;
                tuneStart = null;
                tunedStrings.delete(idx);
                if (!el.classList.contains('tuned')) {
                    el.className = 'string-indicator out-of-tune';
                }
            }
        } else {
            if (!el.classList.contains('tuned')) {
                el.className = 'string-indicator';
            }
        }
    });
}

// Reset UI to idle after SILENCE_TIMEOUT_MS with no incoming events
setInterval(() => {
    if (lastDetectedTime !== null && Date.now() - lastDetectedTime > SILENCE_TIMEOUT_MS) {
        document.getElementById("note").textContent = "--";
        document.getElementById("cents").textContent = "0 cents";
        document.getElementById("needle").style.transform = "rotate(0deg)";
        lastDetectedTime = null;
        resetStringIndicators();
    }
}, 500);

if (TEST_MODE) {
    setInterval(() => processNote({frequency: 111.0, note: "A", cents: 2.5}), 200);
} else {
    const noteStream = new EventSource('/api/note/events');
    noteStream.onmessage = (e) => processNote(JSON.parse(e.data));
}

$(document).on('click', function(e) {
    if (!$(e.target).closest('.dropdown').length) {
        $('.dropdown-menu').removeClass('show');
    }
});

$(document).ready(function() {
    fetch('/api/mode', { method: 'POST', body: 'note' }).catch(() => {});
    renderTuningDropdown();
    document.getElementById('tuning-dropdown').addEventListener('change', handleTuningDropdownChange);
});
