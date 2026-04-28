const A4 = 440;

const noteToMidi = {
    "E2": 40, "A2": 45, "D3": 50, "G3": 55, "B3": 59, "E4": 64,
    "D2": 38, "G2": 43, "C3": 48, "F3": 53, "A3": 57
};

const tunings = {
    "Standard": ["E2", "A2", "D3", "G3", "B3", "E4"],
    "D Standard": ["D2", "G2", "C3", "F3", "A3", "D3"],
    "Drop D": ["D2", "A2", "D3", "G3", "B3", "E4"]
};

let currentTuning = "Standard";
let lastDetectedTime = null;
const SILENCE_TIMEOUT_MS = 5000;

let tuneStringIdx = -1;
let tuneStart = null;
let tunedStrings = new Set();
let audioCtx = null;

function playDing() {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const osc  = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.type = 'sine';
    osc.frequency.value = 880;
    const t = audioCtx.currentTime;
    gain.gain.setValueAtTime(0, t);
    gain.gain.linearRampToValueAtTime(0.4, t + 0.01);
    gain.gain.exponentialRampToValueAtTime(0.001, t + 0.35);
    osc.start(t);
    osc.stop(t + 0.35);
}

const TEST_MODE = true; // Set to false to use real ESP data

const tuningNames = Object.keys(tunings);
let selectedTuning = tuningNames[0];

function noteToFrequency(noteName) {
    const midiNote = noteToMidi[noteName];
    return A4 * Math.pow(2, (midiNote - 69) / 12);
}

function findBestMatchingString(detectedNote, detectedFreq) {
    const tuning = tunings[currentTuning];
    let bestIdx = -1;
    let bestDiff = Infinity;
    tuning.forEach((stringNote, idx) => {
        if (stringNote.replace(/\d+$/, '') === detectedNote) {
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
    const tuning = tunings[currentTuning];

    if (currentTuning === 'Custom') {
        document.getElementById('tuning-display').innerHTML = '<div class="text-center">TODO</div>';
        return;
    }

    const html = '<div class="text-center">' +
        tuning.map((note, idx) =>
            '<span class="string-indicator" id="string-' + idx + '">' + note + '</span>'
        ).join(' | ') +
        '</div>';
    document.getElementById('tuning-display').innerHTML = html;
    tuneStringIdx = -1;
    tuneStart = null;
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

async function update() {
    let j;
    if (TEST_MODE) {
        j = {frequency: 111.0, note: "A", cents: 2.5};
    } else {
        const r = await fetch("/api/note");
        j = await r.json();
    }

    if (j.frequency <= 0) {
        if (lastDetectedTime !== null && Date.now() - lastDetectedTime > SILENCE_TIMEOUT_MS) {
            document.getElementById("note").textContent = "--";
            document.getElementById("cents").textContent = "0 cents";
            document.getElementById("needle").style.transform = "rotate(0deg)";
            lastDetectedTime = null;
            resetStringIndicators();
        }
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

    // String indicator highlighting
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

setInterval(update, 50);

$(document).on('click', function(e) {
    if (!$(e.target).closest('.dropdown').length) {
        $('.dropdown-menu').removeClass('show');
    }
});

$(document).ready(function() {
    fetch('/api/mode', { method: 'POST', body: 'note' }).catch(() => {});
    renderTuningDropdown();
    document.getElementById('tuning-dropdown').addEventListener('change', handleTuningDropdownChange);
    update();
});
