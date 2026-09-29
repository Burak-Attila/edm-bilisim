// Erişte kesme makinesi - Arduino Uno / Nano
//
// Gerekli kütüphaneler (Arduino IDE > Kütüphane Yöneticisi):
//   - AccelStepper       (Mike McCauley)
//   - LiquidCrystal I2C  (Frank de Brabander)
//
// Butonlar:  SEC   -> tarif değiştir (HAZIR / HATA durumunda)
//            BASLA -> başlat / duraklatmadan devam / hatayı sıfırla
//            DUR   -> kesim bittiğinde dur; tekrar basınca iptal
// Acil stop sürücülerin enerjisini DONANIMLA keser; yazılım ayrıca izler.

#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#include "config.h"
#include "kesim.h"

using namespace eriste;

AccelStepper ileri(AccelStepper::DRIVER, PIN_ILERI_STEP, PIN_ILERI_DIR);
AccelStepper giyotin(AccelStepper::DRIVER, PIN_GIYOTIN_STEP, PIN_GIYOTIN_DIR);
LiquidCrystal_I2C lcd(LCD_ADRES, 16, 2);

const MakineAyar AYAR = {ILERI_ADIM_PER_MM_X10000, SENSOR_BICAK_MESAFE_UM,
                         CIKIS_MESAFE_UM};
Kontrolcu kontrol(AYAR, TARIFLER, TARIF_SAYISI);

bool sifirlamaAktif = false;

// --- Sıçrama önleyici (debounce) giriş ---------------------------------------
struct Giris {
  uint8_t pin;
  uint16_t beklemeMs;
  bool kararli;       // true = aktif (LOW)
  bool ham;
  uint32_t degisimMs;

  void baslat() {
    pinMode(pin, INPUT_PULLUP);
    kararli = ham = (digitalRead(pin) == LOW);
    degisimMs = millis();
  }
  // Aktif olduğu ilk döngüde true döner.
  bool guncelle(uint32_t simdi) {
    bool okunan = (digitalRead(pin) == LOW);
    if (okunan != ham) {
      ham = okunan;
      degisimMs = simdi;
    }
    if (ham != kararli && simdi - degisimMs >= beklemeMs) {
      kararli = ham;
      return kararli;
    }
    return false;
  }
};

Giris btnSec = {PIN_BTN_SEC, BUTON_BEKLEME_MS};
Giris btnBasla = {PIN_BTN_BASLA, BUTON_BEKLEME_MS};
Giris btnDur = {PIN_BTN_DUR, BUTON_BEKLEME_MS};
Giris hamurSensor = {PIN_HAMUR_SENSOR, HAMUR_BEKLEME_MS};

// --- LCD: sadece değişen karakterleri, motorlar dururken yazar -------------
// I2C LCD yazmak birkaç ms sürer; motor hareket ederken yazılırsa adımlar
// gecikir. Kesimler arasındaki kısa duruşlar ekranı güncellemeye yeter.
char ekran[2][17];
char ekranGosterilen[2][17];

void ekranHazirla() {
  memset(ekran, ' ', sizeof(ekran));
  ekran[0][16] = ekran[1][16] = '\0';

  const Tarif& t = kontrol.tarif();
  char satir[17];

  snprintf(satir, sizeof(satir), "%u:%s", kontrol.tarifNo() + 1, t.ad);
  memcpy(ekran[0], satir, strlen(satir));

  switch (kontrol.durum()) {
    case Durum::HAZIR:
      snprintf(satir, sizeof(satir), "SEC:tarif BASLA");
      break;
    case Durum::SIFIRLANIYOR:
      snprintf(satir, sizeof(satir), "Bicak sifirlanir");
      break;
    case Durum::HAMUR_BEKLENIYOR:
      snprintf(satir, sizeof(satir), "Hamur ver Y:%lu",
               (unsigned long)kontrol.yaprakSayisi());
      break;
    case Durum::ILERLIYOR:
    case Durum::KESIYOR:
      snprintf(satir, sizeof(satir), "Kesim:%lu",
               (unsigned long)kontrol.kesimSayisi());
      break;
    case Durum::DURAKLATILDI:
      snprintf(satir, sizeof(satir), "DURDU BASLA/DUR");
      break;
    case Durum::HATA:
      snprintf(satir, sizeof(satir), "!%s", hataMetni(kontrol.hata()));
      break;
  }
  memcpy(ekran[1], satir, strlen(satir));
}

void ekranYaz() {
  for (uint8_t s = 0; s < 2; s++) {
    for (uint8_t c = 0; c < 16; c++) {
      if (ekran[s][c] != ekranGosterilen[s][c]) {
        lcd.setCursor(c, s);
        lcd.write(ekran[s][c]);
        ekranGosterilen[s][c] = ekran[s][c];
      }
    }
  }
}

// ---------------------------------------------------------------------------
void setup() {
  pinMode(PIN_SURUCU_EN, OUTPUT);
  digitalWrite(PIN_SURUCU_EN, !SURUCU_AKTIF_SEVIYE);
  pinMode(PIN_UYARI_LED, OUTPUT);

  pinMode(PIN_ACIL_STOP, INPUT_PULLUP);
  pinMode(PIN_KAPAK, INPUT_PULLUP);
  pinMode(PIN_GIYOTIN_SIFIR, INPUT_PULLUP);
  pinMode(PIN_KASET_B0, INPUT_PULLUP);
  pinMode(PIN_KASET_B1, INPUT_PULLUP);
  btnSec.baslat();
  btnBasla.baslat();
  btnDur.baslat();
  hamurSensor.baslat();

  ileri.setMaxSpeed(ILERI_HIZ);
  ileri.setAcceleration(ILERI_IVME);
  giyotin.setMaxSpeed(GIYOTIN_HIZ);
  giyotin.setAcceleration(GIYOTIN_IVME);

  Wire.begin();
  Wire.setClock(400000);
  lcd.init();
  lcd.backlight();
  memset(ekranGosterilen, 0, sizeof(ekranGosterilen));
}

void loop() {
  ileri.run();
  giyotin.run();

  bool sifirda = (digitalRead(PIN_GIYOTIN_SIFIR) == LOW);

  if (sifirlamaAktif) {
    if (sifirda) {
      giyotin.setCurrentPosition(0);  // anında dur, burası sıfır noktası
      sifirlamaAktif = false;
    } else if (giyotin.distanceToGo() == 0) {
      sifirlamaAktif = false;  // tur bitti, sensör görülmedi -> kontrolcü hata verir
    }
    if (!sifirlamaAktif) giyotin.setMaxSpeed(GIYOTIN_HIZ);
  }

  uint32_t simdi = millis();
  Girdiler g;
  g.acilStop = (digitalRead(PIN_ACIL_STOP) == HIGH);  // kablo koparsa da durur
  g.kapakKapali = (digitalRead(PIN_KAPAK) == LOW);
  g.giyotinSifirda = sifirda;
  g.kasetKodu = (digitalRead(PIN_KASET_B0) == LOW ? 1 : 0) |
                (digitalRead(PIN_KASET_B1) == LOW ? 2 : 0);
  g.sec = btnSec.guncelle(simdi);
  g.basla = btnBasla.guncelle(simdi);
  g.dur = btnDur.guncelle(simdi);
  hamurSensor.guncelle(simdi);
  g.hamurVar = hamurSensor.kararli;
  bool motorlarDurgun = ileri.distanceToGo() == 0 && giyotin.distanceToGo() == 0;
  g.hareketBitti = motorlarDurgun && !sifirlamaAktif;

  Komut k = kontrol.guncelle(g);

  if (k.hemenDur) {
    ileri.setCurrentPosition(ileri.currentPosition());
    giyotin.setCurrentPosition(giyotin.currentPosition());
    giyotin.setMaxSpeed(GIYOTIN_HIZ);
    sifirlamaAktif = false;
  }
  digitalWrite(PIN_SURUCU_EN,
               k.suruculerAktif ? SURUCU_AKTIF_SEVIYE : !SURUCU_AKTIF_SEVIYE);

  if (k.ileriAdim != 0) ileri.move(k.ileriAdim);
  if (k.giyotinTur) giyotin.move(GIYOTIN_ADIM_TUR);
  if (k.giyotinSifirla) {
    if (sifirda) {
      giyotin.setCurrentPosition(0);
    } else {
      // En fazla 1.2 tur yavaşça dön, sensörü görünce dur.
      giyotin.setMaxSpeed(GIYOTIN_SIFIR_HIZ);
      giyotin.move(GIYOTIN_ADIM_TUR * 12 / 10);
      sifirlamaAktif = true;
    }
  }

  digitalWrite(PIN_UYARI_LED, kontrol.durum() == Durum::HATA ? HIGH : LOW);

  if (motorlarDurgun && k.ileriAdim == 0 && !k.giyotinTur && !sifirlamaAktif) {
    ekranHazirla();
    ekranYaz();
  }
}
