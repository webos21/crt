# libcrtmedia

Media upper runtime layer.

FFmpeg is the first planned reference stack. The initial scope is software
decode into explicit frame/audio-buffer objects; GPU texture and host audio
device handoff should wait until `libcrtgfx` has a stable surface/frame
abstraction.
