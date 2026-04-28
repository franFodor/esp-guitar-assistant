const BPM_MIN = 40;
const BPM_MAX = 240;

const TIME_SIGS = {
    '2/4': { beats: 2, accents: [0] },
    '3/4': { beats: 3, accents: [0] },
    '4/4': { beats: 4, accents: [0] },
    '6/8': { beats: 6, accents: [0, 3] },
};

let bpm        = 120;
let timeSig    = '4/4';
let isPlaying  = false;
let beatIndex  = 0;       // next beat to schedule
let nextBeatTime = 0;     // AudioContext time of that beat
let schedulerTimer = null;
let audioCtx   = null;
// Schedule beats this far ahead so the audio clock handles timing precisely.
// JS setInterval alone drifts noticeably at high BPMs.
const LOOKAHEAD         = 0.1;  // seconds
const SCHEDULE_INTERVAL = 25;   // ms

// { beat, time } pairs waiting to drive the visual beat indicator
const noteQueue = [];

function scheduleClick(beat, time) {
    const osc  = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.connect(gain);
    gain.connect(audioCtx.destination);

    const accent = TIME_SIGS[timeSig].accents.includes(beat);
    osc.frequency.value = accent ? 1000 : 700;
    const vol = accent ? 0.7 : 0.4;

    gain.gain.setValueAtTime(vol, time);
    gain.gain.exponentialRampToValueAtTime(0.001, time + 0.08);
    osc.start(time);
    osc.stop(time + 0.08);

    noteQueue.push({ beat, time });
}

function scheduler() {
    const beats = TIME_SIGS[timeSig].beats;
    while (nextBeatTime < audioCtx.currentTime + LOOKAHEAD) {
        scheduleClick(beatIndex, nextBeatTime);
        nextBeatTime += 60.0 / bpm;
        beatIndex = (beatIndex + 1) % beats;
    }
}

function drawVisual() {
    if (!isPlaying) return;
    requestAnimationFrame(drawVisual);

    const now = audioCtx.currentTime;
    while (noteQueue.length && noteQueue[0].time <= now + 0.02) {
        updateBeatDots(noteQueue.shift().beat);
    }
}

function updateBeatDots(beat) {
    document.querySelectorAll('.beat-dot').forEach((dot, i) => {
        dot.classList.toggle('active', i === beat);
    });
}

function renderBeatDots() {
    const container = document.getElementById('beat-dots');
    container.innerHTML = '';
    const sig = TIME_SIGS[timeSig];
    for (let i = 0; i < sig.beats; i++) {
        const dot = document.createElement('div');
        dot.className = 'beat-dot' + (sig.accents.includes(i) ? ' accent' : '');
        container.appendChild(dot);
    }
}

function start() {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();

    isPlaying    = true;
    beatIndex    = 0;
    nextBeatTime = audioCtx.currentTime + 0.05;
    noteQueue.length = 0;

    schedulerTimer = setInterval(scheduler, SCHEDULE_INTERVAL);
    requestAnimationFrame(drawVisual);

    const btn = document.getElementById('start-btn');
    btn.textContent = 'Stop';
    btn.classList.add('listening');
}

function stop() {
    isPlaying = false;
    clearInterval(schedulerTimer);
    schedulerTimer = null;
    noteQueue.length = 0;

    document.querySelectorAll('.beat-dot').forEach(d => d.classList.remove('active'));

    const btn = document.getElementById('start-btn');
    btn.textContent = 'Start';
    btn.classList.remove('listening');
}

function setBpm(val) {
    bpm = Math.max(BPM_MIN, Math.min(BPM_MAX, val));
    document.getElementById('bpm-display').textContent = bpm;
    document.getElementById('bpm-slider').value = bpm;
}


$(document).ready(function () {
    fetch('nav.html')
        .then(r => r.text())
        .then(data => {
            $('#navbar').html(data);
            $('#nav-metronome').addClass('active').append('<span class="sr-only">(current)</span>');
        });

    renderBeatDots();

    document.getElementById('start-btn').addEventListener('click', () => {
        if (isPlaying) stop(); else start();
    });

    document.getElementById('bpm-slider').addEventListener('input', function () {
        setBpm(parseInt(this.value));
    });

    document.getElementById('bpm-down').addEventListener('click', () => setBpm(bpm - 1));
    document.getElementById('bpm-up').addEventListener('click', () => setBpm(bpm + 1));

    document.getElementById('timesig-dropdown').addEventListener('change', function () {
        timeSig = this.value;
        const wasPlaying = isPlaying;
        if (wasPlaying) stop();
        renderBeatDots();
        if (wasPlaying) start();
    });
});
