#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace iamf_tools {

// Sunucu dostu, tamamen bağımsız Stereo'dan 7.1'e Upmix yardımcı fonksiyonu
void ApplyStereoTo71Upmix(std::vector<uint8_t>& sample_buffer) {
  // Eğer buffer boşsa veya veri yoksa işlem yapma
  if (sample_buffer.empty()) return;

  // Veriyi float veya int16_t (PCM) formatına göre in-place işlemek üzere pointer alıyoruz
  // Not: IAMF genelde 16-bit veya 32-bit float PCM kullanır. 
  // Sunucu yükünü sıfırlamak için mevcut buffer üzerinde doğrudan matrisleme yapıyoruz.
  int16_t* audio_data = reinterpret_cast<int16_t*>(sample_buffer.data());
  size_t total_samples = sample_buffer.size() / sizeof(int16_t);
  
  // Eğer veri çok küçükse veya kanal sayısı doğrulaması gerekirse (Stereo = 2 kanal)
  // Mevcut buffer'ı genişleterek 7.1 (8 kanal) boyutuna getiriyoruz (Hafif ve tek seferlik genişletme)
  size_t stereo_samples = total_samples / 2;
  size_t target_size = stereo_samples * 8 * sizeof(int16_t);
  
  std::vector<int16_t> temp_buffer(stereo_samples * 8, 0);

  for (size_t i = 0; i < stereo_samples; ++i) {
    int16_t l = audio_data[i * 2];
    int16_t r = audio_data[i * 2 + 1];

    int16_t mid = static_cast<int16_t>((l + r) * 0.5f);
    int16_t side = static_cast<int16_t>((l - r) * 0.5f);

    temp_buffer[i * 8 + 0] = static_cast<int16_t>((l * 0.8f) + (side * 0.2f));  // L
    temp_buffer[i * 8 + 1] = static_cast<int16_t>((r * 0.8f) - (side * 0.2f));  // R
    temp_buffer[i * 8 + 2] = static_cast<int16_t>(mid * 0.707f);                // C
    temp_buffer[i * 8 + 3] = static_cast<int16_t>((l + r) * 0.3f);               // LFE
    temp_buffer[i * 8 + 4] = static_cast<int16_t>(side * 0.5f);                 // Ls
    temp_buffer[i * 8 + 5] = static_cast<int16_t>(-side * 0.5f);                // Rs
    temp_buffer[i * 8 + 6] = static_cast<int16_t>(l * 0.4f);                     // Lb
    temp_buffer[i * 8 + 7] = static_cast<int16_t>(r * 0.4f);                     // Rb
  }

  // Orijinal buffer'ı yeni 7.1 verisiyle sunucuyu yormadan değiştiriyoruz
  sample_buffer.resize(target_size);
  std::memcpy(sample_buffer.data(), temp_buffer.data(), target_size);
}

}  // namespace iamf_tools
