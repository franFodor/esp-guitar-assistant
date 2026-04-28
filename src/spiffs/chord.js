const TEST_MODE = true; // Set to false to use real ESP data
let selectedChordName = "A major"; // User's selected chord to practice
let detectedChordName = "None"; // Chord detected by API
let detectedNotes = [];
let isListening = false; // Whether user has pressed Listen button

// Note positions on fretboard for standard tuning
function getNoteAtFret(string, fret) {
    const openNotes = ["E", "A", "D", "G", "B", "E"];
    const noteOrder = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
    
    let baseIndex = noteOrder.indexOf(openNotes[string]);
    if (baseIndex === -1) baseIndex = 0;
    
    let noteIndex = (baseIndex + fret) % 12;
    return noteOrder[noteIndex];
}

function updateChordDisplay(chord, notes) {
    const chordNameEl = document.getElementById('chord-name');
    const targetNotesEl = document.getElementById('target-notes-display');
    const detectedNotesEl = document.getElementById('detected-notes-display');
    
    if (chord && chord !== "None") {
        chordNameEl.textContent = chord;
        
        // Store detected chord info
        detectedChordName = chord;
        detectedNotes = notes || [];
        
        if (isListening && detectedNotesEl && notes && notes.length > 0) {
            detectedNotesEl.innerHTML = '<strong>Detected Notes:</strong> ' + notes.join(', ');
        } else if (detectedNotesEl) {
            detectedNotesEl.textContent = '-';
        }
    } else {
        chordNameEl.textContent = "--";
        detectedChordName = "None";
        detectedNotes = [];
        if (detectedNotesEl) detectedNotesEl.textContent = '-';
    }
    
    // Update chord color based on comparison
    updateChordColor();
    
    highlightFretboard();
}

function highlightFretboard() {
    const chordData = CHORDS[selectedChordName];
    if (!chordData || !chordData.positions) {
        return;
    }
    
    // Always render the selected chord positions in blue (target notes)
    highlightSelectedChord(chordData.positions);
    
    // Only show detected notes if listening is active
    if (isListening && detectedNotes.length > 0) {
        const isCorrect = (detectedChordName === selectedChordName && detectedChordName !== "None");
        highlightDetectedNotes(detectedNotes, isCorrect);
    }
}

function highlightSelectedChord(positions) {
    const fretboard = document.getElementById('fretboard');
    const cells = fretboard.getElementsByClassName('fret-cell');
    const xoCells = fretboard.getElementsByClassName('fret-xo');
    
    // Highlight the selected chord positions in blue (target)
    for (let string = 5; string >= 0; string--) {
        const pos = positions[string][0];
        const finger = positions[string][1];
        
        if (pos === null) {
            continue;
        } else if (pos === 0) {
            // Open string - style the O circle
            const xoCellIndex = 5 - string;
            if (xoCells[xoCellIndex]) {
                const oCircle = xoCells[xoCellIndex].querySelector('.fret-o');
                if (oCircle) {
                    oCircle.classList.add('fret-circle-target');
                }
            }
        } else {
            // Fretted note
            const fretIndex = pos - 1;
            const cellIndex = (5 - string) * 12 + fretIndex;
            
            if (cellIndex >= 0 && cellIndex < cells.length) {
                const cell = cells[cellIndex];
                const existingFretCircle = cell.querySelector('.fret-circle');
                
                if (existingFretCircle) {
                    existingFretCircle.classList.add('fret-circle-target');
                }
            }
        }
    }
}

function highlightDetectedNotes(notes, isCorrect) {
    const fretboard = document.getElementById('fretboard');
    const cells = fretboard.getElementsByClassName('fret-cell');
    const xoCells = fretboard.getElementsByClassName('fret-xo');
    const openNotes = ["E", "A", "D", "G", "B", "E"];
    
    // Get target chord notes
    const targetChordData = CHORDS[selectedChordName];
    const targetNotes = targetChordData ? targetChordData.notes : [];
    
    // Highlight detected notes at open strings
    for (let string = 5; string >= 0; string--) {
        const openNote = openNotes[string];
        if (notes.includes(openNote)) {
            const xoCellIndex = 5 - string;
            if (xoCells[xoCellIndex]) {
                const oCircle = xoCells[xoCellIndex].querySelector('.fret-o');
                if (oCircle) {
                    // Check if this detected note is in target chord
                    if (targetNotes.includes(openNote)) {
                        oCircle.classList.add('fret-circle-correct');
                    } else {
                        oCircle.classList.add('fret-circle-incorrect');
                    }
                }
            }
        }
    }
    
    // Highlight detected notes across all 5 visible frets
    let cellIndex = 0;
    for (let string = 5; string >= 0; string--) {
        for (let fret = 0; fret < 5; fret++) {
            const displayFret = fret + 1;
            const noteAtPosition = getNoteAtFret(string, displayFret);
            
            if (notes.includes(noteAtPosition)) {
                const cell = cells[cellIndex];
                const existingFretCircle = cell.querySelector('.fret-circle');
                
                // Check if this detected note is in target chord
                const isTargetNote = targetNotes.includes(noteAtPosition);
                
                if (existingFretCircle) {
                    // Color the finger circle based on whether note is in target
                    if (isTargetNote) {
                        existingFretCircle.classList.add('fret-circle-correct');
                    } else {
                        existingFretCircle.classList.add('fret-circle-incorrect');
                    }
                } else {
                    // Add a note circle for detected notes
                    const circle = document.createElement('div');
                    circle.className = 'note-circle';
                    circle.textContent = noteAtPosition;
                    if (isTargetNote) {
                        circle.classList.add('note-circle-correct');
                    } else {
                        circle.classList.add('note-circle-incorrect');
                    }
                    cell.appendChild(circle);
                }
            }
            cellIndex++;
        }
    }
}

async function fetchChord() {
    try {
        let data;
        if (TEST_MODE) {
            // In test mode, always detect A major regardless of selection
            // This lets you test: select A major = green, select anything else = red
            data = { chord: "A major", notes: ["A", "C#", "E"] };
        } else {
            const response = await fetch('/api/chord');
            data = await response.json();
        }
        if (data.chord !== detectedChordName || JSON.stringify(data.notes) !== JSON.stringify(detectedNotes)) {
            // Render base fretboard first, then apply highlights on top
            if (CHORDS[selectedChordName]) {
                renderFretboard(CHORDS[selectedChordName].positions);
            }
            updateChordDisplay(data.chord, data.notes);
        }
    } catch (error) {
        console.error('Error fetching chord:', error);
    }
}

// Chord dropdown and fretboard rendering
const chordNames = Object.keys(CHORDS);

function renderDropdown() {
    const dropdown = document.getElementById('chord-dropdown');
    dropdown.innerHTML = '';
    chordNames.forEach(name => {
        const option = document.createElement('option');
        option.value = name;
        option.textContent = name;
        dropdown.appendChild(option);
    });
    dropdown.value = selectedChordName;
}

function updateChordImage(chordName) {
    const img = document.getElementById('chord-img');
    const placeholder = document.getElementById('chord-img-placeholder');
    const nameEl = document.getElementById('chord-img-name');
    if (nameEl) nameEl.textContent = chordName;
    if (!img) return;
    const normalized = chordName.replace(/ /g, '_').replace(/#/g, 'sharp');
    img.src = `chord_images/${normalized}.png`;
    img.onload = () => { img.style.display = 'block'; if (placeholder) placeholder.style.display = 'none'; };
    img.onerror = () => { img.style.display = 'none'; if (placeholder) placeholder.style.display = 'flex'; };
}

function handleDropdownChange(e) {
    selectedChordName = e.target.value;
    renderFretboard(CHORDS[selectedChordName].positions);
    updateChordImage(selectedChordName);

    // Update target notes display
    const targetNotesEl = document.getElementById('target-notes-display');
    const chordData = CHORDS[selectedChordName];
    if (targetNotesEl && chordData && chordData.notes) {
        targetNotesEl.innerHTML = '<strong>Target Notes:</strong> ' + chordData.notes.join(', ');
    }

    // Re-evaluate chord color based on new selection
    updateChordColor();

    highlightFretboard();
}

function updateChordColor() {
    const chordNameEl = document.getElementById('chord-name');
    if (!chordNameEl || detectedChordName === "None") {
        return;
    }
    
    const isCorrect = (detectedChordName === selectedChordName);
    chordNameEl.classList.remove('detected-correct', 'detected-incorrect');
    if (isCorrect) {
        chordNameEl.classList.add('detected-correct');
    } else {
        chordNameEl.classList.add('detected-incorrect');
    }
}

function renderFretboard(chordPositions = null) {
    const fretboard = document.getElementById('fretboard');
    fretboard.innerHTML = '';
    for (let string = 5; string >= 0; string--) {
        // Render X/O marker as first cell in grid row
        const xoCell = document.createElement('div');
        xoCell.className = 'fret-xo';
        let mark = null;
        let isMuted = false;
        let isOpen = false;
        if (chordPositions) {
            if (chordPositions[string][0] === null) {
                mark = '×';
                isMuted = true;
            } else if (chordPositions[string][0] === 0) {
                mark = 'O';
                isOpen = true;
            }
        }
        
        // Create circle for X/O markers
        if (mark) {
            const circle = document.createElement('div');
            circle.className = 'fret-circle fret-xo-circle';
            circle.textContent = mark;
            circle.dataset.string = string;
            circle.dataset.fret = isMuted ? -1 : 0;
            if (isMuted) {
                circle.classList.add('fret-x');
            } else if (isOpen) {
                circle.classList.add('fret-o');
            }
            xoCell.appendChild(circle);
        }
        
        fretboard.appendChild(xoCell);
        for (let fret = 0; fret < 5; fret++) {
            const cell = document.createElement('div');
            cell.className = 'fret-cell';
            cell.style.width = '100%';

            if (string === 0) cell.classList.add('last-row');

            if (string === 0 && (fret === 2 || fret === 4)) {
                cell.innerHTML = `<span class='fret-label'>${fret + 1}</span>`;
            }

            if (chordPositions) {
                const pos    = chordPositions[string][0];
                const finger = chordPositions[string][1];
                if (pos !== null && pos > 0 && pos - 1 === fret) {
                    const circle = document.createElement('div');
                    circle.className = 'fret-circle';
                    if (finger) circle.textContent = finger;
                    cell.appendChild(circle);
                }
            }

            fretboard.appendChild(cell);
        }
    }

    fretboard.style.gridTemplateColumns = `32px repeat(5, 72px)`;
    fretboard.style.minWidth = (32 + 5 * 72) + 'px';
}


$(document).ready(function() {
    fetch('/api/mode', { method: 'POST', body: 'chord' }).catch(() => {});

    // Load navigation
    fetch('nav.html')
        .then(response => response.text())
        .then(data => {
            $('#navbar').html(data);
            $('#nav-chord').addClass('active').append('<span class="sr-only">(current)</span>');
        });

    // Start polling for chord data (includes notes)
    setInterval(fetchChord, 50);

    // Initialize dropdown and fretboard
    renderDropdown();
    renderFretboard(CHORDS[selectedChordName].positions);
    updateChordImage(selectedChordName);
    
    // Show target notes for initial chord
    const targetNotesEl = document.getElementById('target-notes-display');
    const initialChordData = CHORDS[selectedChordName];
    if (targetNotesEl && initialChordData && initialChordData.notes) {
        targetNotesEl.innerHTML = '<strong>Target Notes:</strong> ' + initialChordData.notes.join(', ');
    }
    
    highlightFretboard();

    document.getElementById('chord-dropdown').addEventListener('change', handleDropdownChange);
    
    // Listen button toggle
    const listenBtn = document.getElementById('listen-btn');
    if (listenBtn) {
        listenBtn.addEventListener('click', function() {
            isListening = !isListening;
            if (isListening) {
                listenBtn.classList.add('listening');
                listenBtn.textContent = 'Stop Listening';
                detectedChordName = "None"; // force next poll to re-render with isListening=true
            } else {
                listenBtn.classList.remove('listening');
                listenBtn.textContent = 'Start Listening';
                detectedChordName = "None";
                detectedNotes = [];
                document.getElementById('chord-name').textContent = "--";
                document.getElementById('detected-notes-display').textContent = '-';
            }
            highlightFretboard();
        });
    }
});
