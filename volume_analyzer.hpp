#pragma once
#include <deque>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>

class VolumeAnalyzer {
public:
    VolumeAnalyzer(size_t window_size = 500) // 10 seconds at 50Hz
        : max_samples(window_size)
        , last_update(std::chrono::steady_clock::now())
        , base_update_interval_ms(20) // Base update rate of 50Hz
    {}

    void AddSample(float left_vol, float right_vol) {
        float max_vol = std::max(left_vol, right_vol);
        samples.push_back(max_vol);

        // If we have enough samples, check for dramatic changes
        if (samples.size() > 50) {
            float recent_avg = GetRecentAverage(50);
            float historical_avg = GetHistoricalAverage();
            float relative_diff = std::abs(recent_avg - historical_avg) / std::max(0.0001f, historical_avg);

            // If there's a dramatic change, clear most of the history
            if (relative_diff > 0.5f) { // More than 50% change
                size_t keep_samples = std::min(size_t(100), samples.size()); // Keep 2 seconds
                while (samples.size() > keep_samples) {
                    samples.pop_front();
                }
            }
        }

        // Normal sample management
        while (samples.size() > max_samples) {
            samples.pop_front();
        }
    }

    float GetRecentAverage(size_t window) const {
        if (samples.empty()) return 0.0f;
        size_t count = std::min(window, samples.size());
        float sum = 0.0f;
        auto it = samples.rbegin();
        for (size_t i = 0; i < count; ++i, ++it) {
            sum += *it;
        }
        return sum / count;
    }

    float GetHistoricalAverage() const {
        if (samples.empty()) return 0.0f;
        float sum = 0.0f;
        for (float sample : samples) {
            sum += sample;
        }
        return sum / samples.size();
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

    // Get suggested thresholds based on stats with current volume context
    std::pair<float, float> GetSuggestedThresholds(float volume_multiplier, float excessive_multiplier) const {
        auto [mean, std_dev] = GetStats();
        float volume_threshold = mean + (std_dev * volume_multiplier);
        float excessive_threshold = mean + (std_dev * excessive_multiplier);
        
        // Lower minimum values for very quiet audio
        volume_threshold = std::max(0.01f, volume_threshold);  // Reduced from 0.05f
        excessive_threshold = std::max(volume_threshold + 0.01f, excessive_threshold);  // Reduced from 0.05f
        
        return {volume_threshold, excessive_threshold};
    }

    bool ShouldUpdate() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
        
        // Calculate current mean
        auto [current_mean, _] = GetStats();
        
        // Calculate mean of last N samples to get recent trend
        size_t recent_window = std::min(size_t(50), samples.size()); // Shorter window - 1 second at 50Hz
        float recent_sum = 0.0f;
        size_t recent_count = 0;
        auto it = samples.rbegin();
        for (size_t i = 0; i < recent_window && it != samples.rend(); ++i, ++it) {
            recent_sum += *it;
            recent_count++;
        }
        float recent_mean = recent_count > 0 ? recent_sum / recent_count : 0.0f;

        // Calculate the relative difference, with explicit handling for very small values
        float relative_diff;
        if (current_mean < 0.0001f) {  // Handle extremely small means
            relative_diff = std::abs(recent_mean - current_mean) * 1000.0f;  // Scale up small differences
        } else {
            relative_diff = std::abs(recent_mean - current_mean) / std::max(0.0001f, current_mean);
        }
        
        float adjustment_factor;
        
        // Special case for very quiet audio (using the recent mean as reference)
        if (recent_mean < 0.01f && current_mean > 0.02f) {
            adjustment_factor = 0.1f;   // 10x faster updates
        }
        // Regular adjustment logic but more aggressive and with lower thresholds
        else if (relative_diff > 0.3f) {  // Over 30% difference
            adjustment_factor = 0.1f;   // 10x faster updates
        } else if (relative_diff > 0.2f) {  // Over 20% difference
            adjustment_factor = 0.2f;   // 5x faster updates
        } else if (relative_diff > 0.1f) {  // Over 10% difference
            adjustment_factor = 0.33f;  // 3x faster updates
        } else if (relative_diff > 0.05f) {  // Over 5% difference
            adjustment_factor = 0.5f;   // 2x faster updates
        } else {
            adjustment_factor = 1.0f;   // Normal speed
        }

        int adjusted_interval = static_cast<int>(base_update_interval_ms * adjustment_factor);
        return elapsed >= adjusted_interval;
    }

    void UpdateTimestamp() {
        last_update = std::chrono::steady_clock::now();
    }

private:
    std::deque<float> samples;
    size_t max_samples;
    std::chrono::steady_clock::time_point last_update;
    const int base_update_interval_ms;
};