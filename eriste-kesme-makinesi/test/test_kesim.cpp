// Kontrol mantığının bilgisayarda testi (Arduino gerekmez).
//   g++ -std=c++17 -Wall -Wextra -I../firmware/eriste_kesme test_kesim.cpp -o test_kesim && ./test_kesim

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include "config.h"
#include "kesim.h"

using namespace eriste;

static int hataSayisi = 0;
#define KONTROL(kosul)                                                    \
  do {                                                                    \
    if (!(kosul)) {                                                       \
      std::printf("  BASARISIZ %s:%d: %s\n", __FILE__, __LINE__, #kosul); \
      hataSayisi++;                                                       \
    }                                                                     \
  } while (0)

static const MakineAyar AYAR = {ILERI_ADIM_PER_MM_X10000,
                                SENSOR_BICAK_MESAFE_UM, CIKIS_MESAFE_UM};

// Basit makine simülasyonu: komutları uygular, sensörleri üretir.
struct Makine {
  Kontrolcu k{AYAR, TARIFLER, TARIF_SAYISI};
  Girdiler g;
  int64_t toplamAdim = 0;      // besleme motorunun attığı toplam adım
  int64_t yaprakBasiAdim = 0;  // hamurun ön kenarı sensöre geldiğinde
  int64_t yaprakBoyuAdim = -1; // -1 = hamur yok
  bool giyotinSikisik = false;
  bool sifirSensoruBozuk = false;
  int giyotinTurSayisi = 0;

  void yaprakKoy(double boy_mm) {
    yaprakBasiAdim = toplamAdim;
    yaprakBoyuAdim = mesafeyiAdimaCevir((int64_t)(boy_mm * 1000), AYAR.ileriAdimPerMm_x10000);
  }

  Komut adim() {
    g.hamurVar = yaprakBoyuAdim >= 0 && (toplamAdim - yaprakBasiAdim) < yaprakBoyuAdim;
    Komut c = k.guncelle(g);
    g.basla = g.dur = g.sec = false;
    // Hareketleri tek döngüde tamamlanmış say.
    toplamAdim += c.ileriAdim;
    if (c.giyotinTur) {
      giyotinTurSayisi++;
      g.giyotinSifirda = !giyotinSikisik;
    }
    if (c.giyotinSifirla) g.giyotinSifirda = !sifirSensoruBozuk;
    g.hareketBitti = true;
    return c;
  }

  void calistir(int dongu) {
    for (int i = 0; i < dongu; i++) adim();
  }

  void hazirla(uint8_t tarif) {
    while (k.tarifNo() != tarif) {
      g.sec = true;
      adim();
    }
    g.kasetKodu = TARIFLER[tarif].kaset;
    g.basla = true;
    adim();  // SIFIRLANIYOR
    adim();  // HAMUR_BEKLENIYOR
  }
};

static void test_adim_hesabi_birikmez() {
  std::printf("adim hesabi: yuvarlama hatasi birikmiyor\n");
  const int64_t K = AYAR.ileriAdimPerMm_x10000;
  KONTROL(K == 611155);  // 200*16*3 / (pi*50) = 61.1155 adım/mm
  for (int32_t boy : {300, 500, 20000, 25000}) {
    int64_t toplam = 0, konum = 0;
    for (int i = 0; i < 10000; i++) {
      int32_t once = mesafeyiAdimaCevir(konum, K);
      konum += boy;
      toplam += mesafeyiAdimaCevir(konum, K) - once;
    }
    KONTROL(toplam == mesafeyiAdimaCevir(konum, K));
  }
  // 0.3 mm ~ 18 adım: tek adım 0.016 mm -> yeterli çözünürlük
  KONTROL(mesafeyiAdimaCevir(300, K) == 18);
}

static void test_tam_yaprak(uint8_t tarifNo, double yaprak_mm) {
  const Tarif& t = TARIFLER[tarifNo];
  std::printf("tam yaprak: %s, %.0f mm yaprak\n", t.ad, yaprak_mm);
  Makine m;
  m.hazirla(tarifNo);
  KONTROL(m.k.durum() == Durum::HAMUR_BEKLENIYOR);

  m.yaprakKoy(yaprak_mm);
  for (int i = 0; i < 2000000 && m.k.yaprakSayisi() == 0; i++) m.adim();
  KONTROL(m.k.yaprakSayisi() == 1);
  KONTROL(m.k.durum() == Durum::HAMUR_BEKLENIYOR);

  // Kesim sayısı: baş kırpma + yaprak boyunca her "uzunluk"ta bir kesim.
  double beklenen = 1 + yaprak_mm * 1000.0 / t.uzunluk_um;
  KONTROL(m.k.kesimSayisi() >= beklenen - 1);
  KONTROL(m.k.kesimSayisi() <= beklenen + 2);

  // Bant, yaprağın arka kenarını bıçağın ötesine taşımış olmalı.
  double gidilen_mm = (double)(m.toplamAdim - m.yaprakBasiAdim) / (AYAR.ileriAdimPerMm_x10000 / 10000.0);
  KONTROL(gidilen_mm >= yaprak_mm + SENSOR_BICAK_MESAFE_UM / 1000.0);
}

static void test_kaset_kontrolu() {
  std::printf("yanlis / eksik kaset ile baslamaz\n");
  Makine m;
  m.g.kasetKodu = 0;
  m.g.basla = true;
  m.adim();
  KONTROL(m.k.durum() == Durum::HATA && m.k.hata() == Hata::KASET_YOK);

  m.g.kasetKodu = 2;  // tarif 1 (kaset 1) seçiliyken kaset 2 takılı
  m.g.basla = true;
  m.adim();
  KONTROL(m.k.hata() == Hata::KASET_UYUMSUZ);

  m.g.sec = true;  // tarif 2'ye geç -> kaset uyar
  m.adim();
  m.g.basla = true;
  m.adim();
  KONTROL(m.k.durum() == Durum::SIFIRLANIYOR);
  KONTROL(m.k.hata() == Hata::YOK);
}

static void test_kapak_acilinca_durur() {
  std::printf("kesim sirasinda kapak acilinca aninda durur\n");
  Makine m;
  m.hazirla(1);
  m.yaprakKoy(500);
  m.calistir(10);
  KONTROL(m.k.durum() == Durum::ILERLIYOR || m.k.durum() == Durum::KESIYOR);
  m.g.kapakKapali = false;
  Komut c = m.adim();
  KONTROL(c.hemenDur && !c.suruculerAktif);
  KONTROL(m.k.hata() == Hata::KAPAK_ACIK);
  // Kapak açıkken BASLA işe yaramaz
  m.g.basla = true;
  m.adim();
  KONTROL(m.k.durum() == Durum::HATA);
  // Kapak kapanınca BASLA ile önce bıçak sıfırlanır
  m.g.kapakKapali = true;
  m.g.basla = true;
  c = m.adim();
  KONTROL(c.giyotinSifirla);
}

static void test_acil_stop() {
  std::printf("acil stop her durumda hata verir\n");
  Makine m;
  m.hazirla(0);
  m.yaprakKoy(50);
  m.calistir(5);
  m.g.acilStop = true;
  Komut c = m.adim();
  KONTROL(c.hemenDur);
  KONTROL(m.k.hata() == Hata::ACIL_STOP);
  m.g.basla = true;
  m.adim();
  KONTROL(m.k.durum() == Durum::HATA);  // acil stop basılıyken başlamaz
}

static void test_duraklat_devam() {
  std::printf("DUR: bicak yukarida durur, BASLA ile devam eder\n");
  Makine m;
  m.hazirla(1);
  m.yaprakKoy(300);
  m.calistir(6);
  m.g.dur = true;
  m.calistir(4);
  KONTROL(m.k.durum() == Durum::DURAKLATILDI);
  KONTROL(m.g.giyotinSifirda);
  uint32_t kesim = m.k.kesimSayisi();
  m.calistir(10);
  KONTROL(m.k.kesimSayisi() == kesim);  // beklerken kesmez
  m.g.basla = true;
  m.calistir(200);
  KONTROL(m.k.yaprakSayisi() == 1);
  KONTROL(m.k.durum() == Durum::HAMUR_BEKLENIYOR);
}

static void test_giyotin_sikisma() {
  std::printf("giyotin sikisirsa / sensor bozuksa hata\n");
  Makine m;
  m.hazirla(2);
  m.yaprakKoy(200);
  m.giyotinSikisik = true;
  m.calistir(10);
  KONTROL(m.k.durum() == Durum::HATA && m.k.hata() == Hata::GIYOTIN_KAYDI);

  Makine m2;
  m2.sifirSensoruBozuk = true;
  m2.g.giyotinSifirda = false;
  m2.hazirla(0);
  KONTROL(m2.k.hata() == Hata::SIFIR_BULUNAMADI);
}

int main() {
  test_adim_hesabi_birikmez();
  test_tam_yaprak(0, 100);   // 0.5 x 0.5 mm
  test_tam_yaprak(1, 500);   // 2 x 2 cm
  test_tam_yaprak(2, 400);   // 0.3 mm x 2.5 cm
  test_kaset_kontrolu();
  test_kapak_acilinca_durur();
  test_acil_stop();
  test_duraklat_devam();
  test_giyotin_sikisma();
  if (hataSayisi) {
    std::printf("\n%d kontrol BASARISIZ\n", hataSayisi);
    return 1;
  }
  std::printf("\nTum testler gecti.\n");
  return 0;
}
