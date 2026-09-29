// Erişte kesme makinesi - kontrol mantığı
//
// Bu dosya Arduino'ya bağımlı DEĞİLDİR: pin okumaz, motor sürmez.
// Sadece "girdiler -> komutlar" hesabı yapar. Böylece aynı kod hem
// Arduino'da çalışır hem de bilgisayarda test edilebilir (bkz. test/).
//
// Kesim prensibi:
//   1) Soldurulmuş hamur yaprağı giriş bandına konur.
//   2) Dilimleme kaseti (dönen disk bıçaklar) yaprağı boyuna şeritlere
//      ayırır  -> GENİŞLİK  (kasetin bıçak aralığı ile belirlenir)
//   3) Giyotin bıçak şeritleri enine keser -> UZUNLUK (yazılımla ayarlanır:
//      bant her kesimde tam "uzunluk" kadar ilerletilir)

#pragma once
#include <stdint.h>

namespace eriste {

// Bir ürün tarifi. Ölçüler mikrometre (µm) cinsinden tam sayı tutulur,
// böylece 0.3 mm gibi değerler kayan nokta hatası olmadan işlenir.
struct Tarif {
  const char* ad;       // LCD'de görünen isim (en fazla 16 karakter, ASCII)
  int32_t genislik_um;  // boyuna kesim aralığı (dilimleme kaseti)
  int32_t uzunluk_um;   // enine kesim aralığı (giyotin, yazılımla)
  uint8_t kaset;        // bu tarif için takılı olması gereken kaset kodu (1..3)
};

struct MakineAyar {
  // Besleme motorunun milimetre başına adım sayısı x 10000
  // (ör. 61.1155 adım/mm -> 611155). Tam sayı olduğu için birikimli hata yok.
  int64_t ileriAdimPerMm_x10000;
  // Hamur sensöründen giyotin bıçağına kadar olan bant mesafesi.
  int32_t sensorBicakMesafe_um;
  // Yaprak bittikten sonra son parçaları banttan atmak için ekstra ilerleme.
  int32_t cikisMesafe_um;
};

// Toplam mesafeyi (µm) toplam adıma çevirir, en yakın tam sayıya yuvarlar.
// Her kesimde "o ana kadarki toplam mesafe" üzerinden hesap yapıldığı için
// 0.3 mm gibi adımlarda bile yuvarlama hatası birikmez.
inline int32_t mesafeyiAdimaCevir(int64_t um, int64_t adimPerMm_x10000) {
  return (int32_t)((um * adimPerMm_x10000 + 5000000) / 10000000);
}

enum class Durum : uint8_t {
  HAZIR,             // motorlar kapalı, tarif seçilebilir
  SIFIRLANIYOR,      // giyotin üst noktayı (sıfır) arıyor
  HAMUR_BEKLENIYOR,  // bant hazır, yeni yaprak bekleniyor
  ILERLIYOR,         // bant hamuru bir kesim boyu ilerletiyor
  KESIYOR,           // giyotin bir tur atıyor
  DURAKLATILDI,      // DUR basıldı, bıçak yukarıda bekliyor
  HATA,              // motorlar durduruldu, kullanıcı müdahalesi gerekli
};

enum class Hata : uint8_t {
  YOK,
  ACIL_STOP,         // acil stop butonu basılı / hattı kopuk
  KAPAK_ACIK,        // koruma kapağı açık
  KASET_YOK,         // dilimleme kaseti takılı değil
  KASET_UYUMSUZ,     // takılı kaset seçili tarife uymuyor
  SIFIR_BULUNAMADI,  // giyotin üst nokta sensörünü göremedi
  GIYOTIN_KAYDI,     // kesimden sonra bıçak üst noktaya dönmedi (adım kaçırma / sıkışma)
};

struct Girdiler {
  bool acilStop = false;
  bool kapakKapali = true;
  bool hamurVar = false;        // giriş sensörü hamur görüyor
  bool giyotinSifirda = false;  // bıçak üst noktada
  bool hareketBitti = true;     // iki motor da hedefine ulaştı
  uint8_t kasetKodu = 0;        // 0 = kaset yok
  // Buton olayları (basıldığı döngüde bir kez true)
  bool basla = false;
  bool dur = false;
  bool sec = false;
};

struct Komut {
  int32_t ileriAdim = 0;        // besleme motoru bu kadar adım ilerlesin
  bool giyotinTur = false;      // giyotin bir tam tur atsın
  bool giyotinSifirla = false;  // giyotin üst noktayı arasın
  bool hemenDur = false;        // tüm hareketleri anında kes
  bool suruculerAktif = false;  // motor sürücüleri enerjili mi
};

class Kontrolcu {
 public:
  Kontrolcu(const MakineAyar& ayar, const Tarif* tarifler, uint8_t tarifSayisi)
      : ayar_(ayar), tarifler_(tarifler), tarifSayisi_(tarifSayisi) {}

  Komut guncelle(const Girdiler& g) {
    Komut k;

    // --- Her durumda geçerli güvenlik kontrolleri ---
    if (g.acilStop) return hataVer(Hata::ACIL_STOP);
    if (calisiyor()) {
      if (!g.kapakKapali) return hataVer(Hata::KAPAK_ACIK);
      Hata kh = kasetKontrol(g.kasetKodu);
      if (kh != Hata::YOK) return hataVer(kh);
    }

    // Parça boyu kontrolü: yaprağın sonu sensörden geçtiyse bıçağa
    // ulaşacağı noktayı kaydet. Hamur hareketin herhangi bir anında
    // bitmiş olabilir; beslenen_um zaten hareketin hedefini tuttuğu için
    // hesap güvenli tarafta kalır (en fazla bir boş kesim yapılır).
    if ((durum_ == Durum::ILERLIYOR || durum_ == Durum::KESIYOR ||
         durum_ == Durum::DURAKLATILDI) &&
        !g.hamurVar && bitis_um_ < 0) {
      bitis_um_ = beslenen_um_ + ayar_.sensorBicakMesafe_um;
    }

    switch (durum_) {
      case Durum::HAZIR:
      case Durum::HATA:
        if (g.sec) sonrakiTarif();
        if (g.basla) {
          if (!g.kapakKapali) return hataVer(Hata::KAPAK_ACIK);
          Hata kh = kasetKontrol(g.kasetKodu);
          if (kh != Hata::YOK) return hataVer(kh);
          hata_ = Hata::YOK;
          durum_ = Durum::SIFIRLANIYOR;
          k.giyotinSifirla = true;
        }
        break;

      case Durum::SIFIRLANIYOR:
        if (g.hareketBitti) {
          if (!g.giyotinSifirda) return hataVer(Hata::SIFIR_BULUNAMADI);
          durum_ = Durum::HAMUR_BEKLENIYOR;
        }
        break;

      case Durum::HAMUR_BEKLENIYOR:
        if (g.dur) {
          durum_ = Durum::HAZIR;
        } else if (g.hamurVar) {
          // Yeni yaprak: ön kenarı bıçağa getir, sonra baş kırpma kesimi yap.
          beslenen_um_ = 0;
          bitis_um_ = -1;
          cikisModu_ = false;
          durIstendi_ = false;
          ilerle(k, ayar_.sensorBicakMesafe_um);
        }
        break;

      case Durum::ILERLIYOR:
        if (g.dur) durIstendi_ = true;
        if (g.hareketBitti) {
          if (cikisModu_) {
            // Yaprak bitti, parçalar banttan atıldı.
            yaprakSayisi_++;
            durum_ = durIstendi_ ? Durum::HAZIR : Durum::HAMUR_BEKLENIYOR;
            durIstendi_ = false;
          } else {
            durum_ = Durum::KESIYOR;
            k.giyotinTur = true;
          }
        }
        break;

      case Durum::KESIYOR:
        if (g.dur) durIstendi_ = true;
        if (g.hareketBitti) {
          if (!g.giyotinSifirda) return hataVer(Hata::GIYOTIN_KAYDI);
          kesimSayisi_++;
          if (durIstendi_) {
            durIstendi_ = false;
            durum_ = Durum::DURAKLATILDI;
          } else {
            sonrakiAdim(k);
          }
        }
        break;

      case Durum::DURAKLATILDI:
        if (g.basla) {
          sonrakiAdim(k);
        } else if (g.dur) {
          durum_ = Durum::HAZIR;  // yaprağı iptal et
        }
        break;
    }

    k.suruculerAktif = (durum_ != Durum::HAZIR && durum_ != Durum::HATA);
    return k;
  }

  Durum durum() const { return durum_; }
  Hata hata() const { return hata_; }
  const Tarif& tarif() const { return tarifler_[tarifNo_]; }
  uint8_t tarifNo() const { return tarifNo_; }
  uint32_t kesimSayisi() const { return kesimSayisi_; }
  uint32_t yaprakSayisi() const { return yaprakSayisi_; }

 private:
  bool calisiyor() const {
    return durum_ != Durum::HAZIR && durum_ != Durum::HATA;
  }

  Hata kasetKontrol(uint8_t kod) const {
    if (kod == 0) return Hata::KASET_YOK;
    if (kod != tarif().kaset) return Hata::KASET_UYUMSUZ;
    return Hata::YOK;
  }

  void sonrakiTarif() { tarifNo_ = (uint8_t)((tarifNo_ + 1) % tarifSayisi_); }

  // Bir kesim bittikten (veya duraklatmadan dönüldükten) sonra ne yapılacak?
  void sonrakiAdim(Komut& k) {
    if (bitis_um_ >= 0 && beslenen_um_ >= bitis_um_) {
      cikisModu_ = true;
      ilerle(k, ayar_.cikisMesafe_um);
    } else {
      ilerle(k, tarif().uzunluk_um);
    }
  }

  void ilerle(Komut& k, int32_t mesafe_um) {
    int32_t once = mesafeyiAdimaCevir(beslenen_um_, ayar_.ileriAdimPerMm_x10000);
    beslenen_um_ += mesafe_um;
    int32_t sonra = mesafeyiAdimaCevir(beslenen_um_, ayar_.ileriAdimPerMm_x10000);
    k.ileriAdim = sonra - once;
    durum_ = Durum::ILERLIYOR;
  }

  Komut hataVer(Hata h) {
    durum_ = Durum::HATA;
    hata_ = h;
    durIstendi_ = false;
    Komut k;
    k.hemenDur = true;
    k.suruculerAktif = false;
    return k;
  }

  MakineAyar ayar_;
  const Tarif* tarifler_;
  uint8_t tarifSayisi_;
  uint8_t tarifNo_ = 0;

  Durum durum_ = Durum::HAZIR;
  Hata hata_ = Hata::YOK;

  int64_t beslenen_um_ = 0;  // bu yaprak için toplam ilerleme
  int64_t bitis_um_ = -1;    // yaprağın arka kenarının bıçağa ulaştığı nokta
  bool cikisModu_ = false;
  bool durIstendi_ = false;

  uint32_t kesimSayisi_ = 0;
  uint32_t yaprakSayisi_ = 0;
};

inline const char* hataMetni(Hata h) {
  switch (h) {
    case Hata::YOK: return "";
    case Hata::ACIL_STOP: return "ACIL STOP";
    case Hata::KAPAK_ACIK: return "KAPAK ACIK";
    case Hata::KASET_YOK: return "KASET YOK";
    case Hata::KASET_UYUMSUZ: return "YANLIS KASET";
    case Hata::SIFIR_BULUNAMADI: return "GIYOTIN SIFIR?";
    case Hata::GIYOTIN_KAYDI: return "GIYOTIN SIKISTI";
  }
  return "?";
}

}  // namespace eriste
