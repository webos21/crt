// Web Tranche 1D-D: exercise JavaScriptCore's signal-based sampling profiler
// on the main VM and on four concurrent worker VMs. This runs only in the
// separate ENABLE_SAMPLING_PROFILER=ON build made by tools/build_webkit_jsc.py.

function check(condition, message) {
    if (!condition)
        throw new Error(message);
}

function profileHotLoop(deadline) {
    let value = 1;
    while ($262.agent.monotonicNow() < deadline) {
        for (let i = 0; i < 20000; ++i)
            value = ((value * 1664525 + 1013904223) ^ i) | 0;
    }
    return value;
}
noInline(profileHotLoop);

function takeProfile(milliseconds) {
    check(platformSupportsSamplingProfiler(), "profiler was not compiled in");
    startSamplingProfiler();
    profileHotLoop($262.agent.monotonicNow() + milliseconds);
    const profile = samplingProfilerStackTraces();
    check(profile && profile.interval > 0, "missing profiler interval");
    check(Array.isArray(profile.traces) && profile.traces.length > 0, "no stack traces collected");

    let hotFrames = 0;
    const categories = new Set();
    for (const trace of profile.traces) {
        check(Array.isArray(trace.frames), "trace has no frames");
        for (const frame of trace.frames) {
            if (frame.category)
                categories.add(frame.category);
            if (frame.name === "profileHotLoop" || (frame.inliner && frame.inliner.name === "profileHotLoop"))
                ++hotFrames;
        }
    }
    check(hotFrames > 0, "the hot JavaScript function was never sampled");
    return { traces: profile.traces.length, hotFrames, categories: Array.from(categories).sort() };
}

function workerMain() {
    function check(condition, message) {
        if (!condition)
            throw new Error(message);
    }
    function profileHotLoop(deadline) {
        let value = 1;
        while ($262.agent.monotonicNow() < deadline) {
            for (let i = 0; i < 20000; ++i)
                value = ((value * 1664525 + 1013904223) ^ i) | 0;
        }
        return value;
    }
    noInline(profileHotLoop);
    check(platformSupportsSamplingProfiler(), "worker profiler was not compiled in");
    startSamplingProfiler();
    profileHotLoop($262.agent.monotonicNow() + 300);
    const profile = samplingProfilerStackTraces();
    let hotFrames = 0;
    for (const trace of profile.traces || []) {
        for (const frame of trace.frames || []) {
            if (frame.name === "profileHotLoop" || (frame.inliner && frame.inliner.name === "profileHotLoop"))
                ++hotFrames;
        }
    }
    check(profile.traces.length > 0 && hotFrames > 0, "worker collected no usable samples");
    $262.agent.report(JSON.stringify({ traces: profile.traces.length, hotFrames }));
}

check(platformSupportsSamplingProfiler(), "platform_support=false");
const workerSource = "(" + workerMain.toString() + ")()";
for (let i = 0; i < 4; ++i)
    $262.agent.start(workerSource, "sampling-profiler-worker-" + i + ".js");

const main = takeProfile(300);
let traces = main.traces;
let hotFrames = main.hotFrames;
for (let i = 0; i < 4; ++i) {
    const report = JSON.parse(waitForReport());
    traces += report.traces;
    hotFrames += report.hotFrames;
}

print("platform_support=true");
print("jsc_sampling_profiler_acceptance: ok profiles=5 traces=" + traces
    + " hot_frames=" + hotFrames + " categories=" + main.categories.join(","));
