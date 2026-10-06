// FTL acceptance (Web Tranche 1D-B). Run with the harness's FTL options (JSC_useFTLJIT=true on top of
// DFG and concurrent JIT, WebAssembly off): hot functions go Baseline -> DFG -> FTL (B3/Air) on compiler
// threads while the main thread keeps running them. The harness separately requires the FTL compile
// reports; this file checks the answers. Loop counts are sized so that functions reach FTL even at
// the default tier-up thresholds (thresholdForFTLOptimizeAfterWarmUp).
var failures = [];
var groups = [];
function check(group, cond, detail) { if (!cond) failures.push(group + (detail ? ": " + detail : "")); }
function group(name, fn) { groups.push(name); try { fn(); } catch (e) { failures.push(name + ": threw " + e); } }
function warm(f, args, times) { var r; for (var i = 0; i < times; i++) r = f.apply(null, args); return r; }

group("hot-int-and-double", function () {
  function mix(n) { var h = 2166136261; for (var i = 0; i < n; i++) { h = Math.imul(h ^ (i & 255), 16777619) >>> 0; } return h; }
  var expected = mix(1000), got = 0;
  for (var round = 0; round < 30000; round++) got = mix(1000);
  check("hot-int-and-double", got === expected, "int loop " + got + " vs " + expected);
  function sum(n) { var s = 0; for (var i = 1; i <= n; i++) s += i * 0.5; return s; }
  check("hot-int-and-double", warm(sum, [2000], 30000) === 1000500, "double loop");
  function ovf(a, b) { return a + b; }
  warm(ovf, [1, 2], 20000);
  check("hot-int-and-double", ovf(2147483647, 1) === 2147483648 && ovf(0.5, 0.25) === 0.75, "int overflow exit");
});

group("osr-exit-and-reoptimise", function () {
  function f(x) { return x + 1; }
  warm(f, [1], 300000);
  check("osr-exit-and-reoptimise", f("a") === "a1", "string after int speculation");
  check("osr-exit-and-reoptimise", f(1.5) === 2.5 && f(null) === 1 && f(undefined) !== f(undefined), "mixed types");
  warm(f, [2], 300000);
  check("osr-exit-and-reoptimise", f(3) === 4, "reoptimised");
  function g(o) { return o.v; }
  warm(g, [{ v: 1 }], 300000);
  var shapes = [{ v: 1 }, { a: 0, v: 2 }, { b: 0, c: 0, v: 3 }, Object.create({ v: 4 })], t = 0;
  for (var i = 0; i < 400000; i++) t += g(shapes[i & 3]);
  check("osr-exit-and-reoptimise", t === 100000 * 10, "shape change after optimisation " + t);
  function h(a, i) { return a[i]; }
  warm(h, [[1, 2, 3], 1], 300000);
  check("osr-exit-and-reoptimise", h([1, 2, 3], 7) === undefined && h({ 1: "x" }, 1) === "x", "out of bounds / non-array");
});

group("inlining-and-calls", function () {
  function sq(x) { return x * x; }
  function sumsq(n) { var s = 0; for (var i = 0; i < n; i++) s += sq(i); return s; }
  check("inlining-and-calls", warm(sumsq, [100], 50000) === 328350, "inlined callee");
  var target = sq;
  function viaVar(x) { return target(x) + 1; }
  warm(viaVar, [3], 300000);
  target = function (x) { return -x; };
  check("inlining-and-calls", viaVar(3) === -2, "callee replaced after inlining");
  function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
  check("inlining-and-calls", fib(27) === 196418, "recursion");
  function va() { var s = 0; for (var i = 0; i < arguments.length; i++) s += arguments[i]; return s; }
  function spread(a) { return va.apply(null, a) + va(...a); }
  check("inlining-and-calls", warm(spread, [[1, 2, 3]], 20000) === 12, "arguments / apply / spread");
  class A { constructor(x) { this.x = x; } get d() { return this.x * 2; } m(y) { return this.x + y; } }
  class B extends A { m(y) { return super.m(y) * 2; } }
  var o = new B(5), acc = 0; for (var i = 0; i < 400000; i++) acc += o.m(i & 3) + o.d;
  check("inlining-and-calls", o.m(1) === 12 && acc > 0, "class dispatch");
});

group("exception-through-ftl", function () {
  function leaf(i) { if (i % 1000 === 999) throw new RangeError("leaf " + i); return i; }
  function middle(i) { return leaf(i) + 1; }
  function top(i) { try { return middle(i); } catch (e) { return -1; } finally { top.n = (top.n | 0) + 1; } }
  var caught = 0; for (var i = 0; i < 200000; i++) if (top(i) === -1) caught++;
  check("exception-through-ftl", caught === 200 && top.n === 200000, "caught " + caught);
  function div(a, b) { return a / b | 0; }
  warm(div, [10, 3], 300000);
  check("exception-through-ftl", div(1, 0) === 0 && div(-7, 2) === -3, "division exits");
  function deep(n) { return n === 0 ? null.x : deep(n - 1); }
  var type = null; try { for (var i = 0; i < 2000; i++) deep(20); } catch (e) { type = e; }
  check("exception-through-ftl", type instanceof TypeError, "TypeError from optimised frames");
  var overflow = false; function inf() { return inf() + 1; }
  try { for (var i = 0; i < 3; i++) inf(); } catch (e) { overflow = e instanceof RangeError; }
  check("exception-through-ftl", overflow, "stack overflow");
});

group("gc-with-live-ftl-code", function () {
  function make(k) { return function (x) { return x * k + 1; }; }
  var fns = []; for (var i = 0; i < 100; i++) fns.push(make(i));
  for (var r = 0; r < 30000; r++) for (var i = 0; i < fns.length; i++) fns[i](r);
  fullGC();
  var junk = []; for (var i = 0; i < 100000; i++) junk.push({ i: i, s: "j" + i });
  junk = null; edenGC(); fullGC();
  var again = 0; for (var i = 0; i < fns.length; i++) again += fns[i](3);
  check("gc-with-live-ftl-code", again === 100 + 3 * (99 * 100 / 2), "survived collections " + again);
  function alloc(n) { var o = null; for (var i = 0; i < n; i++) o = { next: o, i: i }; return o; }
  var len = 0; for (var k = 0; k < 400; k++) { var l = alloc(200); len = 0; while (l) { len++; l = l.next; } if (k % 100 === 0) edenGC(); }
  check("gc-with-live-ftl-code", len === 200, "allocation in optimised code");
});

group("concurrent-compilation", function () {
  // Many distinct functions become hot together, so several are queued for the compiler
  // threads while the main thread keeps executing (and invalidating) them.
  var fns = [];
  for (var i = 0; i < 60; i++) fns.push(new Function("a", "var s = 0; for (var i = 0; i < 50; i++) s += (a ^ i) * " + (i + 1) + "; return s;"));
  var total = 0;
  for (var r = 0; r < 20000; r++) for (var i = 0; i < fns.length; i++) total += fns[i](r & 15);
  var expect = 0;
  for (var r = 0; r < 20000; r++) for (var i = 0; i < 60; i++) { var s = 0; for (var j = 0; j < 50; j++) s += ((r & 15) ^ j) * (i + 1); expect += s; }
  check("concurrent-compilation", total === expect, "sum " + total + " vs " + expect);
  var holder = { x: 1 };
  function reader() { return holder.x; }
  var seen = 0;
  for (var i = 0; i < 400000; i++) { if (i === 200000) holder = { y: 0, x: 2 }; seen += reader(); }
  check("concurrent-compilation", seen === 200000 + 400000, "invalidation while compiling " + seen);
});

group("typed-arrays-and-strings", function () {
  var f64 = new Float64Array(1000); for (var i = 0; i < 1000; i++) f64[i] = i / 4;
  function tsum(t) { var s = 0; for (var i = 0; i < t.length; i++) s += t[i]; return s; }
  check("typed-arrays-and-strings", warm(tsum, [f64], 30000) === 124875, "float64 array");
  var u8 = new Uint8Array(256); function fill(a) { for (var i = 0; i < a.length; i++) a[i] = i * 3; return a[255]; }
  check("typed-arrays-and-strings", warm(fill, [u8], 200000) === (255 * 3) % 256, "uint8 wrap");
  function cat(n) { var s = ""; for (var i = 0; i < n; i++) s += String.fromCharCode(97 + (i % 26)); return s.length + s.charCodeAt(n - 1); }
  check("typed-arrays-and-strings", warm(cat, [200], 50000) === 200 + 97 + (199 % 26), "string building");
});


group("large-function", function () {
  // Many basic blocks and live values: stresses B3 lowering, Air register allocation and spilling.
  var body = "var a=x|0,b=a+1,c=b*3,d=c^a,e=d+b,f=e-c,g=f*2,h=g|1,i2=h+a,j=i2^b;"
  for (var k = 0; k < 40; k++) body += "if ((a + " + k + ") & 1) { a = (a * 31 + " + k + ") | 0; b = (b ^ a) + c; } else { c = (c + b) | 0; d = (d * 7 - a) | 0; } e = (e + a + b + c + d) | 0;";
  body += "return (a + b + c + d + e + f + g + h + i2 + j) | 0;";
  var big = new Function("x", body), ref = new Function("x", body);   // ref stays cold longer than big
  var v = 0; for (var r = 0; r < 60000; r++) v = big(r & 255);
  var w = 0; for (var r = 0; r < 3; r++) w = ref(255);
  check("large-function", big(255) === w, "optimised result " + big(255) + " vs cold " + w);
  function mulhi(a, b) { return Math.imul(a, b) + (a * b) % 7 + Math.floor(a / 3) + (a >>> 3) + Math.clz32(a) + Math.trunc(b / 5); }
  var m = 0; for (var r = 0; r < 300000; r++) m = mulhi(r, r + 1);
  check("large-function", m === mulhi(299999, 300000), "mixed integer/double ops");
});

group("float-and-int64-ish", function () {
  function dot(a, b) { var s = 0; for (var i = 0; i < a.length; i++) s += a[i] * b[i]; return s; }
  var a = new Float64Array(256), b = new Float64Array(256);
  for (var i = 0; i < 256; i++) { a[i] = i * 0.25; b[i] = 256 - i; }
  var d = 0; for (var r = 0; r < 20000; r++) d = dot(a, b);
  var exp = 0; for (var i = 0; i < 256; i++) exp += a[i] * b[i];
  check("float-and-int64-ish", d === exp, "float64 dot product");
  function nanbox(x) { return x !== x ? 1 : (x === Infinity ? 2 : (x === 0 && 1 / x < 0 ? 3 : 0)); }
  warm(nanbox, [1.5], 300000);
  check("float-and-int64-ish", nanbox(NaN) === 1 && nanbox(Infinity) === 2 && nanbox(-0) === 3 && nanbox(7) === 0, "special values");
  function big(n) { var x = 1; for (var i = 0; i < n; i++) x = (x * 3) % 4294967291; return x; }
  var bx = 0; for (var r = 0; r < 20000; r++) bx = big(40);
  check("float-and-int64-ish", bx === big(40) && bx > 0, "values above int32");
  function bigint(n) { var x = 1n; for (var i = 0; i < n; i++) x = (x * 3n + 1n) % 1000000007n; return x; }
  var bi = 0n; for (var r = 0; r < 3000; r++) bi = bigint(30);
  check("float-and-int64-ish", bi === bigint(30), "BigInt arithmetic");
});


if (failures.length) { print("jsc_ftl_acceptance: FAILED " + failures.join(" | ")); throw new Error("ftl acceptance failed"); }
print("jsc_ftl_acceptance: ok groups=" + groups.length + " " + groups.join(","));
