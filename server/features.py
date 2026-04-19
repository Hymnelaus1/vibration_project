import numpy as np

def extract_features(samples: list[float]) -> dict:
    """
    Ham ADC örneklerinden makine öğrenmesi için özellikler çıkarır.
    samples: float listesi (voltaj veya raw ADC değerleri)
    """
    x = np.array(samples, dtype=np.float32)

    # RMS — ortalama enerji
    rms = float(np.sqrt(np.mean(x ** 2)))

    # Peak-to-peak — mekanik gevşeme göstergesi
    peak_to_peak = float(np.max(x) - np.min(x))

    # Kurtosis — darbe/şok yoğunluğu (rulman hasarı)
    mean = np.mean(x)
    std  = np.std(x) + 1e-9
    kurtosis = float(np.mean(((x - mean) / std) ** 4))

    # Crest factor — tepe/RMS oranı (çatlak/darbe)
    crest = float(np.max(np.abs(x)) / (rms + 1e-9))

    # FFT dominant frekans enerjisi
    fft_vals = np.abs(np.fft.rfft(x))
    fft_energy = float(np.sum(fft_vals ** 2))
    dominant_freq_idx = int(np.argmax(fft_vals[1:])) + 1  # DC'yi atla

    return {
        "rms":              round(rms, 5),
        "peak_to_peak":     round(peak_to_peak, 5),
        "kurtosis":         round(kurtosis, 4),
        "crest_factor":     round(crest, 4),
        "fft_energy":       round(fft_energy, 2),
        "dominant_freq_idx": dominant_freq_idx
    }