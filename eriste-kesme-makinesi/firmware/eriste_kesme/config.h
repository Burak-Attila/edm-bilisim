// Erişte kesme makinesi - ayarlar
// Makineyi kurduktan sonra değiştirmen gereken her şey bu dosyada.

#pragma once
#include "kesim.h"

// ---------------------------------------------------------------------------
// TARİFLER
// genislik = dilimleme kasetinin bıçak aralığı (mekanik, kaset değişir)
// uzunluk  = giyotinin kesim aralığı (yazılım, istediğin değeri yaz)
// Ölçüler mikrometre: 1 mm = 1000, 1 cm = 10000.
// ---------------------------------------------------------------------------
static const eriste::Tarif TARIFLER[] = {
  //  LCD adı          genişlik  uzunluk  kaset
  { "0.5 x 0.5 mm",       500,     500,    1 },
  { "2 x 2 cm",         20000,   20000,    2 },
  { "0.3mm x 2.5cm",      300,   25000,    3 },
};
static const uint8_t TARIF_SAYISI = sizeof(TARIFLER) / sizeof(TARIFLER[0]);

// ---------------------------------------------------------------------------
// MEKANİK
// ---------------------------------------------------------------------------
// Besleme (bant + dilimleme merdaneleri aynı motordan, kayışla)
#define ILERI_MOTOR_ADIM_TUR   200.0   // 1.8° step motor
#define ILERI_MIKROADIM        16.0    // sürücüdeki DIP switch ayarı
#define ILERI_REDUKSIYON       3.0     // GT2 kasnak oranı (20T -> 60T)
#define BANT_MERDANE_CAP_MM    50.0    // tahrik merdanesinin dış çapı
// Kalibrasyon: 1000 mm ilerlet, bandın gerçekte gittiği mesafeyi ölç ve
// ilk sayıya yaz. Ör. 992 mm gittiyse: (992.0 / 1000.0)
#define ILERI_KALIBRASYON      (1000.0 / 1000.0)

// adım/mm x 10000 (derleme sırasında hesaplanır)
#define ILERI_ADIM_PER_MM_X10000                                           \
  ((int64_t)(ILERI_MOTOR_ADIM_TUR * ILERI_MIKROADIM * ILERI_REDUKSIYON /   \
             (3.14159265 * BANT_MERDANE_CAP_MM) / ILERI_KALIBRASYON *      \
             10000.0 + 0.5))

// Giyotin (krank-biyel, 1 tur = 1 kesim)
#define GIYOTIN_ADIM_TUR       (200L * 8L)  // 8 mikroadım
#define GIYOTIN_HIZ            3200.0       // adım/sn  (2 tur/sn = 2 kesim/sn)
#define GIYOTIN_IVME           30000.0      // adım/sn²
#define GIYOTIN_SIFIR_HIZ      400.0        // sıfır ararken yavaş

#define ILERI_HIZ              4000.0       // adım/sn (~65 mm/sn; Uno için üst sınır)
#define ILERI_IVME             40000.0      // adım/sn²

// Hamur sensöründen giyotin bıçağına kadar olan mesafe (makinede ölç!)
#define SENSOR_BICAK_MESAFE_UM 80000L       // 80 mm
// Yaprak bitince son parçaları banttan atmak için ekstra ilerleme
#define CIKIS_MESAFE_UM        60000L       // 60 mm

// ---------------------------------------------------------------------------
// PİNLER (Arduino Uno / Nano)
// ---------------------------------------------------------------------------
#define PIN_ILERI_STEP     2
#define PIN_ILERI_DIR      3
#define PIN_GIYOTIN_STEP   4
#define PIN_GIYOTIN_DIR    5
#define PIN_BTN_SEC        6
#define PIN_BTN_BASLA      7
#define PIN_SURUCU_EN      8   // iki sürücünün ENA+ ucu ortak
#define PIN_BTN_DUR        9
#define PIN_ACIL_STOP      10  // NC kontak -> GND, basılınca açılır
#define PIN_KAPAK          11  // NC emniyet switchi -> GND, kapak açılınca açılır
#define PIN_GIYOTIN_SIFIR  12  // NPN endüktif sensör, bıçak yukarıdayken LOW
#define PIN_UYARI_LED      13
#define PIN_HAMUR_SENSOR   A0  // NPN kızılötesi sensör, hamur varken LOW
#define PIN_KASET_B0       A1  // kaset kodlama switchleri (GND'ye çeker)
#define PIN_KASET_B1       A2
// A4 = SDA, A5 = SCL -> I2C LCD

// Sürücü ENA girişi: TB6600/DM542'de ENA aktifken motor BOŞA ALINIR.
// Bu yüzden "sürücü çalışsın" = LOW. Sürücün tersini istiyorsa HIGH yap.
#define SURUCU_AKTIF_SEVIYE LOW

#define LCD_ADRES 0x27
#define BUTON_BEKLEME_MS 30
#define HAMUR_BEKLEME_MS 50
