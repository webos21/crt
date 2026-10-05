// DFG acceptance (Web Tranche 1D-A). Run with the harness's DFG options (JSC_useDFGJIT=true,
// JSC_useConcurrentJIT=true, low optimisation thresholds, FTL and WebAssembly off): hot
// functions go Baseline -> DFG on compiler threads while the main thread keeps running them.
// The harness separately requires the DFG compile reports; this file checks the answers.
// `optimizeNextInvocation`, `numberOfDFGCompiles` and `fullGC` are jsc-shell testing hooks.
var failures = [];
var groups = [];
function check(group, cond, detail) { if (!cond) failures.push(group + (detail ? ": " + detail : "")); }
function group(name, fn) { groups.push(name); try { fn(); } catch (e) { failures.push(name + ": threw " + e); } }
function warm(f, args, times) { var r; for (var i = 0; i < times; i++) r = f.apply(null, args); return r; }

group("hot-int-and-double", function () {
  function mix(n) { var h = 2166136261; for (var i = 0; i < n; i++) { h = Math.imul(h ^ (i & 255), 16777619) >>> 0; } return h; }
  var expected = mix(1000), got = 0;
  for (var round = 0; round < 3000; round++) got = mix(1000);
  check("hot-int-and-double", got === expected, "int loop " + got + " vs " + expected);
  function sum(n) { var s = 0; for (var i = 1; i <= n; i++) s += i * 0.5; return s; }
  check("hot-int-and-double", warm(sum, [2000], 3000) === 1000500, "double loop");
  function ovf(a, b) { return a + b; }
  warm(ovf, [1, 2], 20000);
  check("hot-int-and-double", ovf(2147483647, 1) === 2147483648 && ovf(0.5, 0.25) === 0.75, "int overflow exit");
});

group("osr-exit-and-reoptimise", function () {
  function f(x) { return x + 1; }
  warm(f, [1], 30000);                      // optimised for int32
  check("osr-exit-and-reoptimise", f("a") === "a1", "string after int speculation");
  check("osr-exit-and-reoptimise", f(1.5) === 2.5 && f(null) === 1 && f(undefined) !== f(undefined), "mixed types");
  warm(f, [2], 30000);
  check("osr-exit-and-reoptimise", f(3) === 4, "reoptimised");
  function g(o) { return o.v; }
  warm(g, [{ v: 1 }], 30000);
  var shapes = [{ v: 1 }, { a: 0, v: 2 }, { b: 0, c: 0, v: 3 }, Object.create({ v: 4 })], t = 0;
  for (var i = 0; i < 40000; i++) t += g(shapes[i & 3]);
  check("osr-exit-and-reoptimise", t === 10000 * 10, "shape change after optimisation " + t);
  function h(a, i) { return a[i]; }
  warm(h, [[1, 2, 3], 1], 30000);
  check("osr-exit-and-reoptimise", h([1, 2, 3], 7) === undefined && h({ 1: "x" }, 1) === "x", "out of bounds / non-array");
});

group("inlining-and-calls", function () {
  function sq(x) { return x * x; }
  function sumsq(n) { var s = 0; for (var i = 0; i < n; i++) s += sq(i); return s; }
  check("inlining-and-calls", warm(sumsq, [100], 5000) === 328350, "inlined callee");
  var target = sq;
  function viaVar(x) { return target(x) + 1; }
  warm(viaVar, [3], 30000);
  target = function (x) { return -x; };
  check("inlining-and-calls", viaVar(3) === -2, "callee replaced after inlining");
  function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
  check("inlining-and-calls", fib(25) === 75025, "recursion");
  function va() { var s = 0; for (var i = 0; i < arguments.length; i++) s += arguments[i]; return s; }
  function spread(a) { return va.apply(null, a) + va(...a); }
  check("inlining-and-calls", warm(spread, [[1, 2, 3]], 20000) === 12, "arguments / apply / spread");
  class A { constructor(x) { this.x = x; } get d() { return this.x * 2; } m(y) { return this.x + y; } }
  class B extends A { m(y) { return super.m(y) * 2; } }
  var o = new B(5), acc = 0; for (var i = 0; i < 60000; i++) acc += o.m(i & 3) + o.d;
  check("inlining-and-calls", o.m(1) === 12 && acc > 0, "class dispatch");
});

group("exception-through-dfg", function () {
  function leaf(i) { if (i % 1000 === 999) throw new RangeError("leaf " + i); return i; }
  function middle(i) { return leaf(i) + 1; }
  function top(i) { try { return middle(i); } catch (e) { return -1; } finally { top.n = (top.n | 0) + 1; } }
  var caught = 0; for (var i = 0; i < 60000; i++) if (top(i) === -1) caught++;
  check("exception-through-dfg", caught === 60 && top.n === 60000, "caught " + caught);
  function div(a, b) { return a / b | 0; }
  warm(div, [10, 3], 30000);
  check("exception-through-dfg", div(1, 0) === 0 && div(-7, 2) === -3, "division exits");
  function deep(n) { return n === 0 ? null.x : deep(n - 1); }
  var type = null; try { for (var i = 0; i < 2000; i++) deep(20); } catch (e) { type = e; }
  check("exception-through-dfg", type instanceof TypeError, "TypeError from optimised frames");
  var overflow = false; function inf() { return inf() + 1; }
  try { for (var i = 0; i < 3; i++) inf(); } catch (e) { overflow = e instanceof RangeError; }
  check("exception-through-dfg", overflow, "stack overflow");
});

group("gc-with-live-dfg-code", function () {
  function make(k) { return function (x) { return x * k + 1; }; }
  var fns = []; for (var i = 0; i < 100; i++) fns.push(make(i));
  for (var r = 0; r < 3000; r++) for (var i = 0; i < fns.length; i++) fns[i](r);
  fullGC();
  var junk = []; for (var i = 0; i < 100000; i++) junk.push({ i: i, s: "j" + i });
  junk = null; edenGC(); fullGC();
  var again = 0; for (var i = 0; i < fns.length; i++) again += fns[i](3);
  check("gc-with-live-dfg-code", again === 100 + 3 * (99 * 100 / 2), "survived collections " + again);
  function alloc(n) { var o = null; for (var i = 0; i < n; i++) o = { next: o, i: i }; return o; }
  var len = 0; for (var k = 0; k < 400; k++) { var l = alloc(200); len = 0; while (l) { len++; l = l.next; } if (k % 100 === 0) edenGC(); }
  check("gc-with-live-dfg-code", len === 200, "allocation in optimised code");
});

group("concurrent-compilation", function () {
  // Many distinct functions become hot together, so several are queued for the compiler
  // threads while the main thread keeps executing (and invalidating) them.
  var fns = [];
  for (var i = 0; i < 60; i++) fns.push(new Function("a", "var s = 0; for (var i = 0; i < 50; i++) s += (a ^ i) * " + (i + 1) + "; return s;"));
  var total = 0;
  for (var r = 0; r < 4000; r++) for (var i = 0; i < fns.length; i++) total += fns[i](r & 15);
  var expect = 0;
  for (var r = 0; r < 4000; r++) for (var i = 0; i < 60; i++) { var s = 0; for (var j = 0; j < 50; j++) s += ((r & 15) ^ j) * (i + 1); expect += s; }
  check("concurrent-compilation", total === expect, "sum " + total + " vs " + expect);
  var holder = { x: 1 };
  function reader() { return holder.x; }
  var seen = 0;
  for (var i = 0; i < 100000; i++) { if (i === 50000) holder = { y: 0, x: 2 }; seen += reader(); }
  check("concurrent-compilation", seen === 50000 + 100000, "invalidation while compiling " + seen);
});

group("typed-arrays-and-strings", function () {
  var f64 = new Float64Array(1000); for (var i = 0; i < 1000; i++) f64[i] = i / 4;
  function tsum(t) { var s = 0; for (var i = 0; i < t.length; i++) s += t[i]; return s; }
  check("typed-arrays-and-strings", warm(tsum, [f64], 3000) === 124875, "float64 array");
  var u8 = new Uint8Array(256); function fill(a) { for (var i = 0; i < a.length; i++) a[i] = i * 3; return a[255]; }
  check("typed-arrays-and-strings", warm(fill, [u8], 20000) === (255 * 3) % 256, "uint8 wrap");
  function cat(n) { var s = ""; for (var i = 0; i < n; i++) s += String.fromCharCode(97 + (i % 26)); return s.length + s.charCodeAt(n - 1); }
  check("typed-arrays-and-strings", warm(cat, [200], 5000) === 200 + 97 + (199 % 26), "string building");
});

if (typeof numberOfDFGCompiles === "function") {
  var f = function (x) { return x * 3 + 1; };
  for (var i = 0; i < 100000; i++) f(i);
  optimizeNextInvocation(f); f(1);
  check("dfg-compiled", numberOfDFGCompiles(f) > 0, "numberOfDFGCompiles(f) == " + numberOfDFGCompiles(f));
}

if (failures.length) { print("jsc_dfg_acceptance: FAILED " + failures.join(" | ")); throw new Error("dfg acceptance failed"); }
print("jsc_dfg_acceptance: ok groups=" + groups.length + " " + groups.join(","));
