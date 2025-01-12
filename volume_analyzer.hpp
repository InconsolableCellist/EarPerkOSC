#pragma once
#include <deque>
#include <chrono>
#include <cmath>

class VolumeAnalyzer {
public:
    VolumeAnalyzer(size_t window_size = 3000) // 1 minute at 50Hz sampling
        : max_samples(window_size)
        , last_update(std::chrono::steady_clock::now()) {}

    void AddSample(float left_vol, float right_vol) {
        float max_vol = std::max(left_vol, right_vol);
        samples.push_back(max_vol);
        if (samples.size() > max_samples) {
            samples.pop_front();
        }
    }

    // Calculate mean and standard deviation
    std::pair<float, float> GetStats() const {
        if (samples.empty()) return {0.0f, 0.0f};

        // Calculate mean
        float sum = 0.0f;
        for (float sample : samples) {
            sum += sample;
        }
        float mean = sum / samples.size();

        // Calculate standard deviation
        float variance_sum = 0.0f;
        for (float sample : samples) {
            float diff = sample - mean;
            variance_sum += diff * diff;
        }
        float std_dev = std::sqrt(variance_sum / samples.size());

        return {mean, std_dev};
    }

    // Get suggested thresholds based on stats
    std::pair<float, float> GetSuggestedThresholds(float volume_multiplier, float excessive_multiplier) const {
        auto [mean, std_dev] = GetStats();
        float volume_threshold = mean + (std_dev * volume_multiplier);
        float excessive_threshold = mean + (std_dev * excessive_multiplier);
        
        // Ensure minimum values and proper ordering
        volume_threshold = std::max(0.05f, volume_threshold);
        excessive_threshold = std::max(volume_threshold + 0.05f, excessive_threshold);
        
        return {volume_threshold, excessive_threshold};
    }

    bool ShouldUpdate() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
        return elapsed >= 20; // Update at 50Hz
    }

    void UpdateTimestamp() {
        last_update = std::chrono::steady_clock::now();
    }

private:
    std::deque<float> samples;
    size_t max_samples;
    std::chrono::steady_clock::time_point last_update;
};