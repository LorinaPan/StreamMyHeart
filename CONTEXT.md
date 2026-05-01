# StreamMyHeart Context

## Domain Concepts

- Heart-rate monitor module: the OBS filter users attach to a video capture device.
- Signal estimation pipeline: the runtime flow that turns face-region RGB samples into a BPM estimate.
- Display scene module: the auxiliary OBS sources used to render heart rate text, graph, mood, and ECG output.
- Face detection adapter: an adapter that extracts face-region RGB samples from a frame using a concrete detection approach.
