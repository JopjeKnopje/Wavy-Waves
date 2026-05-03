import math
# https://en.wikipedia.org/wiki/Exponential_smoothing
smoothing_factor: float = 0.1

def main1():
    samples: list[float] = []
    for ax in range(8):
        samples.append(math.sin(ax))

    processed_samples: list[float] = []

    for _, s in enumerate(samples):
        ax = s
        ax_prev_len = len(processed_samples)
        if (ax_prev_len == 0):
            ax_prev = 0
        else:
            ax_prev = processed_samples[ax_prev_len - 1]
        processed_samples.append(ax + (1 - smoothing_factor) ** ax_prev)

    print(f"s1: {samples}")
    print(f"s2: {processed_samples}")



def main():

    sample_chunk = 256
    signal: list[float] = [0] * (sample_chunk * 2)
    f = 500
    fs = 44100

    phase_inc = (2 * math.pi * f) / fs
    print(f"phase_inc: {phase_inc}")

    m_phase = 0

    for i in range(sample_chunk, sample_chunk * 2):
        signal[i] = m_phase
        m_phase += phase_inc

    f = 1000
    phase_inc = (2 * math.pi * f) / fs


if __name__ == "__main__":
    main()
