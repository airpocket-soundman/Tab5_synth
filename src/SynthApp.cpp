#include "SynthApp.h"

#include <M5Unified.h>

#include <array>
#include <cstdlib>
#include <cstdio>

namespace {

constexpr std::uint32_t kUiRefreshIntervalMs = 33;
constexpr int kTouchMarkerThreshold = 12;
constexpr float kParameterChangeThreshold = 0.01f;
constexpr int kKeyboardOctaveShift = 24;
constexpr std::uint32_t kTouchReleaseGuardMs = 12;
constexpr std::uint32_t kPersistedSlotVersion = 2;

bool touchMovedEnough(int current_x, int current_y, int previous_x, int previous_y) {
  return std::abs(current_x - previous_x) >= kTouchMarkerThreshold ||
         std::abs(current_y - previous_y) >= kTouchMarkerThreshold;
}

bool isTouchActive(const m5::touch_detail_t& touch) {
  return touch.isPressed() || touch.wasPressed() || touch.isHolding();
}

UiEffect effectForParameter(UiParameter parameter) {
  switch (parameter) {
    case UiParameter::DelayTime:
    case UiParameter::DelayFeedback:
    case UiParameter::DelayMix:
      return UiEffect::Delay;
    case UiParameter::ChorusRate:
    case UiParameter::ChorusDepth:
    case UiParameter::ChorusMix:
      return UiEffect::Chorus;
    case UiParameter::FilterCutoff:
    case UiParameter::FilterResonance:
    case UiParameter::FilterMix:
      return UiEffect::Filter;
    default:
      return UiEffect::Delay;
  }
}

std::size_t parameterIndex(UiParameter parameter) {
  return static_cast<std::size_t>(parameter);
}

bool isValidUiParameter(std::uint8_t value) {
  return value <= static_cast<std::uint8_t>(UiParameter::FilterMix);
}

bool isValidUiEffect(std::uint8_t value) {
  return value <= static_cast<std::uint8_t>(UiEffect::Filter);
}

bool isValidUiLfoParameter(std::uint8_t value) {
  return value <= static_cast<std::uint8_t>(UiLfoParameter::Shape);
}

const char* slotKey(std::size_t slot_index, char* buffer, std::size_t size) {
  std::snprintf(buffer, size, "slot%u", static_cast<unsigned>(slot_index));
  return buffer;
}

}  // namespace

void SynthApp::begin() {
  ui_.begin();
  audio_engine_.begin();
  preferences_ready_ = preferences_.begin("tab5synth", false);
  for (auto& state : lfo_target_states_) {
    state.rate = ui_state_.lfo_rate;
    state.depth = ui_state_.lfo_depth;
    state.shape = ui_state_.lfo_shape;
    state.enabled = ui_state_.lfo_enabled;
  }
  if (preferences_ready_) {
    const std::size_t slot_count = 5;
    const auto persisted_slot = static_cast<std::size_t>(preferences_.getUChar("active_slot", 0));
    ui_state_.selected_memory_slot = std::min(slot_count - 1, persisted_slot);
    loadSlot(ui_state_.selected_memory_slot);
  }
  audio_engine_.setDelayEnabled(ui_state_.delay_enabled);
  audio_engine_.setDelayParameters(ui_state_.delay_time, ui_state_.delay_feedback, ui_state_.delay_mix);
  syncUiState();
  ui_.drawInitial(ui_state_);
  last_note_input_ms_ = millis();
  last_ui_refresh_ms_ = millis();
}

void SynthApp::update() {
  audio_engine_.update();
  handleTouch();
}

void SynthApp::handleTouch() {
  const std::uint32_t now = millis();
  const std::uint8_t count = M5.Touch.getCount();
  bool mic_button_active = false;

  if (count > 0) {
    for (std::uint8_t i = 0; i < count && i < SynthConfig::ui.max_touch_points; ++i) {
      const auto& touch = M5.Touch.getDetail(i);
      const int x = touch.x;
      const int y = touch.y;
      if (isTouchActive(touch) && ui_.isSelectionArea(x, y) &&
          ui_.sourceAt(x, y, ui_state_.selected_source) == AudioSourceType::OnboardMic) {
        mic_button_active = true;
        break;
      }
    }
  }

  if (mic_recording_) {
    if (mic_button_active) {
      audio_engine_.updateMicSampleRecording();
    } else {
      finishMicRecording();
    }
    return;
  }

  if (count == 0) {
    const bool release_guard_elapsed = (now - last_note_input_ms_) >= kTouchReleaseGuardMs;
    if (release_guard_elapsed && (ui_state_.touch_count > 0 || ui_state_.keyboard_note_count > 0)) {
      stopNote();
    }
    return;
  }

  std::array<int, SynthConfig::ui.max_touch_points> pad_xs{};
  std::array<int, SynthConfig::ui.max_touch_points> pad_ys{};
  std::array<float, SynthConfig::ui.max_touch_points> note_values{};
  std::array<int, SynthConfig::ui.max_touch_points> keyboard_notes{};
  std::size_t pad_count = 0;
  std::size_t note_count = 0;
  std::size_t keyboard_note_count = 0;

  for (std::uint8_t i = 0; i < count && i < SynthConfig::ui.max_touch_points; ++i) {
    const auto& touch = M5.Touch.getDetail(i);
    const int x = touch.x;
    const int y = touch.y;

    if (touch.wasPressed() && ui_.isSelectionArea(x, y)) {
      handleSelectionTouch(x, y);
      return;
    }

    if (touch.wasPressed() && ui_.isParameterArea(x, y)) {
      ui_state_.selected_parameter = ui_.parameterAt(x, y, ui_state_.selected_parameter);
      ui_state_.lfo_edit_mode = false;
      loadLfoTargetState(ui_state_.selected_parameter);
      switch (ui_state_.selected_parameter) {
        case UiParameter::DelayTime:
        case UiParameter::DelayFeedback:
        case UiParameter::DelayMix:
        case UiParameter::ChorusRate:
        case UiParameter::ChorusDepth:
        case UiParameter::ChorusMix:
        case UiParameter::FilterCutoff:
        case UiParameter::FilterResonance:
        case UiParameter::FilterMix:
          ui_state_.selected_effect = effectForParameter(ui_state_.selected_parameter);
          break;
        default:
          break;
      }
      ui_.refreshParameterSelection(ui_state_);
      ui_.refreshParameterControl(ui_state_);
      last_ui_refresh_ms_ = millis();
      return;
    }

    if (touch.wasPressed() && ui_.isEffectArea(x, y)) {
      ui_state_.selected_effect = ui_.effectAt(x, y, ui_state_.selected_effect);
      switch (ui_state_.selected_effect) {
        case UiEffect::Delay:
          ui_state_.delay_enabled = !ui_state_.delay_enabled;
          audio_engine_.setDelayEnabled(ui_state_.delay_enabled);
          break;
        case UiEffect::Chorus:
          ui_state_.chorus_enabled = !ui_state_.chorus_enabled;
          break;
        case UiEffect::Filter:
          ui_state_.filter_enabled = !ui_state_.filter_enabled;
          break;
      }
      ui_.refreshParameterSelection(ui_state_);
      last_ui_refresh_ms_ = millis();
      saveCurrentSlot();
      return;
    }

    if (touch.wasPressed() && ui_.isLfoArea(x, y)) {
      ui_state_.lfo_edit_mode = true;
      ui_state_.selected_lfo_parameter = ui_.lfoAt(x, y, ui_state_.selected_lfo_parameter);
      loadLfoTargetState(ui_state_.selected_parameter);
      ui_.refreshParameterSelection(ui_state_);
      ui_.refreshParameterControl(ui_state_);
      last_ui_refresh_ms_ = millis();
      return;
    }

    if (touch.wasPressed() && ui_.isLfoLabelArea(x, y)) {
      ui_state_.lfo_enabled = !ui_state_.lfo_enabled;
      storeLfoTargetState(ui_state_.selected_parameter);
      ui_.refreshParameterSelection(ui_state_);
      ui_.refreshParameterControl(ui_state_);
      last_ui_refresh_ms_ = millis();
      saveCurrentSlot();
      return;
    }

    if (touch.wasPressed() && ui_.isMemorySlotArea(x, y)) {
      const std::size_t next_slot = ui_.memorySlotAt(x, y, ui_state_.selected_memory_slot);
      if (next_slot != ui_state_.selected_memory_slot) {
        saveCurrentSlot();
        ui_state_.selected_memory_slot = next_slot;
        if (preferences_ready_) {
          preferences_.putUChar("active_slot", static_cast<std::uint8_t>(next_slot));
        }
        loadSlot(next_slot);
        syncUiState();
        ui_.refreshSourceSelection(ui_state_);
      }
      ui_.refreshParameterSelection(ui_state_);
      ui_.refreshParameterControl(ui_state_);
      ui_.refreshPitchMode(ui_state_);
      last_ui_refresh_ms_ = millis();
      return;
    }

    if (touch.wasPressed() && ui_.isPitchModeArea(x, y)) {
      handlePitchModeTouch(x, y);
      return;
    }

    if (isTouchActive(touch) && ui_.isSliderArea(x, y)) {
      handleSliderTouch(x, y);
      return;
    }

    if (isTouchActive(touch) && ui_.isPerformanceArea(x, y)) {
      if (pad_count < SynthConfig::ui.max_touch_points) {
        pad_xs[pad_count] = x;
        pad_ys[pad_count] = y;
        ++pad_count;
      }
      if (note_count < SynthConfig::ui.max_touch_points) {
        note_values[note_count] = ui_.xToNoteValue(x, ui_state_.quantize_to_semitone);
        ++note_count;
      }
      continue;
    }

    if (isTouchActive(touch) && ui_.isKeyboardArea(x, y)) {
      const int keyboard_note = static_cast<int>(std::lround(ui_.keyboardNoteValueAt(x, y)));
      const int shifted_keyboard_note =
          std::clamp(keyboard_note + kKeyboardOctaveShift, SynthConfig::ui.min_midi_note, SynthConfig::ui.max_midi_note);
      if (note_count < SynthConfig::ui.max_touch_points) {
        note_values[note_count] = static_cast<float>(shifted_keyboard_note);
        ++note_count;
      }
      if (keyboard_note_count < SynthConfig::ui.max_touch_points) {
        keyboard_notes[keyboard_note_count] = keyboard_note;
        ++keyboard_note_count;
      }
    }
  }

  if (note_count > 0) {
    last_note_input_ms_ = now;
    handlePerformanceTouches(note_values, note_count, pad_xs, pad_ys, pad_count, keyboard_notes, keyboard_note_count);
  } else if ((now - last_note_input_ms_) >= kTouchReleaseGuardMs &&
             (ui_state_.touch_count > 0 || ui_state_.keyboard_note_count > 0)) {
    stopNote();
  }
}

void SynthApp::handleSelectionTouch(int x, int y) {
  const AudioSourceType next_source = ui_.sourceAt(x, y, ui_state_.selected_source);
  const AudioSourceType previous_source = ui_state_.selected_source;
  const Waveform previous_waveform = ui_state_.selected_waveform;

  if (next_source == AudioSourceType::OnboardMic) {
    stopNote();
    mic_recording_ = audio_engine_.beginMicSampleRecording();
    if (mic_recording_) {
      mic_recording_started_ms_ = millis();
    }
  } else {
    if (next_source == AudioSourceType::Oscillator) {
      ui_state_.selected_waveform = ui_.waveformAt(x, y, ui_state_.selected_waveform);
    }
    audio_engine_.setSourceType(next_source);
  }

  syncUiState();

  if (previous_source != ui_state_.selected_source || previous_waveform != ui_state_.selected_waveform) {
    ui_.refreshSourceSelection(ui_state_);
  }
  ui_.refreshPerformance(ui_state_);
  last_ui_refresh_ms_ = millis();
  saveCurrentSlot();
}

void SynthApp::handlePitchModeTouch(int x, int y) {
  const bool next_quantize = ui_.quantizeModeAt(x, y, ui_state_.quantize_to_semitone);
  if (next_quantize == ui_state_.quantize_to_semitone) {
    return;
  }

  ui_state_.quantize_to_semitone = next_quantize;
  ui_.refreshPitchMode(ui_state_);
  saveCurrentSlot();
}

void SynthApp::handleSliderTouch(int x, int /*y*/) {
  const float next_value = ui_.sliderValueFromTouch(x);

  if (ui_state_.lfo_edit_mode) {
    float* target_value = nullptr;
    switch (ui_state_.selected_lfo_parameter) {
      case UiLfoParameter::Rate:
        target_value = &ui_state_.lfo_rate;
        break;
      case UiLfoParameter::Depth:
        target_value = &ui_state_.lfo_depth;
        break;
      case UiLfoParameter::Shape:
        target_value = &ui_state_.lfo_shape;
        break;
    }
    if (target_value == nullptr || std::abs(next_value - *target_value) < kParameterChangeThreshold) {
      return;
    }
    *target_value = next_value;
    storeLfoTargetState(ui_state_.selected_parameter);
    ui_.refreshParameterControl(ui_state_);
    last_ui_refresh_ms_ = millis();
    saveCurrentSlot();
    return;
  }

  bool updated = false;
  switch (ui_state_.selected_parameter) {
    case UiParameter::Volume:
      if (std::abs(next_value - ui_state_.volume) < kParameterChangeThreshold) {
        return;
      }
      audio_engine_.setVolume(next_value);
      updated = true;
      break;
    case UiParameter::Attack:
      if (std::abs(next_value - ui_state_.attack) < kParameterChangeThreshold) {
        return;
      }
      audio_engine_.setAttackNormalized(next_value);
      updated = true;
      break;
    case UiParameter::Decay:
      if (std::abs(next_value - ui_state_.decay) < kParameterChangeThreshold) {
        return;
      }
      audio_engine_.setDecayNormalized(next_value);
      updated = true;
      break;
    case UiParameter::Sustain:
      if (std::abs(next_value - ui_state_.sustain) < kParameterChangeThreshold) {
        return;
      }
      audio_engine_.setSustainNormalized(next_value);
      updated = true;
      break;
    case UiParameter::Release:
      if (std::abs(next_value - ui_state_.release) < kParameterChangeThreshold) {
        return;
      }
      audio_engine_.setReleaseNormalized(next_value);
      updated = true;
      break;
    case UiParameter::DelayTime:
      if (std::abs(next_value - ui_state_.delay_time) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.delay_time = next_value;
      updated = true;
      break;
    case UiParameter::DelayFeedback:
      if (std::abs(next_value - ui_state_.delay_feedback) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.delay_feedback = next_value;
      updated = true;
      break;
    case UiParameter::DelayMix:
      if (std::abs(next_value - ui_state_.delay_mix) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.delay_mix = next_value;
      updated = true;
      break;
    case UiParameter::ChorusRate:
      if (std::abs(next_value - ui_state_.chorus_rate) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.chorus_rate = next_value;
      updated = true;
      break;
    case UiParameter::ChorusDepth:
      if (std::abs(next_value - ui_state_.chorus_depth) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.chorus_depth = next_value;
      updated = true;
      break;
    case UiParameter::ChorusMix:
      if (std::abs(next_value - ui_state_.chorus_mix) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.chorus_mix = next_value;
      updated = true;
      break;
    case UiParameter::FilterCutoff:
      if (std::abs(next_value - ui_state_.filter_cutoff) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.filter_cutoff = next_value;
      updated = true;
      break;
    case UiParameter::FilterResonance:
      if (std::abs(next_value - ui_state_.filter_resonance) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.filter_resonance = next_value;
      updated = true;
      break;
    case UiParameter::FilterMix:
      if (std::abs(next_value - ui_state_.filter_mix) < kParameterChangeThreshold) {
        return;
      }
      ui_state_.filter_mix = next_value;
      updated = true;
      break;
  }

  if (!updated) {
    return;
  }
  const float delay_time = ui_state_.delay_time;
  const float delay_feedback = ui_state_.delay_feedback;
  const float delay_mix = ui_state_.delay_mix;
  const float chorus_rate = ui_state_.chorus_rate;
  const float chorus_depth = ui_state_.chorus_depth;
  const float chorus_mix = ui_state_.chorus_mix;
  const float filter_cutoff = ui_state_.filter_cutoff;
  const float filter_resonance = ui_state_.filter_resonance;
  const float filter_mix = ui_state_.filter_mix;
  syncUiState();
  ui_state_.delay_time = delay_time;
  ui_state_.delay_feedback = delay_feedback;
  ui_state_.delay_mix = delay_mix;
  ui_state_.chorus_rate = chorus_rate;
  ui_state_.chorus_depth = chorus_depth;
  ui_state_.chorus_mix = chorus_mix;
  ui_state_.filter_cutoff = filter_cutoff;
  ui_state_.filter_resonance = filter_resonance;
  ui_state_.filter_mix = filter_mix;
  audio_engine_.setDelayParameters(ui_state_.delay_time, ui_state_.delay_feedback, ui_state_.delay_mix);
  ui_.refreshParameterControl(ui_state_);
  last_ui_refresh_ms_ = millis();
  saveCurrentSlot();
}

void SynthApp::loadLfoTargetState(UiParameter target) {
  const auto& state = lfo_target_states_[parameterIndex(target)];
  ui_state_.lfo_rate = state.rate;
  ui_state_.lfo_depth = state.depth;
  ui_state_.lfo_shape = state.shape;
  ui_state_.lfo_enabled = state.enabled;
}

void SynthApp::storeLfoTargetState(UiParameter target) {
  auto& state = lfo_target_states_[parameterIndex(target)];
  state.rate = ui_state_.lfo_rate;
  state.depth = ui_state_.lfo_depth;
  state.shape = ui_state_.lfo_shape;
  state.enabled = ui_state_.lfo_enabled;
}

void SynthApp::saveCurrentSlot() {
  if (!preferences_ready_) {
    return;
  }
  PersistedSlot slot{};
  slot.version = kPersistedSlotVersion;
  slot.selected_waveform = static_cast<std::uint8_t>(ui_state_.selected_waveform);
  slot.selected_source = static_cast<std::uint8_t>(ui_state_.selected_source);
  slot.selected_parameter = static_cast<std::uint8_t>(ui_state_.selected_parameter);
  slot.selected_effect = static_cast<std::uint8_t>(ui_state_.selected_effect);
  slot.selected_lfo_parameter = static_cast<std::uint8_t>(ui_state_.selected_lfo_parameter);
  slot.quantize_to_semitone = ui_state_.quantize_to_semitone ? 1 : 0;
  slot.delay_enabled = ui_state_.delay_enabled ? 1 : 0;
  slot.chorus_enabled = ui_state_.chorus_enabled ? 1 : 0;
  slot.filter_enabled = ui_state_.filter_enabled ? 1 : 0;
  slot.volume = ui_state_.volume;
  slot.attack = ui_state_.attack;
  slot.decay = ui_state_.decay;
  slot.sustain = ui_state_.sustain;
  slot.release = ui_state_.release;
  slot.delay_time = ui_state_.delay_time;
  slot.delay_feedback = ui_state_.delay_feedback;
  slot.delay_mix = ui_state_.delay_mix;
  slot.chorus_rate = ui_state_.chorus_rate;
  slot.chorus_depth = ui_state_.chorus_depth;
  slot.chorus_mix = ui_state_.chorus_mix;
  slot.filter_cutoff = ui_state_.filter_cutoff;
  slot.filter_resonance = ui_state_.filter_resonance;
  slot.filter_mix = ui_state_.filter_mix;
  for (std::size_t i = 0; i < kUiParameterCount; ++i) {
    slot.lfo_target_rate[i] = lfo_target_states_[i].rate;
    slot.lfo_target_depth[i] = lfo_target_states_[i].depth;
    slot.lfo_target_shape[i] = lfo_target_states_[i].shape;
    slot.lfo_target_enabled[i] = lfo_target_states_[i].enabled ? 1 : 0;
  }
  char key[12];
  preferences_.putBytes(slotKey(ui_state_.selected_memory_slot, key, sizeof(key)), &slot, sizeof(slot));
  preferences_.putUChar("active_slot", static_cast<std::uint8_t>(ui_state_.selected_memory_slot));
}

void SynthApp::loadSlot(std::size_t slot_index) {
  if (!preferences_ready_) {
    return;
  }

  char key[12];
  const char* slot_name = slotKey(slot_index, key, sizeof(key));
  if (preferences_.getBytesLength(slot_name) != sizeof(PersistedSlot)) {
    saveCurrentSlot();
    return;
  }

  PersistedSlot slot{};
  const std::size_t bytes = preferences_.getBytes(slot_name, &slot, sizeof(slot));
  if (bytes != sizeof(slot) || slot.version != kPersistedSlotVersion) {
    return;
  }

  ui_state_.selected_waveform = static_cast<Waveform>(slot.selected_waveform);
  ui_state_.selected_source = static_cast<AudioSourceType>(slot.selected_source);
  if (isValidUiParameter(slot.selected_parameter)) {
    ui_state_.selected_parameter = static_cast<UiParameter>(slot.selected_parameter);
  }
  if (isValidUiEffect(slot.selected_effect)) {
    ui_state_.selected_effect = static_cast<UiEffect>(slot.selected_effect);
  }
  if (isValidUiLfoParameter(slot.selected_lfo_parameter)) {
    ui_state_.selected_lfo_parameter = static_cast<UiLfoParameter>(slot.selected_lfo_parameter);
  }
  ui_state_.lfo_edit_mode = false;
  ui_state_.quantize_to_semitone = slot.quantize_to_semitone != 0;
  ui_state_.delay_enabled = slot.delay_enabled != 0;
  ui_state_.chorus_enabled = slot.chorus_enabled != 0;
  ui_state_.filter_enabled = slot.filter_enabled != 0;
  ui_state_.volume = slot.volume;
  ui_state_.attack = slot.attack;
  ui_state_.decay = slot.decay;
  ui_state_.sustain = slot.sustain;
  ui_state_.release = slot.release;
  ui_state_.delay_time = slot.delay_time;
  ui_state_.delay_feedback = slot.delay_feedback;
  ui_state_.delay_mix = slot.delay_mix;
  ui_state_.chorus_rate = slot.chorus_rate;
  ui_state_.chorus_depth = slot.chorus_depth;
  ui_state_.chorus_mix = slot.chorus_mix;
  ui_state_.filter_cutoff = slot.filter_cutoff;
  ui_state_.filter_resonance = slot.filter_resonance;
  ui_state_.filter_mix = slot.filter_mix;
  for (std::size_t i = 0; i < kUiParameterCount; ++i) {
    lfo_target_states_[i].rate = slot.lfo_target_rate[i];
    lfo_target_states_[i].depth = slot.lfo_target_depth[i];
    lfo_target_states_[i].shape = slot.lfo_target_shape[i];
    lfo_target_states_[i].enabled = slot.lfo_target_enabled[i] != 0;
  }
  loadLfoTargetState(ui_state_.selected_parameter);

  audio_engine_.setSourceType(ui_state_.selected_source);
  audio_engine_.setVolume(ui_state_.volume);
  audio_engine_.setAttackNormalized(ui_state_.attack);
  audio_engine_.setDecayNormalized(ui_state_.decay);
  audio_engine_.setSustainNormalized(ui_state_.sustain);
  audio_engine_.setReleaseNormalized(ui_state_.release);
  audio_engine_.setDelayEnabled(ui_state_.delay_enabled);
  audio_engine_.setDelayParameters(ui_state_.delay_time, ui_state_.delay_feedback, ui_state_.delay_mix);
}

void SynthApp::handlePerformanceTouches(const std::array<float, SynthConfig::ui.max_touch_points>& note_values,
                                        std::size_t note_count,
                                        const std::array<int, SynthConfig::ui.max_touch_points>& xs,
                                        const std::array<int, SynthConfig::ui.max_touch_points>& ys,
                                        std::size_t pad_count,
                                        const std::array<int, SynthConfig::ui.max_touch_points>& keyboard_notes,
                                        std::size_t keyboard_note_count) {
  bool marker_changed = pad_count != last_drawn_touch_count_;
  for (std::size_t i = 0; i < pad_count && !marker_changed; ++i) {
    marker_changed = touchMovedEnough(xs[i], ys[i], last_drawn_touch_xs_[i], last_drawn_touch_ys_[i]);
  }
  bool keyboard_changed = keyboard_note_count != last_drawn_keyboard_note_count_;
  for (std::size_t i = 0; i < keyboard_note_count && !keyboard_changed; ++i) {
    keyboard_changed = keyboard_notes[i] != last_drawn_keyboard_notes_[i];
  }
  const bool refresh_due = (millis() - last_ui_refresh_ms_) >= kUiRefreshIntervalMs;

  audio_engine_.noteOnVoices(note_values.data(), note_count, ui_state_.selected_waveform);
  ui_state_.touch_count = pad_count;
  ui_state_.touch_xs = xs;
  ui_state_.touch_ys = ys;
  ui_state_.keyboard_note_count = keyboard_note_count;
  ui_state_.keyboard_notes = keyboard_notes;
  syncUiState();
  ui_state_.touch_count = pad_count;
  ui_state_.touch_xs = xs;
  ui_state_.touch_ys = ys;
  ui_state_.keyboard_note_count = keyboard_note_count;
  ui_state_.keyboard_notes = keyboard_notes;

  if (marker_changed || refresh_due) {
    ui_.refreshPerformance(ui_state_);
    last_ui_refresh_ms_ = millis();
    last_drawn_touch_count_ = pad_count;
    last_drawn_touch_xs_ = xs;
    last_drawn_touch_ys_ = ys;
  }
  if (keyboard_changed) {
    ui_.refreshKeyboard(ui_state_);
    last_drawn_keyboard_note_count_ = keyboard_note_count;
    last_drawn_keyboard_notes_ = keyboard_notes;
  }
}

void SynthApp::finishMicRecording() {
  mic_recording_ = false;
  const std::uint32_t held_ms = millis() - mic_recording_started_ms_;
  const bool commit_sample = held_ms >= SynthConfig::audio.mic_sample_commit_min_ms;
  audio_engine_.finishMicSampleRecording(commit_sample);
  mic_recording_started_ms_ = 0;
  syncUiState();
  ui_.refreshSourceSelection(ui_state_);
  ui_.refreshPerformance(ui_state_);
  ui_.refreshKeyboard(ui_state_);
  last_ui_refresh_ms_ = millis();
}

void SynthApp::syncUiState() {
  ui_state_.selected_source = audio_engine_.activeSourceType();
  ui_state_.oscillator_available = audio_engine_.isSourceAvailable(AudioSourceType::Oscillator);
  ui_state_.onboard_mic_available = audio_engine_.isSourceAvailable(AudioSourceType::OnboardMic);
  ui_state_.external_i2s_available = audio_engine_.isSourceAvailable(AudioSourceType::ExternalI2S);
  ui_state_.note_playing = audio_engine_.isNotePlaying();
  ui_state_.active_midi_note = audio_engine_.activeMidiNote();
  ui_state_.active_frequency = audio_engine_.activeFrequency();
  ui_state_.volume = audio_engine_.volume();
  ui_state_.attack = audio_engine_.attackNormalized();
  ui_state_.decay = audio_engine_.decayNormalized();
  ui_state_.sustain = audio_engine_.sustainNormalized();
  ui_state_.release = audio_engine_.releaseNormalized();

  if (!ui_state_.note_playing && ui_state_.touch_count == 0) {
    ui_state_.touch_xs.fill(0);
    ui_state_.touch_ys.fill(0);
  }
}

void SynthApp::stopNote() {
  audio_engine_.noteOff();
  ui_state_.touch_count = 0;
  ui_state_.touch_xs.fill(0);
  ui_state_.touch_ys.fill(0);
  ui_state_.keyboard_note_count = 0;
  ui_state_.keyboard_notes.fill(0);
  syncUiState();
  ui_.refreshPerformance(ui_state_);
  ui_.refreshKeyboard(ui_state_);
  last_ui_refresh_ms_ = millis();
  last_drawn_touch_count_ = 0;
  last_drawn_touch_xs_.fill(0);
  last_drawn_touch_ys_.fill(0);
  last_drawn_keyboard_note_count_ = 0;
  last_drawn_keyboard_notes_.fill(0);
}



