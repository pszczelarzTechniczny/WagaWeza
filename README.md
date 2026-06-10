# WagaWezy

Firmware dla wagi do węzy opartej na **ESP32**. Urządzenie mierzy masę, wyświetla ją na ekranie OLED, wysyła pomiary przez WiFi na skonfigurowany endpoint HTTP(S) oraz obsługuje aktualizacje firmware przez OTA z GitHub Releases.

**Aktualna wersja firmware:** `1.0.2`

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
- Wysyłka pomiarów metodą **POST JSON** na konfigurowalny endpoint
- **Ping keepalive** — okresowy POST `{"type":"ping"}` na ten sam endpoint (domyślnie co 30 s)
- **Portal serwisowy** (captive portal) do konfiguracji WiFi, kalibracji, czasu i OTA
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
| **OK** | 14 | Potwierdzenie instalacji OTA; nawigacja w menu czasu (tryb serwisowy) |
| **1–6** | 27, 32, 33, 15, 4, 5 | Natychmiastowa wysyłka pomiaru z przypisanym numerem przycisku |

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
3. **Endpoint API** — adres URL do wysyłki pomiarów
4. **Kalibracja wagi** — opróżnij wagę, potem nałóż znane obciążenie (np. 1000 g)
5. **Test wysyłki** — przycisk *Testuj wysyłkę* w portalu
6. **Interwał pinga** — opcjonalnie (domyślnie 30 s; `0` = wyłączony)
7. Wyjdź z trybu serwisowego — waga wróci do normalnej pracy z aktywnym WiFi

---

## Tryb normalny (codzienna praca)

Po starcie waga:

1. Inicjalizuje peryferia (HX711, OLED, RTC, przyciski)
2. Wyświetla ekran powitalny (2 s)
3. Wykonuje automatyczną tarę przy pustej wadze (jeśli skalibrowana)
4. **Łączy się z WiFi** i utrzymuje połączenie w tle
5. Wyświetla aktualną masę netto (gramy) oraz godzinę z DS3231

### Wysyłka pomiaru

Naciśnij przycisk **1–6** — waga od razu odczyta masę i wyśle POST na skonfigurowany endpoint. Nie trzeba wcześniej naciskać OK.

Po wysłaniu:
- **1 sygnał buzzera** — sukces
- **2 sygnały** — błąd (brak WiFi, brak RTC, błąd HTTP itd.)

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
| Wysyłka danych | Endpoint URL, interwał pinga, test wysyłki |
| Tara | Zapis trwałej tary (min. 20 g obciążenia) |
| Kalibracja | Opróżnienie wagi, kalibracja znanym obciążeniem (100–50000 g), reset HX711 |
| Zegar DS3231 | Synchronizacja NTP (CET/CEST) lub ustawienie ręczne |
| Reset | Usunięcie kalibracji i tary z NVS |
| Aktualizacja OTA | Pobranie nowego firmware z GitHub |
| Wyjście | Powrót do trybu normalnego |

### Menu czasu na wadze (bez WWW)

W trybie serwisowym naciśnij **przycisk 3**, aby wejść w menu ustawiania czasu DS3231 na wyświetlaczu (NTP lub edycja ręczna z przyciskiem OK).

---

## Integracja API (JSON)

Waga wysyła żądania **HTTP POST** z nagłówkiem:

```
Content-Type: application/json
User-Agent: waga-wezy-esp32
```

Obsługiwane są adresy `http://` i `https://` (TLS bez weryfikacji certyfikatu — `setInsecure()`).

### Pomiar (przyciski 1–6)

```json
{
  "timestamp": "2026-06-10T14:30:00",
  "weightGrams": 1234,
  "buttonNumber": 3
}
```

| Pole | Typ | Opis |
|------|-----|------|
| `timestamp` | string | Data i czas z DS3231 w formacie ISO 8601 (`YYYY-MM-DDTHH:MM:SS`) |
| `weightGrams` | int | Masa netto w gramach (po tarze) |
| `buttonNumber` | int | Numer przycisku **1–6** |

Mapowanie numerów przycisków po stronie backendu (POS, baza danych itd.) konfigurujesz samodzielnie — firmware nie wysyła nazw ani kodów akcji.

### Ping keepalive

Wysyłany okresowo na **ten sam endpoint** co pomiary (gdy WiFi połączone, brak aktywnej wysyłki pomiaru):

```json
{
  "type": "ping"
}
```

| Ustawienie | Wartość |
|------------|---------|
| Domyślny interwał | 30 s |
| Zakres | 0 (wyłączony), 5–3600 s |
| Konfiguracja | Portal serwisowy → *Interwał pinga* |

Backend powinien rozróżniać ping od pomiaru (np. po obecności pola `type`).

### Odpowiedź serwera

Uznawane za sukces: kod HTTP **200–299**.

---

## WiFi i ping

### Zachowanie połączenia

- Przy starcie waga próbuje połączyć się z zapisaną siecią (timeout ~12 s).
- W tle co 5 s sprawdzany jest stan połączenia; przy utracie sieci następuje automatyczne ponowne łączenie (backoff 10 s).
- Przed wysyłką pomiaru waga upewnia się, że WiFi jest aktywne (`ensureConnected`).
- W trybie serwisowym i podczas OTA zarządzanie WiFi jest wstrzymywane, aby nie kolidować z AP/portalem.

### Wymagania sieciowe

- Sieć WiFi 2,4 GHz (standard ESP32)
- Dostęp do endpointu API z sieci lokalnej lub internetu (zależnie od konfiguracji)

---

## Aktualizacja OTA

Firmware pobiera najnowsze wydanie z **GitHub Releases** repozytorium skonfigurowanego w `WagaWezy.ino`:

```cpp
static const char* OTA_GITHUB_OWNER = "pszczelarzTechniczny";
static const char* OTA_GITHUB_REPO = "WagaWeza";
```

### Jak zaktualizować urządzenie

1. Wejdź w tryb serwisowy.
2. W portalu kliknij **Sprawdź aktualizacje**.
3. Na ekranie wagi **przytrzymaj przycisk OK**, aby potwierdzić instalację.
4. Postęp aktualizacji wyświetlany jest na OLED.

### Wymagania OTA (dla maintainerów)

- Schemat partycji ESP32 z **dwoma slotami OTA** (`ota_0`, `ota_1`)
- GitHub Release z plikiem `.bin` (najnowsza wersja musi być wyższa niż `FW_VERSION` w firmware)
- Urządzenie musi mieć skonfigurowane WiFi (lub skorzysta z portalu konfiguracyjnego OTA)

Po każdej zmianie wersji zaktualizuj stałą w `WagaWezy.ino`:

```cpp
static const char* FW_VERSION = "1.0.2";
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
    ├── network/              # WiFi manager, wysyłka pomiarów, ping
    ├── workflow/             # Logika wysyłki po naciśnięciu przycisku
    ├── service/              # Portal serwisowy WWW, menu czasu
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
| *Brak adresu* przy wysyłce | Nie ustawiono endpointu | Portal → Wysyłka danych → zapisz URL |
| *WiFi timeout* | Zła sieć lub hasło | Ponowna konfiguracja WiFi w portalu |
| Wolna wysyłka | Brak WiFi przy starcie | Od wersji 1.0.2 WiFi łączy się przy boot — upewnij się, że sieć jest zapisana |
| OTA: *Brak partycji OTA* | Zły schemat partycji | W Arduino IDE wybierz schemat z OTA |
| HTTPS nie działa | Certyfikat / serwer | Firmware używa TLS bez weryfikacji certyfikatu; sprawdź URL i log Serial |

Logi diagnostyczne (wysyłane JSON, pingi, stan WiFi) dostępne w **Monitorze szeregowym** (115200 baud).

---

## Autor

Projekt **WagaWezy** — waga do węzy dla pszczelarzy.

Repozytorium: [github.com/pszczelarzTechniczny/WagaWeza](https://github.com/pszczelarzTechniczny/WagaWeza)
