const A4 = 440;

const noteTable = [
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
];

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

const TEST_MODE = true; // Set to false to use real ESP data

const tuningNames = Object.keys(tunings);
let selectedTuning = tuningNames[0];
function noteToFrequency(noteName) {
    const midiNote = noteToMidi[noteName];
    return A4 * Math.pow(2, (midiNote - 69) / 12);
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
    
    // Display target frequencies for each string
    let freqHtml = '<div class="text-center">';
    tuning.forEach((note, index) => {
        const freq = noteToFrequency(note);
        freqHtml += '<strong>' + note + '</strong>: ' + freq.toFixed(2) + ' Hz';
        if (index < tuning.length - 1) freqHtml += ' | ';
    });
    freqHtml += '</div>';
    document.getElementById('tuning-display').innerHTML = freqHtml;
}

function renderTuningDropdown() {
    const dropdown = document.getElementById('tuning-dropdown');
    dropdown.innerHTML = '';
    // Restore original options without separators
    const options = ["Standard", "D Standard", "Drop D", "Custom"];
    options.forEach(name => {
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
    updateTuningDisplay();
}
async function update() {
    let j;
    if (TEST_MODE) {
        j = {frequency: 111.0, note: "A", cents: 2.5}; // Test data
    } else {
        const r = await fetch("/api/note");
        j = await r.json();
    }

    if (j.frequency <= 0) return;

    document.getElementById("note").textContent = j.note;
    document.getElementById("freq").textContent =
        j.frequency.toFixed(2) + " Hz";
    
    let tuneMessage = "";
    if (j.cents < -10) {
        tuneMessage = "Tune up";
    } else if (j.cents > 10) {
        tuneMessage = "Tune down";
    } else {
        tuneMessage = "In tune";
    }
    
    document.getElementById("cents").textContent =
        j.cents.toFixed(1) + " cents - " + tuneMessage;

    // clamp needle to ±50 cents
    const clamped = Math.max(-50, Math.min(50, j.cents));
    const angle = clamped * 1.0; // ±50°
    document.getElementById("needle").style.transform =
        `rotate(${angle}deg)`;
}

setInterval(update, 50);
// Initialization now handled in $(document).ready())

// Close dropdown when clicking outside
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
