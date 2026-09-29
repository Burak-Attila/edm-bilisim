# Erişte Kesme Makinesi

Soldurulmuş (hafif kurutulmuş) hamur yaprağını alıp üç farklı ölçüde erişte kesen makine:

| # | Ürün            | Genişlik | Uzunluk |
|---|-----------------|----------|---------|
| 1 | 0.5 x 0.5 mm    | 0.5 mm   | 0.5 mm  |
| 2 | 2 x 2 cm        | 20 mm    | 20 mm   |
| 3 | 0.3 mm x 2.5 cm | 0.3 mm   | 25 mm   |

> ⚠️ **Ölçüleri bir daha kontrol et.** Açılmış hamur genelde 0.8–1.5 mm kalınlıktadır.
> 0.5 mm ve 0.3 mm genişlik hamurun kalınlığından ince olur, kesilen parça kare değil
> ince bir çubuk olur. Ayrıca disk bıçakların kendisi 0.3–0.5 mm kalınlıktadır, bu aralıkta
> bıçak dizmek pratikte mümkün değildir. Klasik erişte ölçüleri **5 x 5 mm (0.5 cm)** ve
> **3 mm x 2.5 cm (0.3 cm)** civarındadır. Eğer kastettiğin buysa sadece
> `firmware/eriste_kesme/config.h` içindeki tarif tablosunda 500 → 5000, 300 → 3000 yapman yeter.
> Kaset aralıklarını da buna göre sipariş et.

---

## 1. Çalışma prensibi

İki aşamalı kesim yapılır. Endüstriyel erişte makineleri de bu şekilde çalışır:

```
 hamur yaprağı                                             çıkış
     │   hamur sensörü                                     tepsisi
     ▼       [S]          ┌──────────┐        ║ giyotin       │
 ═══════════════════════  │ ◎  ◎  ◎  │        ║  bıçağı       ▼
  giriş bandı  ───────►   │ dilimleme│  ───►  ║  ───►  ▪ ▪ ▪ ▪ ▪ ▪
 ═══════════════════════  │  kaseti  │        ║
     ▲                    └──────────┘        ▲
  M1: besleme motoru  (bant + kaset          M2: giyotin motoru
      aynı motordan kayışla döner)               (krank-biyel, 1 tur = 1 kesim)
```

1. **Giriş bandı** hamur yaprağını taşır. Sensör yaprağın geldiğini görünce makine otomatik başlar.
2. **Dilimleme kaseti** (iki merdane, üsttekinde disk bıçaklar) yaprağı boyuna şeritlere ayırır.
   Şerit **genişliği** kasetteki bıçak aralığıyla belirlenir. Her ürün için ayrı kaset vardır,
   kaset söküp takılarak değiştirilir.
3. **Giyotin** şeritleri enine keser. Bant her seferinde tam *uzunluk* kadar ilerler, durur,
   bıçak iner ve kalkar. **Uzunluk yazılımla ayarlanır.** Yeni ölçü için mekanik değişiklik gerekmez.
4. Yaprak bitince bant son parçaları çıkış tepsisine döker ve bir sonraki yaprağı bekler.

### Neden böyle?
- Uzunluk tamamen yazılımda olduğu için 0.3 mm'den 50 mm'ye kadar istenen boy kesilebilir.
- Genişlik için kaset kullanılıyor çünkü tek bir hareketli bıçakla boyuna kesmek hem çok yavaş
  olur hem de ince şeritleri dağıtır.
- Kasetlerin üzerinde kod pimi var. **Yanlış kaset takılıysa makine başlamaz** (ör. 2x2 cm tarifi
  seçip 0.5 mm kaseti takmışsan ekranda `YANLIS KASET` yazar).

---

## 2. Mekanik

| Parça | Öneri |
|---|---|
| Şase | 30x30 sigma profil veya 304 paslanmaz kutu profil. Kesim alanı ~350 mm genişlik |
| Giriş bandı | Beyaz PU gıda bandı, 300 mm genişlik, Ø50 mm tahrik merdanesi (tırtıklı veya kauçuk kaplı) |
| Besleme tahriki | NEMA 23 (1.9 Nm) + GT2 20T→60T kayış (1:3). Aynı kayış hattından kaset merdaneleri de döner, **yüzey hızları bantla aynı olmalı** |
| Dilimleme kaseti | Üst mil: paslanmaz disk bıçaklar + araya aralık pulları. Alt mil: POM (delrin) karşı merdane. Kaset iki yan plakaya oturur, iki kelebek vidayla sökülür |
| İnce kasetler (≤ 3 mm) | Disk bıçak yerine **oluklu tarak merdane çifti** (makarna makinesindeki gibi) kullan. İnce aralıkta daha sağlam |
| Giyotin | 304 paslanmaz düz bıçak, lineer rulman üzerinde dikey. NEMA 23 + krank-biyel. Bıçak POM kesme çıtasına iner |
| Bıçak sıfır sensörü | M8 endüktif NPN sensör, krank diskinde bıçak en üstteyken sensörü gören çelik bayrak (±10° pencere) |
| Hamur sensörü | Yansımalı kızılötesi NPN sensör (E18-D80NK gibi), bıçaktan ~80 mm önde |
| Kapak | Şeffaf polikarbon koruma kapağı + NC emniyet switchi. Kapak açılınca makine anında durur |
| Kaset kodu | Yan plakada 2 mikro switch. Kaset gövdesindeki pimler basar: kaset 1 = sol, kaset 2 = sağ, kaset 3 = ikisi |

Gıdaya temas eden her yer **paslanmaz 304, POM veya PU** olmalı. Alüminyum ve boyalı parça kullanma.

### Kapasite (config.h'deki hızlarla: giyotin ~0.6 sn/kesim + bant ilerlemesi)

| Ürün | Ortalama bant hızı | 1 m yaprak |
|---|---|---|
| 2 x 2 cm | ~20 mm/sn | ~50 sn |
| 0.3 mm x 2.5 cm | ~23 mm/sn | ~45 sn |
| 0.5 x 0.5 mm | ~0.8 mm/sn | ~22 dk ⚠️ |

0.5 mm boyda her yarım milimetrede bir kesim gerektiği için giyotin çok yavaş kalır. Bu ürün
gerçekten 0.5 mm olacaksa giyotin yerine **döner bıçaklı enine kesici** (bantla eş zamanlı dönen,
üzerinde bıçak olan silindir) gerekir. Ölçü 5 mm ise giyotin yeterli (~6.5 mm/sn, 1 m yaprak ~2.5 dk).

---

## 3. Elektronik

| Malzeme | Adet |
|---|---|
| Arduino Uno veya Nano | 1 |
| NEMA 23 step motor (1.9 Nm) | 2 |
| Step sürücü TB6600 veya DM542 | 2 |
| 24 V 10 A güç kaynağı (motorlar) | 1 |
| 16x2 I2C LCD (adres 0x27) | 1 |
| Buton (SEC, BASLA, DUR) | 3 |
| Mantar acil stop butonu, **2 kontaklı (NC+NC)** | 1 |
| Kontaktör veya röle (sürücü beslemesini kesmek için) | 1 |
| Emniyet switchi (kapak) | 1 |
| M8 endüktif sensör NPN NO (bıçak sıfır) | 1 |
| Kızılötesi NPN sensör (hamur) | 1 |
| Mikro switch (kaset kodu) | 2 |
| 24V→5V ya da 24V→12V çevirici (sensörler/Arduino) | 1 |

### Bağlantı

| Arduino pini | Bağlantı |
|---|---|
| D2 / D3 | Besleme sürücüsü PUL+ / DIR+ |
| D4 / D5 | Giyotin sürücüsü PUL+ / DIR+ |
| D8 | İki sürücünün ENA+ ucu (ortak) |
| D6 / D7 / D9 | SEC / BASLA / DUR butonları → GND |
| D10 | Acil stop NC kontağı → GND |
| D11 | Kapak switchi NC → GND |
| D12 | Bıçak sıfır sensörü (NPN çıkış) |
| D13 | Hata lambası |
| A0 | Hamur sensörü (NPN çıkış) |
| A1 / A2 | Kaset kod switchleri → GND |
| A4 / A5 | LCD SDA / SCL |

Sürücülerin PUL−, DIR−, ENA− uçları Arduino GND'ye bağlanır.

**Güvenlik:**
- Acil stop'un **bir kontağı** kontaktör üzerinden sürücülerin 24 V beslemesini keser.
  Yazılım çökse bile motorlar durur. **İkinci kontağı** D10'a gider, ekranda `ACIL STOP` yazar.
- Butonlar dışındaki bütün güvenlik girişleri **NC (normalde kapalı)** bağlanır.
  Kablo koparsa makine "acil stop basıldı" diye algılar ve durur.
- NPN sensörler 12–24 V ile beslenir, çıkışları açık kollektördür. Arduino'nun dahili pull-up'ı
  ile doğrudan 5 V lojik verirler. Sensör 5 V'tan yüksek pull-up içeriyorsa araya optokuplör koy.

---

## 4. Yazılım

```
eriste-kesme-makinesi/
├── firmware/eriste_kesme/
│   ├── eriste_kesme.ino   Arduino ana program (pinler, motorlar, LCD)
│   ├── config.h           ← TARİFLER, mekanik ölçüler, pinler (buradan ayarlanır)
│   └── kesim.h            Kontrol mantığı (durum makinesi, adım hesabı)
├── test/test_kesim.cpp    Kontrol mantığının bilgisayarda testi
└── platformio.ini
```

**Yükleme:**
- Arduino IDE: `firmware/eriste_kesme/eriste_kesme.ino` dosyasını aç. Kütüphane Yöneticisi'nden
  *AccelStepper* ve *LiquidCrystal I2C* kütüphanelerini kur, Uno'ya yükle.
- PlatformIO: bu klasörde `pio run -t upload`

**Testler** (bilgisayarda, Arduino gerekmez):
```
cd test
g++ -std=c++17 -Wall -I../firmware/eriste_kesme test_kesim.cpp -o test_kesim && ./test_kesim
```

### Yeni ölçü eklemek
`config.h` → `TARIFLER` tablosuna bir satır ekle. Uzunluk istediğin gibi olabilir. Genişlik için
o aralıkta bir kaset yapıp yeni bir kaset kodu verilir (2 switch ile en fazla 3 kaset;
daha fazlası için bir switch daha eklenir).

### Hassasiyet
Besleme: 200 adım × 16 mikroadım × 1:3 kayış / (π × 50 mm) = **61.1 adım/mm**, yani bir adım
≈ 0.016 mm. 0.3 mm'lik bir kesim ≈ 18 adımdır. Her kesimde adım sayısı yaprağın başından
itibaren toplam mesafeden hesaplandığı için yuvarlama hatası birikmez. 1000 kesimden sonra da
son kesim doğru yerdedir.

---

## 5. Kullanım

1. Doğru kaseti tak, kapağı kapat.
2. **SEC** ile tarifi seç (ekranın üst satırında görünür).
3. **BASLA** → bıçak üst noktasını bulur, ekranda `Hamur ver` yazar.
4. Soldurulmuş yaprağı banda koy. Makine kendiliğinden kesmeye başlar.
   Yaprak bitince parçaları döker ve sıradaki yaprağı bekler (`Y:` = kesilen yaprak sayısı).
5. **DUR** → o anki kesim bitince bıçak yukarıda durur. **BASLA** devam ettirir, tekrar **DUR** iptal eder.

| Ekran | Anlamı / Yapılacak |
|---|---|
| `!ACIL STOP` | Acil stop butonunu çevirip aç, BASLA |
| `!KAPAK ACIK` | Kapağı kapat, BASLA |
| `!KASET YOK` / `!YANLIS KASET` | Kaseti tak ya da SEC ile uygun tarifi seç, BASLA |
| `!GIYOTIN SIFIR?` | Bıçak sensörü görülmedi. Sensör mesafesini ve bayrağı kontrol et |
| `!GIYOTIN SIKISTI` | Kesimden sonra bıçak yukarı dönmedi. Hamur sıkışmış ya da motor adım kaçırmış. Kapağı aç, temizle |

### İlk kurulumda ayarlanacaklar (`config.h`)
1. **`BANT_MERDANE_CAP_MM`**: tahrik merdanesinin çapını kumpasla ölç.
2. **`ILERI_KALIBRASYON`**: banda bir cetvel işareti koy, 1000 mm ilerlet, gerçekte gittiği mesafeyi yaz.
3. **`SENSOR_BICAK_MESAFE_UM`**: hamur sensörünün ışını ile bıçak arasındaki mesafe.
4. **`SURUCU_AKTIF_SEVIYE`**: Motorlar hiç kilitlenmiyorsa bunu `HIGH` yap.
5. Bir motor ters dönüyorsa sürücüdeki A+/A− kablolarını yer değiştir.

Hamur soldurma süresi de sonucu çok etkiler. Çok yaş hamur bıçağa yapışır, çok kuru hamur kırılır.
Bıçağa ve kesme çıtasına ince un serpmek yapışmayı azaltır.
