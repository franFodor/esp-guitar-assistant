<h1 align="center">Diplomski projekt</h1>

Note: this is a project I did during my masters at college so this is written in a report style (and in croatian) as opposed to the standard Git README file

## Zadatak
S pomoću ESP32 mikrokontrolera i INMP441 mikrofona napraviti štimer za gitaru. Zadatak se može podijeliti u više koraka i svaki će biti analiziran i diskutiran u nastavku. Kod je dostupan [ovdje](https://github.com/franFodor/dipl).

Štimer služi za ugađanje gitare na potrebne note (frekvencije). S vremenom (recimo zbog sviranja, transporta i sl.) događa se da se žice prirodno otpuštaju te je potrebno redovno provjeravati "stanje" žica. Standardno ugađanje žica (engl. tuning - koristit će se u nastavku) bi bilo, od najniže žice, E-A-D-G-B-E. U današnjoj glazbi postoje i razna druga ugađanja, recimo jedno popularno bi bilo tzv. "drop D" gdje je zadnja žica spusti za jednu notu: D-A-D-G-B-E. 
Ideja rada je da se pomoću mikrofona može očitati frekvencija sviranog tona (može biti bilo kakav, ne nužno od gitare) i pomoću njega naštima gitara budući da svaka nota odgovara nekoj frekvenciji.
### 1. Shema
Prvo je potrebno spojiti komponente. Spoj je jednostavan budući da se radi o samo dvije komponente i prikazan je na shemi ispod. Korišten je [ESP-WROVER-E](https://soldered.com/productdata/2022/03/Soldered_ESP32-WROVER_datasheet.pdf) i [INMP441 mikrofon](https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf).
```
  ESP32               INMP441
+-------+            +-------+
|     25|------------|WS     |
|     26|------------|SCK    |
|       |            |       |
|       |            |       |
|     33|------------|SD     |
|       |            |       |
|       |        +---|LR     |
|       |        |   |       |
|    GND|------------|GND    |
|    3v3|------------|VDD    |
+-------+            +-------+
```
### 2. Čitanje mikrofona
INMP441 mikrofon koristi I2S protokol za komunikaciju. Kao što je prikazano na shemi, mikrofon je spojen na pinove 25, 26 i 33. Prvo je potrebno inicijalizirati mikrofon za što služi funkcija `setup_i2s()` koja postavlja default vrijednosti za većinu parametara, definira pinove i uključuje I2S kanal.

Glavni program podijeljen je u dva zadatka. Prvi zadatak služi za čitanje s mikrofona, a drugi za procesiranje pročitanog o kojem će kasnije biti riječ. Za komunikaciju između zadataka koristi se red (`audio_data_queue`). I2S zadatak puni red, a zadatak za procesiranje čeka da se napuni i obrađuje ga. Čitanje se jednostavno radi uz pomoć funkcije `i2s_channel_read(rx_handle, desc_buf, bytes_to_read, bytes_read, ticks_to_wait)`.
### 3. Obrada signala
Nakon čitanja slijedi obrada pročitanog. Postoje dvije mogućnosti za obradu. Prva je da se koristi neki od algoritama za detekciju visine tona (engl. pitch). Ovaj pristup nije baš najbolji budući da se ograničujemo samo na ton, dok za neke kompleksnije stvari (poput detekcije više nota istovremeno) imamo problem. U nastavku je (ukratko) analiziran jedan od algoritama, Yin. Više o algoritmu može se saznati [ovdje](http://audition.ens.fr/adc/pdf/2002_JASA_YIN.pdf). Implementacija algoritma preuzeta je [odavde](https://github.com/yt752/Yin-Pitch-Tracking), a kod za ESP može se naći na `esp` grani pod `esp_freq`. Kasnije će biti dana usporedba s ostalim metodama.

Druga opcija je nešto sofisticiranija. Naime, ideja je da se očitani zvuk, s pomoću brze furijerove transformacije, prebaci u frekvencijski spektar i tamo možemo odrediti ("najjaču") frekvenciju koja je prisutna u zvučnom signalu. Za to imamo dvije opcije, koristiti sklopovlje dostupno na ESP-u u obliku DSP koprocesora ili koristiti programsku implementaciju. Za sklopovlje se koristi [ESP-DSP library](https://docs.espressif.com/projects/esp-dsp/en/latest/esp32/index.html), dok se za programsku implementaciju koristi [Kiss FFT](https://github.com/mborgerding/kissfft). Odabir metoda može se napraviti odabirom željene implementacije u `CMakeLists.txt` datoteci (`DSP` za sklopovsku, `SOFTWARE` za programsku). U nastavku je dana usporedba dviju metoda. Obe metode koriste veličinu FFT međuspremnika 4096 s frekvencijom uzorkovanja 16000 Hz. Za mjerenje korištene su funkcije `esp_cpu_get_cycle_count()` i `esp_timer_get_time()`.
```
DSP prosjek 10 mjerenja:
I (5702) FFT_METRICS: Time: 3378 us | Cycles: 554295 | Cycles/Sample: 135.33

SOFTWARE prosjek 10 mjerenja:
I (5711) FFT_METRICS: Time: 7774 us | Cycles: 1243685 | Cycles/Sample: 303.63

Yin algoritam prosjek 10 mjerenja
I (12193) YIN: Time: 947399 us | Cycles: 151583842 | Cycles/Sample: 37007.77

```

Iz rezultata vidimo da je očekivano DSP brži (duplo) tako da će se u nastavku rada koristiti sklopovska implementacija. Dodatno, vidimo da je Yin algoritam užasan što se performansi tiče, no to je za očekivati budući da se radi o "naivnoj" implementaciji istog (npr. koristi se dvostruka for petlja). No, i da se radi o boljoj implementaciji i dalje bi bio znatno sporiji pa se iz tog razloga neće uzimati u daljnje razmatranje. Performanse programske implementacije mogle bi se poboljšati sa nekom drugom FFT bibliotekom ali teško da će se doseći performanse sklopovske implementacije.
#### Obrada FFT-a
Kada smo proveli furijerovu transformaciju posao nije gotov. Sada imamo samo frekvencijski spektar zvučnog signala, potrebno je odrediti još i frekvenciju koju mjerimo. Primjer spektra može se vidjeti na slici ispod:
![[img5.png]]
Ako pokušamo naivno uzeti "najjaču" frekvenciju (ona koja ima najveću energiju) najvjerojatnije ćemo dobiti krivi rezultat jer frekvencije koje nas zanimaju pri štimanju gitare (a to su niske frekvencije od 82 Hz pa nadalje) imaju malu energiju pa ćemo očitati neki njihov harmonik. Problem možemo riješiti koristeći neku od metoda za pronalazak fundamentalne frekvencije, npr. korištena je metoda **Harmonic Product Spectrum (HPS)**, više o njoj, i drugim, se može saznati [ovdje](http://musicweb.ucsd.edu/~trsmyth/analysis/analysis.pdf). Za to služi funkcija `harmonic_product_spectrum(magnitudes, half_size, hps)` koja se zove iz funkcije `guitar_frequency_analysis(magnitudes, hps)`. U funkciji za HPS je implementiran algoritam koji je dan na prošlom linku. Dodatno, za preciznije određivanje frekvencije koristi se i kvadratna interpolacija, algoritam je dan u istom linku a implementiran u funkciji `quadratic_interpolation(magnitudes, peak_bin)`. Izmjerena su i performansa te funkcije.

```
guitar_frequency_analysis prosjek 10 mjerenja
I (6730) HARMONIC: Avg Time: 10606 us | Avg Cycles: 1696912 | Avg Cycles/Sample: 414.29
```
Ovim mjerenjem dolazimo do zanimljivog ograničenja broja obrada u sekundi. Ako se koristi DSP implementacija čije trajanje iznosi 3378 us i trajanje obrade uzorka 10606 us imamo ukupno 13.984 ms vrijeme trajanja obrade. Uzmemo li nekakvu prosječnu brzinu sviranja od 120 bpm (beats per minute, otkucaj po minuti, inače se može naći u rasponu od 60 pa do 220+) i sviranje tzv. "osminki" (dva trzaja gitare po jednom otkucaju) vremenski razmak bi trebao biti 250 ms što je zadovoljavajuće. Ako bismo se odlučili za malo "žešću" varijantu glazbe od recimo 200 bpm sa sviranjem "šesnaestinki" (četiri trzaja po otkucaju) vrijeme između dva udarca bi trebao biti 75 ms. Naš rezultat od 14 ms je zadovoljavajuć s dodatnom mogućnosti snimanja 5 uzorka između trzaja (u ovom ekstremnom slučaju) što može poboljšati točnost prikaza (ako bi uveli dodatnu logiku za to).
### 4. Prikaz rezultata
Nakon što smo dobili frekvenciju potrebno ju je i prikazati korisniku. Postoje tri mogućnosti za to. Prva mogućnost, najjednostavnija, je pomoću serial monitora gdje se pomoću `ESP_LOGI` funkcije ispisuju očitane vrijednosti. Druga mogućnost je pomoću OLED prikaznika. On nije previše razmatran u radu, jednostavan kod može se pronaći na `esp` grani pod `esp_freq_oled`.

Glavni prikaz obavljen je uz pomoć web preglednika. Konkretno, napisana je jednostavna web stranica koja korisniku prikazuje trenutnu frekvenciju uz prikaz potrebnih frekvencija, te uz naputak u koju stranu štimati žicu.
![[Pasted image 20260122182739.png]]
Kod za web stranicu nalazi se u `spiffs` direktoriju, gdje je korišten datotečni sustav dostupan na ESP-u, `spiffs`. Za njega je potrebno definirati particiju (`partitions.csv`), a izradu sučelja korišten je [Bootstrap](https://getbootstrap.com/). Na stranici je također prisutna mogućnost mijenjanja tuninga.
![[Pasted image 20260123091504.png]]
Kod za server se nalazi u `server.c` i ima samo jedan api koji vraća trenutno očitanje frekvencije. Isti se periodički zove iz servera kako bi se ažurirao prikaz na ekranu.
### 5. Daljnji rad
- poboljšanje očitanja
	- trenutno za štimanje nije problem ali ako bi pokušali svirati neku melodiju možda bi bio problem lošeg/dugotrajnog očitanja ispravne note
- optimizacija funkcija za obradu zvuka
	- optimizacija određenih dijelova
	- potencijalno izbacivanje kvadratne interpolacije
- detekcija akorda (više nota istovremeno)
	- pomoću FFT
- sviranje skala
- sviranje melodija
