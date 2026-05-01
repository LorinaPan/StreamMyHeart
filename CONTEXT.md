# StreamMyHeart Context

## Domain Concepts

- Heart-rate monitor module: the OBS filter users attach to a video capture device.
- Signal estimation pipeline: the runtime flow that turns face-region RGB samples into a BPM estimate.
- Display scene module: the auxiliary OBS sources used to render heart rate text, graph, mood, and ECG output.
- Face detection adapter: an adapter that extracts face-region RGB samples from a frame using a concrete detection approach.
- Render cadence: how often OBS asks the plugin to render video output.
- Analysis cadence: how often the plugin runs vision and signal estimation work to update heart-rate state.
- Analysis worker: dedicated asynchronous worker owned by Heart-rate monitor module that processes latest captured frame and publishes heart-rate state.
- Analysis result snapshot: single coherent published heart-rate state read by render path as one unit.
