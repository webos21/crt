// Baseline-JIT acceptance (Web Tranche 1C). Run with the JIT options the harness sets
// (JSC_useJIT=true, JSC_jitPolicyScale=0.01, ...): every function below becomes hot almost at
// once, so the results are produced by generated machine code, not the interpreter. The
// harness separately requires the compile report that proves generated code existed.
var failures = [];
var groups = [];
function check(group, cond, detail) { if (!cond) failures.push(group + (detail ? ": " + detail : "")); }
function group(name, fn) { groups.push(name); try { fn(); } catch (e) { failures.push(name + ": threw " + e); } }

group("hot-arithmetic", function () {
  function mix(n) { var h = 2166136261; for (var i = 0; i < n; i++) { h = Math.imul(h ^ (i & 255), 16777619) >>> 0; } return h; }
  var first = mix(1000), again = 0;
  for (var round = 0; round < 300; round++) again = mix(1000);
  check("hot-arithmetic", first === again, "stable result " + first + " vs " + again);
  function sum(n) { var s = 0; for (var i = 1; i <= n; i++) s += i * 0.5; return s; }
  var r = 0; for (var k = 0; k < 400; k++) r = sum(2000);
  check("hot-arithmetic", r === 1000500, "double sum " + r);
  function bigish(n) { var x = 1; for (var i = 0; i < n; i++) x = (x * 3 + 1) % 1000003; return x; }
  var b = 0; for (var k = 0; k < 400; k++) b = bigish(500);
  check("hot-arithmetic", b === bigish(500), "modular loop");
});

group("hot-calls", function () {
  function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
  check("hot-calls", fib(22) === 17711, "recursive fib");
  var add = function (a, b) { return a + b; };
  var t = 0; for (var i = 0; i < 100000; i++) t = add(t, i & 7);
  check("hot-calls", t === 350000, "closure call loop " + t);
  function apply3(f, x) { return f(f(f(x))); }
  var v = 0; for (var i = 0; i < 20000; i++) v = apply3(function (q) { return q + 1; }, i);
  check("hot-calls", v === 19999 + 3, "higher-order " + v);
  class A { constructor(x) { this.x = x; } get d() { return this.x * 2; } m(y) { return this.x + y; } }
  class B extends A { m(y) { return super.m(y) * 2; } }
  var o = new B(5), acc = 0; for (var i = 0; i < 50000; i++) acc += o.m(i & 3) + o.d;
  check("hot-calls", acc > 0 && o.m(1) === 12 && o.d === 10, "class dispatch");
});

group("arrays-properties", function () {
  var arr = []; for (var i = 0; i < 5000; i++) arr.push(i * 2);
  function total(a) { var s = 0; for (var i = 0; i < a.length; i++) s += a[i]; return s; }
  var s = 0; for (var k = 0; k < 200; k++) s = total(arr);
  check("arrays-properties", s === 5000 * 4999, "array sum " + s);
  function getX(o) { return o.x; }
  var shapes = [{ x: 1 }, { x: 2, y: 3 }, { y: 0, x: 4 }, Object.create({ x: 5 })];
  var sum = 0; for (var k = 0; k < 40000; k++) sum += getX(shapes[k & 3]);
  check("arrays-properties", sum === 10000 * (1 + 2 + 4 + 5), "polymorphic property " + sum);
  var ta = new Float64Array(1000); for (var i = 0; i < ta.length; i++) ta[i] = i / 4;
  function tsum(t) { var s = 0; for (var i = 0; i < t.length; i++) s += t[i]; return s; }
  var ts = 0; for (var k = 0; k < 300; k++) ts = tsum(ta);
  check("arrays-properties", ts === 124875, "typed array " + ts);
  var holey = [1, , 3]; holey[10] = 4;
  function cnt(a) { var c = 0; for (var i = 0; i < a.length; i++) if (a[i] !== undefined) c++; return c; }
  var c = 0; for (var k = 0; k < 5000; k++) c = cnt(holey);
  check("arrays-properties", c === 3, "holey array " + c);
});

group("exception-through-jit", function () {
  function leaf(i) { if (i % 1000 === 999) throw new RangeError("leaf " + i); return i; }
  function middle(i) { return leaf(i) + 1; }
  function top(i) { try { return middle(i); } catch (e) { return -1; } finally { top.finalized = (top.finalized | 0) + 1; } }
  var caught = 0; for (var i = 0; i < 20000; i++) if (top(i) === -1) caught++;
  check("exception-through-jit", caught === 20 && top.finalized === 20000, "caught " + caught);
  var uncaught = null; try { for (var i = 0; i < 5000; i++) middle(i); } catch (e) { uncaught = e; }
  check("exception-through-jit", uncaught instanceof RangeError && uncaught.message === "leaf 999", "propagated");
  function deep(n) { return n === 0 ? null.x : deep(n - 1); }
  var type = null; try { for (var i = 0; i < 300; i++) deep(30); } catch (e) { type = e; }
  check("exception-through-jit", type instanceof TypeError, "TypeError from compiled frames");
  var overflow = false; function inf() { return inf() + 1; }
  try { inf(); } catch (e) { overflow = e instanceof RangeError; }
  check("exception-through-jit", overflow, "stack overflow in compiled code");
});

group("gc-with-live-compiled-code", function () {
  function make(k) { return function (x) { return x * k + 1; }; }
  var fns = []; for (var i = 0; i < 200; i++) fns.push(make(i));
  var warm = 0; for (var r = 0; r < 300; r++) for (var i = 0; i < fns.length; i++) warm += fns[i](r) & 1;
  fullGC();
  var junk = []; for (var i = 0; i < 100000; i++) junk.push({ i: i, s: "j" + i });
  junk = null; edenGC(); fullGC();
  var again = 0; for (var i = 0; i < fns.length; i++) again += fns[i](3);
  var expect = 0; for (var i = 0; i < 200; i++) expect += i * 3 + 1;
  check("gc-with-live-compiled-code", again === expect, "closures after GC " + again + " vs " + expect);
  function allocator(n) { var o = null; for (var i = 0; i < n; i++) o = { next: o, v: i }; return o; }
  var keep = null; for (var r = 0; r < 60; r++) { keep = allocator(5000); if (r % 20 === 0) fullGC(); }
  var len = 0; for (var p = keep; p; p = p.next) len++;
  check("gc-with-live-compiled-code", len === 5000, "list intact " + len);
});

group("repeated-compilation", function () {
  var total = 0;
  for (var i = 0; i < 1500; i++) {
    var f = new Function("a", "var s = 0; for (var j = 0; j < 50; j++) s += a + j + " + i + "; return s;");
    for (var k = 0; k < 4; k++) total += f(k);
  }
  var expect = 0;
  for (var i = 0; i < 1500; i++) for (var k = 0; k < 4; k++) { var s = 0; for (var j = 0; j < 50; j++) s += k + j + i; expect += s; }
  check("repeated-compilation", total === expect, "generated functions " + total + " vs " + expect);
  var ev = 0; for (var i = 0; i < 400; i++) ev += eval("(function(){ return " + i + " * 2; })()");
  check("repeated-compilation", ev === 399 * 400, "eval " + ev);
});

group("regexp-and-strings", function () {
  var re = /(\d+)-(\w+)@([a-z]+)\.com/;
  var hits = 0, text = "id 12345-abc@example.com end";
  for (var i = 0; i < 40000; i++) { var m = re.exec(text); if (m && m[1] === "12345" && m[3] === "example") hits++; }
  check("regexp-and-strings", hits === 40000, "regexp hits " + hits);
  var parts = []; for (var i = 0; i < 2000; i++) parts.push("item" + (i % 17));
  var joined = parts.join(","), counts = {};
  joined.split(",").forEach(function (p) { counts[p] = (counts[p] || 0) + 1; });
  check("regexp-and-strings", Object.keys(counts).length === 17 && counts.item0 === 118, "split/count");
  var up = 0; for (var i = 0; i < 5000; i++) up += ("straße" + i).toUpperCase().length;
  check("regexp-and-strings", up > 0, "unicode case mapping from compiled code");
});

var order = [];
Promise.resolve().then(function () { order.push("a"); });
(async function () { for (var i = 0; i < 2000; i++) await null; order.push("loop"); })().then(function () {
  check("async", order[0] === "a" && order[1] === "loop", "async loop order " + order.join());
  if (failures.length) { print("jsc_jit_acceptance: FAILED " + failures.join(" | ")); throw new Error("jit acceptance failed"); }
  print("jsc_jit_acceptance: ok groups=" + (groups.length + 1) + " " + groups.concat("async").join(","));
});
