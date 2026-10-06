// WebAssembly acceptance (Web Tranche 1D-C). The modules are assembled here from raw bytes
// (no wat2wasm dependency) and exercise the Wasm interpreter, BBQ and OMG tiers depending on
// the JSC options the harness sets; the harness separately requires the tier-specific compile
// evidence, and runs this file with useWasmFastMemory both true and false. Out-of-bounds
// accesses must surface as WebAssembly.RuntimeError, never as a process crash.
var failures = [];
var groups = [];
function check(group, cond, detail) { if (!cond) failures.push(group + (detail ? ": " + detail : "")); }
function group(name, fn) { groups.push(name); try { fn(); } catch (e) { failures.push(name + ": threw " + e + (e && e.stack ? " @" + e.stack.split("\n")[0] : "")); } }

// ---- a tiny module assembler -------------------------------------------------------------
var I32 = 0x7f, I64 = 0x7e, F32 = 0x7d, F64 = 0x7c;
function u(n) { var o = []; do { var b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; o.push(b); } while (n); return o; }
function sl(n) { var o = []; for (;;) { var b = n & 0x7f; n >>= 7; if ((n === 0 && !(b & 0x40)) || (n === -1 && (b & 0x40))) { o.push(b); return o; } o.push(b | 0x80); } }
function str(s) { var o = u(s.length); for (var i = 0; i < s.length; i++) o.push(s.charCodeAt(i)); return o; }
function vec(items) { var o = u(items.length); items.forEach(function (it) { o = o.concat(it); }); return o; }
function section(id, bytes) { return [id].concat(u(bytes.length), bytes); }
function type(params, results) { return [0x60].concat(vec(params.map(function (p) { return [p]; })), vec(results.map(function (r) { return [r]; }))); }
function func(locals, code) { var body = vec(locals.map(function (l) { return [l[0], l[1]]; })).concat(code, [0x0b]); return u(body.length).concat(body); }
function build(spec) {
  var bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
  bytes = bytes.concat(section(1, vec(spec.types)));
  if (spec.imports) bytes = bytes.concat(section(2, vec(spec.imports)));
  bytes = bytes.concat(section(3, vec(spec.funcs.map(function (f) { return u(f.type); }))));
  if (spec.table) bytes = bytes.concat(section(4, vec([[0x70, 0x00].concat(u(spec.table))])));
  if (spec.memory) bytes = bytes.concat(section(5, vec([[0x01].concat(u(spec.memory[0]), u(spec.memory[1]))])));
  bytes = bytes.concat(section(7, vec(spec.exports.map(function (e) { return str(e[0]).concat([e[1]], u(e[2])); }))));
  if (spec.elems) bytes = bytes.concat(section(9, vec([[0x00, 0x41, 0x00, 0x0b].concat(vec(spec.elems.map(function (i) { return u(i); })))])));
  bytes = bytes.concat(section(10, vec(spec.funcs.map(function (f) { return f.body; }))));
  return new Uint8Array(bytes);
}
var FUNC = 0, TABLE = 1, MEM = 2;
var op = { get: 0x20, set: 0x21, tee: 0x22, i32c: 0x41, i64c: 0x42, f32c: 0x43, f64c: 0x44, i32add: 0x6a, i32sub: 0x6b, i32mul: 0x6c, i32divs: 0x6d,
           i32lts: 0x48, i32eqz: 0x45, i32load: 0x28, i32store: 0x36, i32load8u: 0x2d, memgrow: 0x40, memsize: 0x3f, call: 0x10, callind: 0x11, loop: 0x03, block: 0x02,
           brif: 0x0d, br: 0x0c, ret: 0x0f, unreachable: 0x00, i64add: 0x7c, i64mul: 0x7e, i64divs: 0x7f, f32add: 0x92, f64add: 0xa0, f64mul: 0xa2, f64div: 0xa3, i32rems: 0x6f,
           f64convi32s: 0xb7, i32truncf64s: 0xaa, f64sqrt: 0x9f };
function instantiate(spec, imports) { return new WebAssembly.Instance(new WebAssembly.Module(build(spec)), imports || {}).exports; }

// ---- groups ------------------------------------------------------------------------------
group("validate-and-compile", function () {
  var good = build({ types: [type([I32, I32], [I32])], funcs: [{ type: 0, body: func([], [op.get, 0, op.get, 1, op.i32add]) }], exports: [["add", FUNC, 0]] });
  check("validate-and-compile", WebAssembly.validate(good), "valid module rejected");
  var bad = good.slice(); bad[bad.length - 3] = 0xff;
  check("validate-and-compile", !WebAssembly.validate(bad), "invalid module accepted");
  var threw = false; try { new WebAssembly.Module(bad); } catch (e) { threw = e instanceof WebAssembly.CompileError; }
  check("validate-and-compile", threw, "CompileError for a bad module");
});

group("numeric-types", function () {
  var m = instantiate({
    types: [type([I32, I32], [I32]), type([I64, I64], [I64]), type([F32, F32], [F32]), type([F64, F64], [F64])],
    funcs: [{ type: 0, body: func([], [op.get, 0, op.get, 1, op.i32mul]) }, { type: 1, body: func([], [op.get, 0, op.get, 1, op.i64mul]) },
            { type: 2, body: func([], [op.get, 0, op.get, 1, op.f32add]) }, { type: 3, body: func([], [op.get, 0, op.get, 1, op.f64div]) },
            { type: 1, body: func([], [op.get, 0, op.get, 1, op.i64add]) }],
    exports: [["mul32", FUNC, 0], ["mul64", FUNC, 1], ["add32f", FUNC, 2], ["div64f", FUNC, 3], ["add64", FUNC, 4]] });
  check("numeric-types", m.mul32(65536, 65536) === 0 && m.mul32(-3, 7) === -21 && m.mul32(46341, 46341) === -2147479015, "i32 wraparound");
  check("numeric-types", m.mul64(4294967296n, 4294967296n) === 0n && m.mul64(3037000500n, 3037000500n) === 9223372037000250000n - 18446744073709551616n, "i64 wraparound " + m.mul64(3037000500n, 3037000500n));
  check("numeric-types", m.add64(9223372036854775807n, 1n) === -9223372036854775808n, "i64 add overflow");
  check("numeric-types", m.add32f(0.1, 0.2) === Math.fround(Math.fround(0.1) + Math.fround(0.2)), "f32 rounding");
  check("numeric-types", m.div64f(1, 3) === 1 / 3 && m.div64f(1, 0) === Infinity && Number.isNaN(m.div64f(0, 0)) && Object.is(m.div64f(-0, 1), -0), "f64 special values");
  var t = false; try { m.mul64(1, 2); } catch (e) { t = e instanceof TypeError; }
  check("numeric-types", t, "Number passed for an i64 must be a TypeError");
});

group("control-flow-and-calls", function () {
  // sum(n) = sum of i*3 for i < n, via loop; fib(n) recursive; both are made hot below.
  var m = instantiate({
    types: [type([I32], [I32])],
    funcs: [
      { type: 0, body: func([[1, I32], [1, I32]], [   // local1 = acc, local2 = i
        op.block, 0x40, op.loop, 0x40,
          op.get, 2, op.get, 0, op.i32lts, op.i32eqz, op.brif, 1,
          op.get, 1, op.get, 2, op.i32c, 3, op.i32mul, op.i32add, op.set, 1,
          op.get, 2, op.i32c, 1, op.i32add, op.set, 2,
          op.br, 0,
        0x0b, 0x0b, op.get, 1]) },
      { type: 0, body: func([], [op.get, 0, op.i32c, 2, op.i32lts, 0x04, I32, op.get, 0, 0x05,
        op.get, 0, op.i32c, 1, op.i32sub, op.call, 1, op.get, 0, op.i32c, 2, op.i32sub, op.call, 1, op.i32add, 0x0b]) }],
    exports: [["sum", FUNC, 0], ["fib", FUNC, 1]] });
  var expect = 0; for (var i = 0; i < 1000; i++) expect += i * 3;
  var got = 0; for (var r = 0; r < 4000; r++) got = m.sum(1000);
  check("control-flow-and-calls", got === expect, "loop result " + got + " vs " + expect);
  check("control-flow-and-calls", m.fib(20) === 6765, "recursive wasm calls " + m.fib(20));
  var f = 0; for (var r = 0; r < 300; r++) f = m.fib(15);
  check("control-flow-and-calls", f === 610, "hot recursion " + f);
});

group("imports-and-exceptions", function () {
  var calls = 0;
  var m = instantiate({
    types: [type([I32], [I32])],
    imports: [str("env").concat(str("cb"), [0x00], u(0))],
    funcs: [{ type: 0, body: func([], [op.get, 0, op.call, 0, op.i32c, 1, op.i32add]) }],
    exports: [["viaImport", FUNC, 1]] },
    { env: { cb: function (x) { calls++; if (x === 13) throw new RangeError("from js " + x); return x * 2; } } });
  var s = 0; for (var i = 0; i < 20000; i++) if (i !== 13) s += m.viaImport(i % 10);
  check("imports-and-exceptions", calls > 19000 && s > 0, "import calls " + calls);
  var caught = null; try { m.viaImport(13); } catch (e) { caught = e; }
  check("imports-and-exceptions", caught instanceof RangeError && caught.message === "from js 13", "JS exception through wasm frames");
  for (var i = 0; i < 20000; i++) { try { m.viaImport(13); } catch (e) { if (!(e instanceof RangeError)) { failures.push("exception loop " + e); break; } } }
  check("imports-and-exceptions", m.viaImport(5) === 11, "still callable after many exceptions");
});

group("table-and-call-indirect", function () {
  var m = instantiate({
    types: [type([I32], [I32]), type([I32, I32], [I32])],
    funcs: [{ type: 0, body: func([], [op.get, 0, op.i32c, 1, op.i32add]) }, { type: 0, body: func([], [op.get, 0, op.i32c, 2, op.i32mul]) },
            { type: 1, body: func([], [op.get, 1, op.get, 0, op.callind, 0, 0]) }],
    table: 3, elems: [0, 1, 2], exports: [["call", FUNC, 2], ["tab", TABLE, 0]] });
  var s = 0; for (var i = 0; i < 30000; i++) s += m.call(i & 1, 10);
  check("table-and-call-indirect", s === 15000 * 11 + 15000 * 20, "indirect calls " + s);
  var t = ""; try { m.call(2, 1); } catch (e) { t = e instanceof WebAssembly.RuntimeError ? "signature" : String(e); }
  check("table-and-call-indirect", t === "signature", "signature mismatch must trap: " + t);
  var o = ""; try { m.call(7, 1); } catch (e) { o = e instanceof WebAssembly.RuntimeError ? "oob" : String(e); }
  check("table-and-call-indirect", o === "oob", "table index out of bounds must trap: " + o);
  check("table-and-call-indirect", m.tab.length === 3 && typeof m.tab.get(0) === "function", "exported table");
});

group("memory-and-traps", function () {
  var m = instantiate({
    types: [type([I32], [I32]), type([I32, I32], []), type([I32, I32], [I32])],
    funcs: [{ type: 0, body: func([], [op.get, 0, op.i32load, 2, 0]) }, { type: 1, body: func([], [op.get, 0, op.get, 1, op.i32store, 2, 0]) },
            { type: 0, body: func([], [op.get, 0, op.memgrow, 0]) }, { type: 0, body: func([], [op.memsize, 0]) },
            { type: 2, body: func([], [op.get, 0, op.get, 1, op.i32divs]) }, { type: 2, body: func([], [op.get, 0, op.get, 1, op.i32rems]) },
            { type: 0, body: func([], [op.unreachable]) }, { type: 0, body: func([], [op.get, 0, op.i32load, 2, 0xff, 0xff, 0xff, 0x0f]) }],
    memory: [1, 4], exports: [["load", FUNC, 0], ["store", FUNC, 1], ["grow", FUNC, 2], ["size", FUNC, 3], ["div", FUNC, 4], ["rem", FUNC, 5], ["unreach", FUNC, 6], ["loadhi", FUNC, 7], ["mem", MEM, 0]] });
  for (var i = 0; i < 2000; i++) m.store(i * 4, i * 7);
  var s = 0; for (var r = 0; r < 20; r++) for (var i = 0; i < 2000; i++) s += m.load(i * 4);
  check("memory-and-traps", s === 20 * 7 * (1999 * 2000 / 2), "load/store " + s);
  check("memory-and-traps", new Int32Array(m.mem.buffer)[10] === 70, "JS view of the exported memory");
  var oldBuf = m.mem.buffer;
  check("memory-and-traps", m.size() === 1 && m.grow(1) === 1 && m.size() === 2 && m.grow(10) === -1, "memory.grow and its limit");
  check("memory-and-traps", oldBuf.byteLength === 0 && m.mem.buffer.byteLength === 2 * 65536, "grow detaches the old buffer");
  m.store(2 * 65536 - 4, 99); check("memory-and-traps", m.load(2 * 65536 - 4) === 99, "access at the new end of memory");
  function trap(f, name) { try { f(); } catch (e) { return e instanceof WebAssembly.RuntimeError ? "RuntimeError" : "wrong error " + e; } return "no trap"; }
  var results = {
    oobLoad: trap(function () { m.load(2 * 65536); }, "oob load"),
    oobLoadStraddle: trap(function () { m.load(2 * 65536 - 2); }, "straddle"),
    oobStore: trap(function () { m.store(2 * 65536, 1); }, "oob store"),
    oobHigh: trap(function () { m.load(-4); }, "negative = huge address"),
    oobOffset: trap(function () { m.loadhi(1); }, "large constant offset"),
    divZero: trap(function () { m.div(1, 0); }, "div0"), divOverflow: trap(function () { m.div(-2147483648, -1); }, "div overflow"), remZeroOk: trap(function () { m.rem(1, 0); }, "rem0"),
    unreachable: trap(function () { m.unreach(0); }, "unreachable") };
  for (var k in results) check("memory-and-traps", results[k] === "RuntimeError", k + " -> " + results[k]);
  // The trap must repeat reliably (the fault path is re-armed each time) and from hot code.
  var n = 0; for (var i = 0; i < 3000; i++) if (trap(function () { m.load(0x7ffffff0 + i); }) === "RuntimeError") n++;
  check("memory-and-traps", n === 3000, "repeated out-of-bounds traps " + n);
  var h = 0; for (var i = 0; i < 100000; i++) { h += m.load((i * 4) % 8000); }
  var again = 0; for (var i = 0; i < 100; i++) { again += trap(function () { m.load(0x10000000 + i); }) === "RuntimeError" ? 1 : 0; }
  check("memory-and-traps", again === 100, "traps from code that has been optimised " + again);
});

group("instance-lifecycle", function () {
  var bytes = build({ types: [type([I32], [I32])], funcs: [{ type: 0, body: func([], [op.get, 0, op.i32c, 1, op.i32add]) }], memory: [1, 2], exports: [["inc", FUNC, 0], ["mem", MEM, 0]] });
  var mod = new WebAssembly.Module(bytes), total = 0;
  for (var i = 0; i < 400; i++) { var inst = new WebAssembly.Instance(mod, {}); total += inst.exports.inc(i); }
  check("instance-lifecycle", total === 400 * 399 / 2 + 400, "400 instances of one module " + total);
  fullGC(); edenGC();
  var keep = new WebAssembly.Instance(mod, {}); fullGC();
  check("instance-lifecycle", keep.exports.inc(1) === 2, "instance usable after GC");
  var memories = []; for (var i = 0; i < 64; i++) memories.push(new WebAssembly.Memory({ initial: 1, maximum: 4 }));
  memories.forEach(function (mem) { mem.grow(1); }); memories = null; fullGC();
  check("instance-lifecycle", true);
});

// Asynchronous compilation and instantiation run the compiler off the calling thread.
var asyncGood = build({ types: [type([I32, I32], [I32])], funcs: [{ type: 0, body: func([], [op.get, 0, op.get, 1, op.i32add]) }], exports: [["add", FUNC, 0]] });
var asyncDone = [];
Promise.all([WebAssembly.instantiate(asyncGood), WebAssembly.compile(asyncGood).then(function (m) { return WebAssembly.instantiate(m); }),
             WebAssembly.instantiate(new Uint8Array([0, 1, 2])).then(function () { return "accepted"; }, function (e) { return e instanceof WebAssembly.CompileError ? "CompileError" : String(e); })])
  .then(function (r) {
    check("async", r[0].instance.exports.add(2, 3) === 5, "WebAssembly.instantiate(bytes)");
    check("async", r[1].exports.add(4, 5) === 9, "WebAssembly.compile then instantiate");
    check("async", r[2] === "CompileError", "async compile of garbage: " + r[2]);
    if (failures.length) { print("jsc_wasm_acceptance: FAILED " + failures.join(" | ")); throw new Error("wasm acceptance failed"); }
    print("jsc_wasm_acceptance: ok groups=" + (groups.length + 1) + " " + groups.concat("async").join(","));
  });
