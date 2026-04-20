const TEST_MODE = false; // Set to false to use real ESP data
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
    
    console.log('updateChordDisplay called:', {
        chord: chord,
        selectedChordName: selectedChordName,
        detectedChordName: detectedChordName
    });
    
    if (chord && chord !== "None") {
        chordNameEl.textContent = chord;
        
        // Store detected chord info
        detectedChordName = chord;
        detectedNotes = notes || [];
        
        // Show detected notes only when listening
        if (isListening && detectedNotesEl && notes && notes.length > 0) {
            detectedNotesEl.innerHTML = '<strong>Detected Notes:</strong> ' + notes.join(', ');
            detectedNotesEl.style.display = 'block';
        } else {
            detectedNotesEl.style.display = 'none';
        }
    } else {
        chordNameEl.textContent = "--";
        detectedChordName = "None";
        detectedNotes = [];
        if (detectedNotesEl) {
            detectedNotesEl.style.display = 'none';
        }
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
    
    // Highlight detected notes at fret positions 1-3 only (not including 4th fret)
    let cellIndex = 0;
    for (let string = 5; string >= 0; string--) {
        for (let fret = 0; fret < 12; fret++) {
            const displayFret = fret + 1;
            
            // Only highlight up to 3rd fret (not including 4th)
            if (displayFret >= 4) {
                cellIndex++;
                continue;
            }
            
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

function highlightChordNotes(notes, isCorrect) {
    const fretboard = document.getElementById('fretboard');
    const cells = fretboard.getElementsByClassName('fret-cell');
    const xoCells = fretboard.getElementsByClassName('fret-xo');

    // Remove all existing note circles and reset fret circles
    const existingNoteCircles = fretboard.querySelectorAll('.note-circle');
    existingNoteCircles.forEach(el => el.remove());
    
    const existingFretCircles = fretboard.querySelectorAll('.fret-circle');
    existingFretCircles.forEach(el => {
        el.classList.remove('fret-circle-correct', 'fret-circle-incorrect');
    });

    if (!notes || notes.length === 0) {
        return;
    }

    // Get the selected chord positions (what user should play)
    const chordData = CHORDS[selectedChordName];
    if (!chordData || !chordData.positions) {
        return;
    }
    
    const positions = chordData.positions;
    
    // Highlight the selected chord positions based on whether detected matches
    for (let string = 5; string >= 0; string--) {
        const pos = positions[string][0];
        const finger = positions[string][1];
        
        if (pos === null) {
            // Muted string (X) - don't highlight
            continue;
        } else if (pos === 0) {
            // Open string (O) - highlight the O circle
            const xoCellIndex = 5 - string;
            if (xoCells[xoCellIndex]) {
                const oCircle = xoCells[xoCellIndex].querySelector('.fret-o');
                if (oCircle) {
                    if (isCorrect) {
                        oCircle.classList.add('fret-circle-correct');
                    } else {
                        oCircle.classList.add('fret-circle-incorrect');
                    }
                }
            }
        } else {
            // Fretted note - highlight the fret circle
            const fretIndex = pos - 1;
            const cellIndex = (5 - string) * 12 + fretIndex;
            
            if (cellIndex >= 0 && cellIndex < cells.length) {
                const cell = cells[cellIndex];
                const existingFretCircle = cell.querySelector('.fret-circle');
                
                if (existingFretCircle) {
                    if (isCorrect) {
                        existingFretCircle.classList.add('fret-circle-correct');
                    } else {
                        existingFretCircle.classList.add('fret-circle-incorrect');
                    }
                } else {
                    const noteAtPosition = getNoteAtFret(string, pos);
                    const circle = document.createElement('div');
                    circle.className = 'note-circle';
                    circle.textContent = noteAtPosition;
                    if (isCorrect) {
                        circle.classList.add('note-circle-correct');
                    } else {
                        circle.classList.add('note-circle-incorrect');
                    }
                    cell.appendChild(circle);
                }
            }
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
            updateChordDisplay(data.chord, data.notes);
            
            // Re-render fretboard with selected chord positions
            if (CHORDS[selectedChordName]) {
                renderFretboard(CHORDS[selectedChordName].positions);
            }
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

function handleDropdownChange(e) {
    selectedChordName = e.target.value;
    console.log('Dropdown changed, selectedChordName:', selectedChordName);
    
    renderFretboard(CHORDS[selectedChordName].positions);

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
    console.log('updateChordColor - Comparison:', {
        detectedChordName: detectedChordName,
        selectedChordName: selectedChordName,
        isCorrect: isCorrect
    });
    
    chordNameEl.classList.remove('detected-correct', 'detected-incorrect');
    if (isCorrect) {
        chordNameEl.classList.add('detected-correct');
        console.log('Adding detected-correct class');
    } else {
        chordNameEl.classList.add('detected-incorrect');
        console.log('Adding detected-incorrect class');
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
        for (let fret = 0; fret < 12; fret++) {
            const cell = document.createElement('div');
            cell.className = 'fret-cell';

            if (string === 0) {
                cell.classList.add('last-row');
            }

            if (string === 0 && (fret === 0 || fret == 2 || fret == 4 || fret == 6 || fret == 8 || fret === 11)) {
                cell.innerHTML = `<span class='fret-label'>${fret+1}</span>`;
            }

            if (chordPositions) {
                const pos = chordPositions[string][0];
                const finger = chordPositions[string][1];
                if (pos !== null && pos > 0 && pos-1 === fret) {
                    const circle = document.createElement('div');
                    circle.className = 'fret-circle';
                    if (finger) circle.textContent = finger;
                    cell.appendChild(circle);
                }
            }

            fretboard.appendChild(cell);
        }
        }
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
    setInterval(fetchChord, 200);

    // Initialize dropdown and fretboard
    renderDropdown();
    renderFretboard(CHORDS[selectedChordName].positions);
    
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
                listenBtn.textContent = 'Stop';
                // Update detected notes display when starting to listen
                const detectedNotesEl = document.getElementById('detected-notes-display');
                const chordNameEl = document.getElementById('chord-name');
                if (detectedNotesEl && detectedNotes.length > 0) {
                    detectedNotesEl.innerHTML = '<strong>Detected Notes:</strong> ' + detectedNotes.join(', ');
                    detectedNotesEl.style.display = 'block';
                }
                if (chordNameEl && detectedChordName !== "None") {
                    chordNameEl.textContent = detectedChordName;
                    chordNameEl.classList.add('detected');
                }
            } else {
                listenBtn.classList.remove('listening');
                listenBtn.textContent = 'Listen';
                // Reset detected chord when stopping
                detectedChordName = "None";
                detectedNotes = [];
                document.getElementById('chord-name').textContent = "--";
                document.getElementById('detected-notes-display').style.display = 'none';
            }
            highlightFretboard();
        });
    }
});
