# Analýza opráv — len master (ESP32-C3)

**Dátum:** 2026-08-28  
**Master (mení sa):** `ovladanie-antena-ext-box.ino`, `html_pages.h`, `config.h`  
**Klient (NEMENÍ sa):** `nano/ovladanie-antena-klient-nano.ino` — Arduino Nano na stožiari  
**Priorita:** funkčnosť pred bezpečnosťou (LAN je zabezpečená; bezpečnosť je v samostatnej sekcii, nie na vrchu)

Tento súbor nahrádza predchádzajúci zoznam ako pracovný podklad na opravy. Starý `ANALYZA-BUGY-A-EDGE-CASES.md` ber ako históriu — niekoľko bodov tam bolo zlých (stand-by, bezpečnosť ako P0).

---

## Pravidlá tohto auditu

1. **Klient sa nemení.** Protokol, ID uzla, relé, teploty, SoftwareSerial — zamknuté. Čo z toho vyplýva, je buď obchádzka na masteri, alebo **neopraviteľné**.
2. **Stand-by je zámer, nie chyba.** Má prestať bombardovať stožiar heartbeatom/recovery a prestať pulzovať LED (diskoška). Relé na stožiari majú ostať v poslednom žiadanom stave.
3. **Závažnosť = dopad na ovládanie antény**, nie na PIN/cookie.

---

## Zamknutý kontrakt klienta (Nano)

Master musí hovoriť presne takto, inak Nano ticho zahodí správu a nepošle ACK.

| Vec | Hodnota na Nano | Dôsledok pre master |
|-----|-----------------|---------------------|
| Node ID | `MY_NODE_ID = 1` natvrdo | `SETID` na inú hodnotu = trvalý timeout. Master musí ostávať na **#1**. |
| Príkaz | `#1 CCCCCCCC` + newline (8 znakov po medzere) | Iná dĺžka → `return` bez ACK. Žiadny CRC, žiadny extra field. |
| Stavy | len `A` / `B` / `C` | Iný znak na danej pozícii Nano **preskočí** (nechá starý stav) a aj tak pošle ACK so skutočnými relé. |
| ACK | `#1ACK:CCCCCCCC,T1:24.5,T2:35.2` | Prefix musí byť `#` + id + `ACK:`. Teploty sú vždy, 1 desatinné. Chyba NTC = `999.0`. |
| Relé | 16 pinov, LOW = zopnuté (low-level trigger) | C / A / B mapovanie je na Nano. Master len posiela písmená. |
| Zbernica | SoftwareSerial 9600, D2/D3, auto-smer | Pomalšie a citlivejšie na kolíziu než HW UART. Master nesmie poslať druhý paket, kým čaká na ACK. |

Nano **nikdy** nespustí relé do C pri výpadku linky. Timeout na masteri je len lokálny dohad.

---

## Čo nie je chyba (zámer)

| Téma | Prečo to nechávať |
|------|-------------------|
| Stand-by nevypína relé na stožiari | Má len stíšiť RS485 a LED (utlmenie chybového stavu). Stožiar drží poslednú konfiguráciu. |
| Zmena antény ruší stand-by | `/set`, profil, USB `SEND` prebudia mute a odošlú nový stav. |
| Heartbeat 30 s / recovery 5 s keď stand-by nie je | Nano je hlúpy slave — bez dopytu nedá teploty. |
| Fork PHONE/RTTY len na ESP GPIO | Nano o výhybke nevie. |
| Žiadny CRC na RS485 | Nano by paket s CRC zahodil (`length != 8`). **Pridať CRC sa nedá.** |

---

## Prehľad

| Skupina | Počet | Čo s tým |
|---------|------:|----------|
| F — musí sa opraviť (funkcia, master) | 12 | Bežné ovládanie, stavy, LED, Wi-Fi, pamäť |
| E — edge / treba doladiť | 10 | Timeout vs UI, buffery, validácia pred TX |
| N — neopraviteľné (zamknutý Nano) | 8 | Len obchádzka alebo akceptovať |
| B — bezpečnosť (nižšia priorita) | 6 | Až po funkcii, sieť je privátna |

---

## F — Funkčnosť (opraviť na masteri)

### F-01. L4 (index 7): stav B sa v selecte pokazí

**Súbor:** `html_pages.h`  
**Kód:** `selB4 = ... ? " electromagnetic layer" : ""` namiesto `" selected"`  
**Čo sa stane:** L4 je na stožiari reflektor, web ukáže Off. „Aplikovať“ pošle C a Nano reflektor vypne.  
**Opraviteľné:** áno (master). Nano je v poriadku.

### F-02. Dropdowny berú `currentStates` namiesto `targetStates`

**Súbor:** `html_pages.h` (všetky `selected`)  
**Čo sa stane:** kým nie je ACK, alebo po timeout (master nastaví current na C), formulár ukáže C. Ďalšie odoslanie **prepíše žiadaný stav** a Nano dostane C.  
**Opraviteľné:** áno. Pending nechať na triede `w-pending`, select viazať na `targetStates`.

### F-03. Po timeout master predstiera, že anténa je C

**Súbor:** `ovladanie-antena-ext-box.ino` — `parseRS485Incoming()`  
**Čo sa stane:** Nano relé **nemeni**. Master dá `currentStates = CCCCCCCC` → LED/web klamú, zapne sa 5 s recovery (opakovaný bombard).  
**Opraviteľné:** áno. Po timeout nechať `currentStates` (alebo ich označiť unknown), `nodeAnomaly = OFFLINE`, nesimulovať C. Recovery môže ostať.

### F-04. Dvojité TX pri štarte

**Súbor:** `setup()` — `triggerRS485Transmission()` vždy a ešte raz ak je A/B  
**Čo sa stane:** Nano spracuje dva príkazy a pošle dva ACK. SoftwareSerial + polovičný duplex = kolízia, stratený ACK, falošný timeout.  
**Opraviteľné:** áno. Jedno TX po načítaní NVS.

### F-05. `clientID` sa dá zmeniť, Nano je navždy #1

**Súbor:** USB `SETID`, web ukazuje ID  
**Čo sa stane:** ID ≠ 1 → Nano paket ignoruje, žiadny ACK, anténa sa neprepne.  
**Opraviteľné na Nano:** nie.  
**Obchádzka na masteri:** zablokovať SETID / tvrdo `clientID = 1` / neukladať iné ID.

### F-06. NVS: `prefs.begin` dvakrát bez `end()`

**Súbor:** `setup()`  
**Čo sa stane:** načítanie `ant_states` / Wi-Fi / PIN môže zlyhať alebo leakovať handle — po reštarte zlá konfigurácia.  
**Opraviteľné:** áno.

### F-07. Wi-Fi `setAutoReconnect(false)` a žiadny vlastný reconnect

**Čo sa stane:** po výpadku AP ovládanie z webu mŕtve do rebootu. Relé na stožiari ostanú (Nano sa neresetuje) — ale master je slepý.  
**Opraviteľné:** áno (`setAutoReconnect(true)` alebo slučka).

### F-08. Po prebudení zo stand-by LED ostanú zhasnuté

**Súbor:** `processLEDGlows()` — v stand-by vypne PWM, **nevymaže** `lastSentRed/Blue`  
**Čo sa stane:** po ACK, kde stav = predchádzajúci A/B, zápis sa preskočí (hodnoty „rovnaké“), LED fyzicky nula.  
**Opraviteľné:** áno. Pri vstupe/výstupe stand-by nastaviť `lastSent*` na -1.

### F-09. Stand-by: web sa reloaduje každé 4–5 s

**Súbor:** JS v `html_pages.h` vs `/api/status`  
**Web ukáže:** `💤 REŽIM SPÁNKU`  
**API vráti:** `STANDBY`  
**Čo sa stane:** texty sa nerovnajú → `location.reload()` v slučke. ESP skladá obrovské HTML, stránka bliká.  
**Opraviteľné:** áno. V JS mapovať `STANDBY` na rovnaký reťazec ako v HTML (alebo porovnávať surový kód).

### F-10. Rozbitý HTML formulár — druhé tlačidlo nič nerobí

**Súbor:** `html_pages.h` — `</form>` po „Aplikovať“, potom ešte `Odoslať nastavenia do RS485` mimo formulára  
**Opraviteľné:** áno. Jedno submit tlačidlo, jedna `</form>`, oprav `</div>`.

### F-11. Extra `}` v CSS

**Súbor:** `html_pages.h` pred `</style>`  
**Riziko:** prehliadač môže orezať štýly (pending, laylout).  
**Opraviteľné:** áno.

### F-12. PHONE/RTTY a (zbytočné) ID sa neukladajú do NVS

**Fork:** po reštarte vždy PHONE (GPIO HIGH), aj keď bol RTTY. Nano to neovplyvňuje, ale lokálna výhybka skočí.  
**Opraviteľné:** áno (fork). ID držať na 1 (F-05).

---

## E — Edge cases / doladenie (master)

### E-01. `handleSet` čaká 100 ms, Nano + SoftwareSerial často nestihne

ACK má ~40 bajtov + `applyRelayStates` + `flush`. `TIMEOUT_MS = 500` je na linku, ale stránka sa vykreslí skôr → F-02.  
**Oprava:** predĺžiť wait, alebo hneď odpovedať a nechať JS/API obnoviť stav (bez rozbitia F-09).

### E-02. Text 429 hovorí „3 sekundy“, timeout je 0,5 s, retry 5 s

Mätúce, nie rozbité.

### E-03. `/profile` nepozerá `waitingForReply`

Kým `handleSet` dá 429, profil pošle druhý paket. Nano môže dostať kolíziu.  
**Oprava:** rovnaká poistka ako pri `/set`.

### E-04. `/set` počas stand-by — ROZHODNUTÉ

Stand-by = utlmenie: žiadny heartbeat/recovery na RS485, LED zhasnuté, chybový stav (timeout/diskoška) potlačený. Relé na Nano sa nemenia.

Keď operátor **zmení anténu** (`/set`, profil, USB `SEND`), stand-by sa zruší a master znova komunikuje.

**Stav v kóde:** implementované (`cancelStandbyForOperatorChange` + `enterStandbyMute` / `exitStandbyMute`).

### E-05. USB `SETVAL` neukladá NVS a neposiela samo

Po reštarte preč. Zámer ak platí „najprv SEND“ — treba to v HELP dopísať. `SETID` nepoužívať (F-05).

### E-06. `ant_states` z NVS bez kontroly A/B/C

Krátky/garbage string → `charAt` = 0 → Nano tie znaky preskočí, ACK vráti staré relé, master uvidí nesúlad, 5 s bombard.  
**Oprava:** pri načítaní/webe pustiť len A/B/C, inak C.

### E-07. Neznámy `/profile?type=foo`

Uloží NVS a aj tak TX. Zbytočný paket na stožiar.

### E-08. `rs485Buffer` / `usbBuffer` bez max dĺžky

Nano vždy posiela `\n`, takže pri čistej linke OK. Šum bez newline vie zjesť heap. Limit ~80 znakov je zlučiteľný s ACK formátom.

### E-09. `isUserInteracting` sa nikdy nevynuluje

Po kliknutí na select prestane auto-refresh. Operátor nevidí ACK, kým nerobí F5. Reset po N sekundách alebo po blur.

### E-10. Slider jasu: `map()` tam a späť driftuje

Kosmetika LED, nie relé.

---

## N — Neopraviteľné (klient sa nemení)

Tieto veci **sa na Nano nedajú opraviť**. Master ich môže len obísť alebo akceptovať.

| ID | Problém na Nano | Obchádzka na masteri |
|----|-----------------|----------------------|
| N-01 | ID natvrdo `1` | Tvrdo clientID=1, zrušiť SETID (F-05) |
| N-02 | Žiadny CRC / iný formát paketu | **Neda sa pridať.** Ostať pri `#1` + medzera + 8 písmen + `\n` |
| N-03 | `payload.length() != 8` → ticho bez ACK | Posielať vždy presne 8× A/B/C |
| N-04 | Neplatný znak = nechá staré relé, ACK aj tak príde | Validovať **pred** TX; ak ACK ≠ target, je to reálny nesúlad na stožiari |
| N-05 | `readStringUntil('\n')` je blokujúce (default timeout 1 s) | Nesmie sa prekrývať TX; neposielať fragmenty; `TIMEOUT_MS` radšej ≥ 500–800 ms |
| N-06 | SoftwareSerial, kolízie, žiadny HW RS485 UART | Jedno TX, čakať ACK, stand-by bez pollu (už zámer) |
| N-07 | Výpadok linky **nemení** relé na Nano | Master nesmie po timeout kresliť C ako fakt (F-03). Bezpečný stav na stožiari **nejde** doplniť bez zmeny Nano. |
| N-08 | NTC chyba len ako `999.0`; vzorec/piny A6/A7 | Parser `> 900` už sedí. Iný prah/jednotky sa meniť nemusia |

**N-07 je jediné tvrdé obmedzenie bezpečnosti antény:** ak RS485 umrie počas TX, relé ostanú kde boli. To sa z mastera nedá zachrániť.

---

## B — Bezpečnosť (neskôr, sieť je privátna)

Nie P0. Zabránia nehode (prefetch, omyl), nie útoku z internetu.

| ID | Téma | Prečo nie prvé |
|----|------|----------------|
| B-01 | `/profile` a `/toggleStandby` bez `isAuthorized()` | Na LAN to vie ktokoľvek s IP. Na zabezpečenej sieti nízke. |
| B-02 | Zmena stavu cez GET (profily ako `<a href>`) | Prefetch prehliadača **vie** spustiť „Všetko OFF“ aj bez útočníka — to už je funkcia. POST je jednoduchá poistka. |
| B-03 | Cookie `session=auth_valid` bez tajomstva | Kozmetický PIN. |
| B-04 | PIN/SSID v HTML bez escapovania | Rozbitie stránky pri divnom SSID. |
| B-05 | OLED ukazuje PIN | Fyzický prístup k boxu. |
| B-06 | Hardcoded Wi-Fi v zdroji | Pri zdieľaní projektu. |

**Odporúčanie po funkčných opravách:** aspoň B-02 (POST namiesto GET na profily) — to nie je „bezpečnosť voči hackerovi“, ale proti Chrome prefetch.

---

## P3 / hygiena (keď ostane čas)

- `console_display.h` — `checkSerialConsole()` sa nevolá
- `writeLED()` sa nevolá
- komentár „ĽAVÁ skupina“ na pravej strane webu
- `config.h`: `extern String nodeAnomaly = "OK"` (definícia v headri)
- JS mapuje `NONE`, firmware posiela `OK`
- `/api/status` bez prihlásenia (len stav, nie relé)
- chýba logout

---

## Ako stoja dva boxy proti sebe

```
Master ESP32-C3                         Nano (NEMENIŤ)
targetStates ──TX──► #1 AABBCCAA\n  ►  ak ID=1 a 8×A/B/C
waitingForReply                          applyRelayStates()
     ▲                                   sendConfirmation()
     └── RX #1ACK:AABBCCAA,T1:..,T2:.. ◄──
currentStates = echo ACK

Stand-by: žiadny TX, LED off, targetStates aj relé na Nano NEMENIŤ
Timeout: Nano relé drží; master dnes klamstvá current=C  ← opraviť
```

---

## Odporúčané poradie (funkcia)

1. **F-01, F-02, F-10, F-11** — web nepošle zlý stav a formulár funguje  
2. **F-03, F-04, E-03** — pravdivý stav + jedno TX, žiadna kolízia so SoftwareSerial  
3. **F-05** — ID natvrdo 1 (Nano iné nepočúva)  
4. **F-08, F-09, E-04** — stand-by ako má byť: ticho na zbernici, ticho na LED, bez reload slučky, LED po prebudení znova svietia  
5. **F-06, F-07, F-12, E-06** — pamäť a Wi-Fi, aby reštart vrátil to isté  
6. **E-01, E-08, E-09** — doladenie čakania a UI  
7. **B-02** a zvyšok B až potom

---

## Rýchla mapa: starý zoznam → tento

| Starý ID | Teraz |
|----------|--------|
| P0-06 stand-by nevypína relé | **Nie je chyba** |
| P0-01, P0-02, P0-03 auth | B-01–B-03 (nízka priorita; B-02 má aj funkčný zmysel) |
| P0-04 L4 B | **F-01** |
| P0-05 select = current | **F-02** |
| P1 timeout = C | **F-03** |
| P1 double TX | **F-04** |
| P1 SETID | **F-05** + **N-01** |
| P2 CRC | **N-02 neopraviteľné** |
| P2 DE/RE pin | Nano má auto-smer; master tiež bez DE — nechať, kým HW sériovo funguje |
