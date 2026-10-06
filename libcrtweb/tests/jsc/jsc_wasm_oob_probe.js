// Out-of-bounds Wasm loads, nothing else: the harness runs this under strace (Linux) to count
// the signals the process handled. With useWasmFastMemory=true each trap is a SIGSEGV that JSC's
// fault handler turns into a WebAssembly.RuntimeError; with it false the bounds check is in code.
var bytes = new Uint8Array([0,97,115,109,1,0,0,0, 1,6,1,96,1,127,1,127, 3,2,1,0, 5,3,1,0,1, 7,8,1,4,108,111,97,100,0,0, 10,9,1,7,0,32,0,40,2,0,11]);
var load = new WebAssembly.Instance(new WebAssembly.Module(bytes)).exports.load;
var traps = 0;
for (var i = 0; i < 50; i++) { try { load(65536 + i); } catch (e) { if (e instanceof WebAssembly.RuntimeError) traps++; } }
if (traps !== 50) throw new Error("expected 50 RuntimeErrors, got " + traps);
print("jsc_wasm_oob_probe: ok traps=" + traps);
