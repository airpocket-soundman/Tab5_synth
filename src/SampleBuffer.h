#pragma once

#include <cstddef>
#include <cstdint>

class SampleBuffer {
 public:
  ~SampleBuffer();

  bool allocate(std::size_t sample_count);
  void clear();

  [[nodiscard]] bool isAllocated() const;
  [[nodiscard]] bool hasData() const;
  [[nodiscard]] std::int16_t* data();
  [[nodiscard]] const std::int16_t* data() const;
  [[nodiscard]] std::size_t capacitySamples() const;
  [[nodiscard]] std::size_t lengthSamples() const;
  void setLengthSamples(std::size_t sample_count);

 private:
  std::int16_t* data_ = nullptr;
  std::size_t capacity_samples_ = 0;
  std::size_t length_samples_ = 0;
};
