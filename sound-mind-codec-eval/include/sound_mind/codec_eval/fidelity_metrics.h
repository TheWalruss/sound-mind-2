#pragma once

#include <cstdint>
#include <vector>

namespace sound_mind::codec_eval {

/// @brief Ceiling applied to every dB-ratio metric below, so a
///        near-perfect (or exactly identical) pair of signals reports a
///        large, finite number instead of `+inf`/`nan` - keeps the CSV
///        report `writeCsvReport()` produces machine-parseable without a
///        special "infinite" sentinel to special-case downstream.
constexpr float kSnrCapDb = 200.0f;

/// @brief Pearson correlation coefficient between two signals, truncated to
///        the shorter one's length - `1.0` for identical (up to a positive
///        scale factor) signals, `0.0` for uncorrelated/silent ones.
///
/// The same metric `sound-mind-codec`'s own round-trip fidelity tests
/// already use (`test_pool_codec.cpp`/`test_stream_codec.cpp`) - reused
/// here, not reimplemented differently, so a number from this tool means
/// the same thing a developer reading those tests already expects.
///
/// @param a First signal.
/// @param b Second signal.
/// @return `0.0f` if either signal is silent (zero norm).
[[nodiscard]] float correlation(const std::vector<float>& a, const std::vector<float>& b);

/// @brief Root-mean-square level of a signal.
/// @param signal The signal.
/// @return `0.0f` for an empty signal.
[[nodiscard]] float rootMeanSquare(const std::vector<float>& signal);

/// @brief The largest absolute sample value in a signal.
/// @param signal The signal.
/// @return `0.0f` for an empty signal.
[[nodiscard]] float peakAbsolute(const std::vector<float>& signal);

/// @brief Overall time-domain signal-to-noise ratio, in dB, between an
///        original signal and a codec's decoded reconstruction of it.
///
/// Computed as `10*log10(energy(original) / energy(original - decoded))`,
/// both signals truncated to their shorter length first (a codec's decoded
/// length can differ by a few samples from the original - see
/// `sound_mind::codec::decode()`/`poolDecode()`'s own "trimmed to
/// `sampleCount`" docs). The error energy's denominator is floored to avoid
/// dividing by zero for a sample-exact round trip; the result is then
/// capped at `kSnrCapDb`.
///
/// @param original The reference signal.
/// @param decoded The codec's reconstruction of it.
/// @return The SNR in dB, capped at `kSnrCapDb`. `0.0f` if `original` is
///         silent (nothing to measure a ratio against).
[[nodiscard]] float signalToNoiseRatioDb(const std::vector<float>& original, const std::vector<float>& decoded);

/// @brief Per-frequency-band SNR breakdown, for seeing *where* a codec's
///        reconstruction error concentrates rather than only its overall
///        average.
///
/// Band edges (250 Hz, 4000 Hz) are a deliberately simple, round-number
/// split - not a perceptual model (e.g. ISO 226 equal-loudness contours, or
/// the legacy codec's own A-weighting curve - see
/// `docs/sound-mind-codec-design.md`'s "What the codec does not do" for why
/// neither is implemented anywhere in this codec yet) - just enough to
/// distinguish "bass," "midrange," and "treble" reconstruction quality at a
/// glance.
struct BandSnrDb {
    /// @brief SNR below 250 Hz, in dB.
    float lowDb = 0.0f;
    /// @brief SNR from 250 Hz to 4000 Hz, in dB.
    float midDb = 0.0f;
    /// @brief SNR above 4000 Hz, in dB.
    float highDb = 0.0f;
};

/// @brief Computes signalToNoiseRatioDb(), separately, within each of
///        three frequency bands (see BandSnrDb's own docs for the edges).
///
/// Both signals are truncated to their shorter length, then one whole-
/// signal real FFT each (of the original, and of the sample-aligned error
/// `original - decoded`) bins their energy by frequency - the same
/// whole-signal-FFT approach `sound_mind::codec::poolEncode()` already uses
/// for the Pool codec's own transform, reused here for consistency rather
/// than a different (e.g. framed/STFT) energy estimate.
///
/// @param original The reference signal.
/// @param decoded The codec's reconstruction of it.
/// @param sampleRateHz The signals' sample rate, for mapping FFT bins to Hz.
/// @return The three-band breakdown, each capped at `kSnrCapDb`. All zero
///         if `original` is empty or silent (every band's own signal
///         energy is then zero, the same "nothing to measure a ratio
///         against" case signalToNoiseRatioDb() handles).
[[nodiscard]] BandSnrDb bandSignalToNoiseRatioDb(const std::vector<float>& original, const std::vector<float>& decoded,
                                                  std::uint32_t sampleRateHz);

}  // namespace sound_mind::codec_eval
