# WagaWezy

Firmware dla wagi do węzy opartej na **ESP32**. Urządzenie mierzy masę, wyświetla ją na ekranie OLED, utrzymuje stałe połączenie WebSocket z POS (POSeidon) i obsługuje aktualizacje firmware przez OTA z GitHub Releases.

**Aktualna wersja firmware:** `1.2.0`

---

## Spis treści

- [Funkcje](#funkcje)
- [Wymagany sprzęt](#wymagany-sprzęt)
- [Okablowanie (GPIO)](#okablowanie-gpio)
- [Przyciski](#przyciski)
- [Wymagania programistyczne](#wymagania-programistyczne)
- [Kompilacja i wgrywanie](#kompilacja-i-wgrywanie)
- [Pierwsza konfiguracja](#pierwsza-konfiguracja)
- [Tryb normalny (codzienna praca)](#tryb-normalny-codzienna-praca)
- [Tryb serwisowy](#tryb-serwisowy)
- [Integracja API (JSON)](#integracja-api-json)
- [WiFi i ping](#wifi-i-ping)
- [Aktualizacja OTA](#aktualizacja-ota)
- [Struktura projektu](#struktura-projektu)
- [Rozwiązywanie problemów](#rozwiązywanie-problemów)

---

## Funkcje

- Pomiar masy przez moduł **HX711** (zakres kalibracji do 50 kg)
- Wyświetlacz **OLED SH1106 128×64** (I2C)
- Zegar czasu rzeczywistego **DS3231** (I2C, strefa Polska CET/CEST)
- 8 przycisków: tara, OK, 6 przycisków akcji (1–6)
- **Stałe połączenie WiFi** — urządzenie łączy się z siecią przy starcie i utrzymuje połączenie w tle
- **Stałe połączenie WebSocket z POS** — masa na żywo, potwierdzenia pozycji (ack), cofanie (undo), telemetria
- **Ping keepalive z telemetrią** — wysyłany po WebSocket co skonfigurowany interwał (wersja firmware, RSSI, uptime, wolny heap)
- **Portal serwisowy** (captive portal) do konfiguracji WiFi, kalibracji, czasu i OTA
- **Obsługa serwisowa z wagi** — czas, kalibracja, tara, test połączenia WS, ekran info i OTA dostępne przyciskami, bez telefonu
- **Aktualizacja OTA** z GitHub Releases
- Sygnalizacja dźwiękowa (buzzer)
- Zapis konfiguracji w pamięci NVS (Preferences)

---

## Wymagany sprzęt

| Element | Opis |
|---------|------|
| Mikrokontroler | ESP32 (np. moduł 30-pin) |
| Waga | Moduł HX711 + tensometr |
| Wyświetlacz | OLED SH1106 128×64, I2C |
| Zegar RTC | DS3231, I2C (wspólna magistrala z OLED) |
| Wejścia | 8 przycisków (do masy, z pull-up) |
| Buzzer | Aktywny 3,3 V |
| Zasilanie | Stabilne 3,3 V / 5 V (zależnie od modułu ESP32) |

> **Uwaga:** Unikaj pinów strapping przy bootowaniu ESP32: GPIO **0**, **2**, **12**. W tej konfiguracji GPIO **15** jest używany jako przycisk 4.

---

## Okablowanie (GPIO)

Domyślne przypisanie pinów znajduje się w pliku [`config/Pins.h`](config/Pins.h).

| Funkcja | GPIO |
|---------|------|
| I2C SDA (OLED + DS3231) | 21 |
| I2C SCL (OLED + DS3231) | 22 |
| HX711 DT | 25 |
| HX711 SCK | 26 |
| Buzzer | 18 |
| Przycisk Tara | 13 |
| Przycisk OK | 14 |
| Przycisk 1 | 27 |
| Przycisk 2 | 32 |
| Przycisk 3 | 33 |
| Przycisk 4 | 15 |
| Przycisk 5 | 4 |
| Przycisk 6 | 5 |

Przyciski podłącz do **GND** — w firmware włączony jest wewnętrzny pull-up (`INPUT_PULLUP`).

---

## Przyciski

| Przycisk | GPIO | Działanie |
|----------|------|-----------|
| **Tara** | 13 | Szybka tara bieżącego obciążenia (offset w RAM) |
| **OK** | 14 | Potwierdzenie instalacji OTA; nawigacja w menu serwisowych |
| **OK** (przytrzymane 1,5 s) | 14 | Cofnięcie w POS pozycji dodanych z wagi (undo) |
| **1–6** | 27, 32, 33, 15, 4, 5 | Wysyłka pomiaru przez WebSocket (wymaga stabilnego odczytu > 0) |

### Wejście w tryb serwisowy

Przytrzymaj jednocześnie **Tara + OK** przez **5 sekund**. Na ekranie pojawi się pasek postępu. Po wejściu w tryb serwisowy waga uruchamia punkt dostępowy WiFi.

---

## Wymagania programistyczne

### Arduino IDE

1. [Arduino IDE](https://www.arduino.cc/en/software) 2.x
2. Pakiet płytki **esp32** by Espressif (Board Manager)
3. Schemat partycji: **Minimal SPIFFS** lub inny z partycjami **OTA** (`ota_0` + `ota_1`) — wymagane do aktualizacji OTA

### Biblioteki (Library Manager)

| Biblioteka | Autor | Użycie |
|------------|-------|--------|
| **HX711** | bogde | Odczyt wagi |
| **U8g2** | olikraus | Wyświetlacz SH1106 |
| **arduinoWebSockets** | Links2004 | Klient WebSocket (PosLink) |

Pozostałe zależności (`WiFi`, `HTTPClient`, `Preferences`, `Wire`, `DNSServer`, `WebServer`) są częścią core ESP32.

---

## Kompilacja i wgrywanie

1. Sklonuj repozytorium:
   ```bash
   git clone https://github.com/pszczelarzTechniczny/WagaWeza.git
   ```
2. Otwórz plik `WagaWezy.ino` w Arduino IDE.
3. Wybierz płytkę: **ESP32 Dev Module** (lub odpowiednik Twojego modułu).
4. Ustaw schemat partycji z obsługą OTA.
5. Skompiluj i wgraj firmware przez USB.

Monitor szeregowy: **115200 baud**.

---

## Pierwsza konfiguracja

1. Wgraj firmware i uruchom urządzenie.
2. Przy starcie wyświetli się ekran powitalny (domyślnie: *Witam / Pszczelarza / z Wąchocka*).
3. Waga spróbuje połączyć się z zapisaną siecią WiFi (przy pierwszym uruchomieniu brak zapisanej sieci — to normalne).
4. Wejdź w **tryb serwisowy** (Tara + OK, 5 s).
5. Połącz telefon lub komputer z siecią WiFi:
   - **SSID:** `WagaWezy-Setup`
   - **Hasło:** `12340000` (domyślne; można zmienić w portalu)
6. Otwórz w przeglądarce adres wyświetlony na ekranie wagi (zwykle `192.168.4.1`). Captive portal powinien przekierować automatycznie.

### Kolejność konfiguracji (zalecana)

1. **WiFi** — zapisz SSID i hasło domowej sieci
2. **Zegar DS3231** — synchronizacja NTP (*Get time z internetu*) lub ustawienie ręczne
3. **Endpoint API** — adres URL serwera POSeidon (z niego wyprowadzany jest adres WebSocket)
4. **Kalibracja wagi** — opróżnij wagę, potem nałóż znane obciążenie (np. 1000 g)
5. **Test połączenia WS** — przycisk *Testuj wysyłkę* w portalu
6. **Interwał pinga** — opcjonalnie (domyślnie 30 s; `0` = wyłączony)
7. Wyjdź z trybu serwisowego — waga wróci do normalnej pracy z aktywnym WiFi

---

## Tryb normalny (codzienna praca)

Po starcie waga:

1. Inicjalizuje peryferia (HX711, OLED, RTC, przyciski)
2. Wyświetla ekran powitalny (2 s)
3. Wykonuje automatyczną tarę przy pustej wadze (jeśli skalibrowana)
4. **Łączy się z WiFi** i utrzymuje połączenie w tle
5. **Nawiązuje połączenie WebSocket z POS** i wysyła masę na żywo
6. Wyświetla aktualną masę netto (gramy) oraz godzinę z DS3231

### Ekran roboczy

Na ekranie wyświetlane są:

- masa netto i godzina z DS3231
- trzy kwadraciki **W / S / P** (WiFi / WebSocket / POS) — pełny kwadrat = aktywne połączenie
- kółko stabilności w lewym dolnym rogu — pełne = stabilny odczyt

### Wysyłka pomiaru

Naciśnij przycisk **1–6**, aby wysłać pomiar do POS przez WebSocket. Wysyłka wymaga:

- stabilnego odczytu (odchylenie ±10 g przez co najmniej 600 ms; kółko stabilności pełne)
- masy netto > 0 g i < 50 kg
- aktywnego połączenia WebSocket (wskaźnik **S** pełny)

Po wysyłce serwer odsyła `ack`. Gdy odpowiedź nie nadejdzie w 2 s, firmware ponawia próbę raz z tym samym `eventId` (serwer deduplikuje). Po dwóch nieudanych próbach wyświetla błąd.

#### Kody błędów ack

| Kod `error` | Komunikat na OLED |
|-------------|-------------------|
| `slot-not-mapped` | Slot bez produktu |
| `product-inactive` | Produkt nieaktywny |
| `product-not-weighed` | Nie na wage |
| `bad-weight` | Najpierw poloz towar |

### Cofnięcie (undo)

Przytrzymaj **OK** przez ~1,5 s — waga wyśle `undo {}` do POS, który wycofa z bieżącego koszyka wszystkie pozycje dodane z wagi. Drugie naciśnięcie (gdy nie ma nic do cofnięcia) jest bezpieczne.

### Szybka tara

Przycisk **Tara** ustawia offset bieżącego obciążenia (tymczasowa tara w RAM, bez zapisu do NVS).

---

## Tryb serwisowy

### Dostęp

- **Wejście:** Tara + OK (5 s)
- **AP WiFi:** `WagaWezy-Setup`
- **Hasło AP:** domyślnie `12340000` (8–63 znaki, konfigurowalne)
- **Adres portalu:** IP punktu dostępowego (np. `192.168.4.1`)

### Dostępne opcje w portalu WWW

| Sekcja | Opis |
|--------|------|
| Hasło AP | Zmiana hasła sieci serwisowej |
| Ekran powitalny | 3 linie tekstu przy starcie (max 24 znaki) |
| WiFi | Skan sieci, zapis SSID i hasła |
| Wysyłka danych | Endpoint URL (z niego wyprowadzany adres WS), interwał pinga, test połączenia WS |
| Token API | Token autoryzacji WS (`?token=` na `/ws/scale`); musi być zgodny z `POS_API_TOKEN` serwera; puste = bez tokenu |
| Tara | Zapis trwałej tary (min. 20 g obciążenia) |
| Kalibracja | Opróżnienie wagi, kalibracja znanym obciążeniem (100–50000 g), reset HX711 |
| Zegar DS3231 | Synchronizacja NTP (CET/CEST) lub ustawienie ręczne |
| Reset | Usunięcie kalibracji i tary z NVS |
| Aktualizacja OTA | Pobranie nowego firmware z GitHub |
| Wyjście | Powrót do trybu normalnego |

### Zdalna aktualizacja z POS

W oknie konfiguracji wagi w POSeidonie (ikona wagi → *Aktualizuj wagę*) można
wysłać do wagi komendę `{type:"update"}` po WebSocket. Waga — o ile nie trwa
pomiar — sama pobierze i zainstaluje najnowszy release z GitHub **bez
potwierdzania przyciskiem OK**, po czym się zrestartuje. Endpoint
`POST /api/scale/update` jest objęty tokenem API serwera (`POS_API_TOKEN`),
gdy ten jest ustawiony.

### Obsługa z wagi (bez WWW)

W trybie serwisowym przyciski na wadze mają funkcje serwisowe — telefon nie jest potrzebny:

| Przycisk | Funkcja |
|----------|---------|
| **1** | Test połączenia WS — łączy się z WebSocket i raportuje stan (adres WS, wynik) na OLED |
| **2** | Ekran info — wersja FW, SSID, RSSI, IP, endpoint, interwał pinga (powrót: Tara/OK/2) |
| **3** | Menu czasu DS3231 — NTP (przycisk 1) lub edycja ręczna (przycisk 2; 1/+, 2/−, OK = następne pole) |
| **4** | Sprawdzenie i instalacja aktualizacji OTA (jak *Sprawdź aktualizacje* w portalu) |
| **5** | Kalibracja — krok 1: opróżnij wagę + OK; krok 2: ustaw masę wzorca 1/+ 2/− (krok 50 g, przytrzymanie = autorepeat, po 3 s krok 500 g), OK = kalibruj |
| **6** | Zapis trwałej tary do NVS (min. 20 g obciążenia) |
| **Tara** (przytrzymane 2 s) | Wyjście z trybu serwisowego |

Ekran serwisowy jest statyczny: nagłówek z godziną, adres IP portalu, mapa przycisków i podpowiedź wyjścia (Tara 2 s).

---

## Integracja API (JSON)

Waga komunikuje się z POS przez **WebSocket**. Adres WS wyprowadzany jest automatycznie z endpointu zapisanego w portalu:

```
http://host[:port]/cokolwiek   →  ws://host:port/ws/scale
https://host[:port]/cokolwiek  →  wss://host:port/ws/scale
```

### Komunikaty: waga → serwer

#### `hello` — po nawiązaniu połączenia

```json
{ "type": "hello", "role": "scale", "fw": "1.2.0" }
```

#### `weight` — masa na żywo

Wysyłany przy każdej zmianie odczytu lub flagi stabilności (maks. ~10/s):

```json
{ "type": "weight", "kg": 1.234, "stable": true }
```

#### `button` — naciśnięcie slotu 1–6

```json
{ "type": "button", "slot": 3, "kg": 1.234, "eventId": "7f3a-42" }
```

| Pole | Typ | Opis |
|------|-----|------|
| `slot` | int | Numer slotu **1–6** |
| `kg` | float | Masa netto w kilogramach |
| `eventId` | string | Unikalny identyfikator zdarzenia (losowy prefiks sesji + licznik) |

#### `undo` — cofnięcie (OK przytrzymane ~1,5 s)

```json
{ "type": "undo" }
```

#### `ping` — telemetria keepalive

Wysyłany co skonfigurowany interwał (domyślnie co 30 s):

```json
{
  "type": "ping",
  "deviceId": "AA:BB:CC:DD:EE:FF",
  "fw": "1.2.0",
  "rssi": -62,
  "uptimeSec": 1234,
  "freeHeap": 123456
}
```

| Pole | Typ | Opis |
|------|-----|------|
| `type` | string | Zawsze `"ping"` |
| `deviceId` | string | Adres MAC interfejsu WiFi |
| `fw` | string | Wersja firmware (`FW_VERSION`) |
| `rssi` | int | Siła sygnału WiFi w dBm |
| `uptimeSec` | int | Czas od startu urządzenia w sekundach |
| `freeHeap` | int | Wolna pamięć heap w bajtach |

| Ustawienie | Wartość |
|------------|---------|
| Domyślny interwał | 30 s |
| Zakres | 0 (wyłączony), 5–3600 s |
| Konfiguracja | Portal serwisowy → *Interwał pinga* |

### Komunikaty: serwer → waga

#### `welcome` — odpowiedź na `hello`

```json
{ "type": "welcome" }
```

#### `pos` — zmiana stanu POS

```json
{ "type": "pos", "online": true }
```

Pole `online` steruje wskaźnikiem **P** na ekranie roboczym.

#### `ack` — potwierdzenie `button`

Sukces:
```json
{ "type": "ack", "eventId": "7f3a-42", "ok": true }
```

Błąd:
```json
{ "type": "ack", "eventId": "7f3a-42", "ok": false, "error": "slot-not-mapped" }
```

Kody błędów i komunikaty wyświetlane na OLED:

| Kod `error` | Komunikat na OLED |
|-------------|-------------------|
| `slot-not-mapped` | Slot bez produktu |
| `product-inactive` | Produkt nieaktywny |
| `product-not-weighed` | Nie na wage |
| `bad-weight` | Najpierw poloz towar |

---

## WiFi i ping

### Zachowanie połączenia

- Przy starcie waga próbuje połączyć się z zapisaną siecią (timeout ~12 s).
- W tle co 5 s sprawdzany jest stan połączenia; przy utracie sieci następuje automatyczne ponowne łączenie (backoff 10 s).
- Po nawiązaniu WiFi uruchamiany jest klient WebSocket (`PosLink`), który utrzymuje stałe połączenie z POS.
- W trybie serwisowym i podczas OTA zarządzanie WiFi i WS jest wstrzymywane, aby nie kolidować z AP/portalem.

### Wymagania sieciowe

- Sieć WiFi 2,4 GHz (standard ESP32)
- Dostęp do serwera POSeidon z sieci lokalnej lub internetu (zależnie od konfiguracji)

---

## Aktualizacja OTA

Firmware pobiera najnowsze wydanie z **GitHub Releases** repozytorium skonfigurowanego w `WagaWezy.ino`:

```cpp
static const char* OTA_GITHUB_OWNER = "pszczelarzTechniczny";
static const char* OTA_GITHUB_REPO = "WagaWeza";
```

### Jak zaktualizować urządzenie

1. Wejdź w tryb serwisowy.
2. W portalu kliknij **Sprawdź aktualizacje** (lub naciśnij **przycisk 4** na wadze).
3. Na ekranie wagi **przytrzymaj przycisk OK**, aby potwierdzić instalację.
4. Postęp aktualizacji wyświetlany jest na OLED.

### Wymagania OTA (dla maintainerów)

- Schemat partycji ESP32 z **dwoma slotami OTA** (`ota_0`, `ota_1`)
- GitHub Release z plikiem `.bin` (najnowsza wersja musi być wyższa niż `FW_VERSION` w firmware)
- Urządzenie musi mieć skonfigurowane WiFi (lub skorzysta z portalu konfiguracyjnego OTA)

Po każdej zmianie wersji zaktualizuj stałą w `WagaWezy.ino`:

```cpp
static const char* FW_VERSION = "1.2.0";
```

---

## Struktura projektu

```
WagaWezy/
├── WagaWezy.ino              # Główny sketch, wersja FW, konfiguracja OTA
├── config/
│   ├── Pins.h                # Mapowanie GPIO
│   └── Buttons.h             # Indeksy przycisków
└── src/
    ├── input/                # Obsługa przycisków
    ├── ui/                   # Wyświetlacz OLED (U8g2)
    ├── scale/                # HX711, kalibracja, tara
    ├── rtc/                  # DS3231, NTP, strefa PL
    ├── storage/              # NVS: WiFi, endpoint, ping, kalibracja
    ├── network/              # PosLink (klient WS), WiFi manager, ping
    ├── workflow/             # Logika wysyłki po naciśnięciu przycisku
    ├── service/              # Portal serwisowy WWW, menu czasu i kalibracji
    ├── wifi/                 # Portal WiFi (OTA fallback)
    ├── ota/                  # Aktualizacja z GitHub Releases
    └── output/               # Buzzer
```

---

## Rozwiązywanie problemów

| Problem | Możliwa przyczyna | Co zrobić |
|---------|-------------------|-----------|
| *Brak DS3231* przy starcie | Brak modułu RTC lub błędne I2C | Sprawdź SDA/SCL (21/22), zasilanie, adres I2C |
| *Nie skalibrowano* | Brak kalibracji w NVS | Wejdź w portal serwisowy → Kalibracja |
| *Brak serwera* przy slocie | WS niepołączony | Sprawdź endpoint w portalu i czy POSeidon działa |
| *WiFi timeout* | Zła sieć lub hasło | Ponowna konfiguracja WiFi w portalu |
| *Poloz towar i poczekaj* | niestabilny/pusty odczyt | Odczekaj aż kółko stabilności będzie pełne |
| OTA: *Brak partycji OTA* | Zły schemat partycji | W Arduino IDE wybierz schemat z OTA |
| HTTPS nie działa | Certyfikat / serwer | Firmware używa TLS bez weryfikacji certyfikatu; sprawdź URL i log Serial |

Logi diagnostyczne (pomiary, pingi, stan WiFi i WS) dostępne w **Monitorze szeregowym** (115200 baud).

---

## Autor

Projekt **WagaWezy** — waga do węzy dla pszczelarzy.

Repozytorium: [github.com/pszczelarzTechniczny/WagaWeza](https://github.com/pszczelarzTechniczny/WagaWeza)
