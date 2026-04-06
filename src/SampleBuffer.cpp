#include "SampleBuffer.h"

#include <algorithm>
#include <cstring>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

SampleBuffer::~SampleBuffer() {
#if defined(ESP_PLATFORM)
  if (data_ != nullptr) {
    heap_caps_free(data_);
  }
#else
  delete[] data_;
#endif
}

bool SampleBuffer::allocate(std::size_t sample_count) {
  if (data_ != nullptr && capacity_samples_ >= sample_count) {
    clear();
    return true;
  }

#if defined(ESP_PLATFORM)
  if (data_ != nullptr) {
    heap_caps_free(data_);
  }
  data_ = static_cast<std::int16_t*>(
      heap_caps_malloc(sample_count * sizeof(std::int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  delete[] data_;
  data_ = new std::int16_t[sample_count];
#endif

  if (data_ == nullptr) {
    capacity_samples_ = 0;
    length_samples_ = 0;
    return false;
  }

  capacity_samples_ = sample_count;
  clear();
  return true;
}

void SampleBuffer::clear() {
  length_samples_ = 0;
  if (data_ != nullptr && capacity_samples_ > 0) {
    std::memset(data_, 0, capacity_samples_ * sizeof(std::int16_t));
  }
}

bool SampleBuffer::isAllocated() const {
  return data_ != nullptr;
}

bool SampleBuffer::hasData() const {
  return data_ != nullptr && length_samples_ > 0;
}

std::int16_t* SampleBuffer::data() {
  return data_;
}

const std::int16_t* SampleBuffer::data() const {
  return data_;
}

std::size_t SampleBuffer::capacitySamples() const {
  return capacity_samples_;
}

std::size_t SampleBuffer::lengthSamples() const {
  return length_samples_;
}

void SampleBuffer::setLengthSamples(std::size_t sample_count) {
  length_samples_ = std::min(sample_count, capacity_samples_);
}
