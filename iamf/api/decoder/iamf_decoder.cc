#include <vector>
#include <cmath>
#include <algorithm>

namespace iamf_tools {

class StereoUpmixer71 {
 public:
  explicit StereoUpmixer71(float sample_rate) : sample_rate_(sample_rate), write_index_(0) {
    delay_side_ = static_cast<size_t>(sample_rate_ * 0.020f);
    delay_back_ = static_cast<size_t>(sample_rate_ * 0.040f);
    left_delay_.resize(delay_back_ + 1, 0.0f);
    right_delay_.resize(delay_back_ + 1, 0.0f);
  }

  void Process(const float* input_l, const float* input_r, float** output_71, size_t samples) {
    if (!input_l || !input_r || !output_71) return;

    for (size_t i = 0; i < samples; ++i) {
      float l = input_l[i];
      float r = input_r[i];

      float mid = (l + r) * 0.5f;
      float side = (l - r) * 0.5f;

      output_71[2][i] = mid * 0.707f; 
      output_71[0][i] = (l * 0.8f) + (side * 0.2f); 
      output_71[1][i] = (r * 0.8f) - (side * 0.2f); 

      left_delay_[write_index_] = l;
      right_delay_[write_index_] = r;

      size_t idx_side = (write_index_ + left_delay_.size() - delay_side_) % left_delay_.size();
      size_t idx_back = (write_index_ + left_delay_.size() - delay_back_) % left_delay_.size();

      output_71[4][i] = (left_delay_[idx_side] - right_delay_[idx_side]) * 0.5f; 
      output_71[5][i] = (right_delay_[idx_side] - left_delay_[idx_side]) * 0.5f; 

      output_71[6][i] = left_delay_[idx_back] * 0.4f;  
      output_71[7][i] = right_delay_[idx_back] * 0.4f; 

      output_71[3][i] = (l + r) * 0.3f; 

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

void ApplyStereoTo71Upmix(const float* input_l, const float* input_r, 
                         float** output_71, size_t samples, float sample_rate,
                         int src_channels, int target_channels) {
  if (src_channels == 2 && target_channels == 8) {
    static StereoUpmixer71 upmixer(sample_rate);
    upmixer.Process(input_l, input_r, output_71, samples);
  }
}

}  // namespace iamf_tools
