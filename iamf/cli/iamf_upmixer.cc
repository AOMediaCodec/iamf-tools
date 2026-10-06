#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace iamf_tools {

// Sunucu dostu, tamamen bağımsız Stereo'dan 7.1'e Upmixer Sınıfı
class StereoUpmixer71 {
 public:
  explicit StereoUpmixer71(float sample_rate) : sample_rate_(sample_rate), write_index_(0) {
    // Sunucu yükünü azaltmak için gecikme havuzlarını başlangıçta bir kez oluşturuyoruz (Lazy allocation yok)
    delay_side_ = static_cast<size_t>(sample_rate_ * 0.020f);
    delay_back_ = static_cast<size_t>(sample_rate_ * 0.040f);
    left_delay_.resize(delay_back_ + 1, 0.0f);
    right_delay_.resize(delay_back_ + 1, 0.0f);
  }

  // Sunucuyu yormayan, bellek kopyalamasız (In-Place / Pointer tabanlı) işlem fonksiyonu
  void Process(const float* input_l, const float* input_r, float** output_71, size_t samples) {
    if (!input_l || !input_r || !output_71) return;

    for (size_t i = 0; i < samples; ++i) {
      float l = input_l[i];
      float r = input_r[i];

      // Mid/Side Ayrıştırması
      float mid = (l + r) * 0.5f;
      float side = (l - r) * 0.5f;

      // 7.1 Kanal Atamaları (Bellek kopyalamadan doğrudan pointer üzerinden yazım)
      output_71[2][i] = mid * 0.707f;               // Center (C)
      output_71[0][i] = (l * 0.8f) + (side * 0.2f); // Left (L)
      output_71[1][i] = (r * 0.8f) - (side * 0.2f); // Right (R)

      // Gecikme Havuzu Güncellemesi (Delay Buffer)
      left_delay_[write_index_] = l;
      right_delay_[write_index_] = r;

      size_t idx_side = (write_index_ + left_delay_.size() - delay_side_) % left_delay_.size();
      size_t idx_back = (write_index_ + left_delay_.size() - delay_back_) % left_delay_.size();

      output_71[4][i] = (left_delay_[idx_side] - right_delay_[idx_side]) * 0.5f; // Side Left (Ls)
      output_71[5][i] = (right_delay_[idx_side] - left_delay_[idx_side]) * 0.5f; // Side Right (Rs)

      output_71[6][i] = left_delay_[idx_back] * 0.4f;  // Back Left (Lb)
      output_71[7][i] = right_delay_[idx_back] * 0.4f; // Back Right (Rb)

      output_71[3][i] = (l + r) * 0.3f; // LFE (Subwoofer)

      write_index_ = (write_index_ + 1) % left_delay_.size();
    }
  }

 private:
  float sample_rate_;
  std::vector<float> left_delay_;
  std::vector<float> right_delay_;
  size_t write_index_;
  size_t delay_side_;
  size_t delay_back_;
};

// DIŞARIYA AÇILAN TEMİZ YARDIMCI FONKSİYON (POST-PROCESSING)
// Bu fonksiyon orijinal decoder çıktısını alıp yukarıdaki upmixer'a besler.
void ApplyStereoTo71Upmix(const float* input_l, const float* input_r, 
                          float** output_71, size_t samples, float sample_rate) {
  // Her çağrıda sıfırdan oluşturulur, sunucuda multi-thread güvenliği sağlar (Thread-safe)
  StereoUpmixer71 upmixer(sample_rate);
  upmixer.Process(input_l, input_r, output_71, samples);
}

}  // namespace iamf_tools
