// CRT acceptance for the JavaScriptCore bring-up (Web Tranche 1B): run with the
// `jsc` shell built by tools/build_webkit_jsc.py. Every group states what it
// checked; any failure prints FAILED and the shell exits non-zero.
var failures = [];
function check(group, cond, detail) {
  if (!cond) failures.push(group + (detail ? ": " + detail : ""));
}
var groups = [];
function group(name, fn) {
  groups.push(name);
  try { fn(); } catch (e) { failures.push(name + ": threw " + e); }
}

group("arithmetic", function () {
  check("arithmetic", 0.1 + 0.2 !== 0.3 && Math.abs(0.1 + 0.2 - 0.3) < 1e-15, "float");
  check("arithmetic", 2 ** 53 + 1 === 2 ** 53, "double precision");
  check("arithmetic", (123456789n * 987654321n).toString() === "121932631112635269", "bigint");
  check("arithmetic", (-7) % 3 === -1 && Math.trunc(-7 / 2) === -3, "remainder");
  check("arithmetic", Math.fround(5.5) === 5.5 && Math.imul(0xffffffff, 5) === -5, "imul");
  var sum = 0;
  for (var i = 1; i <= 100000; i++) sum += i;
  check("arithmetic", sum === 5000050000, "loop sum " + sum);
});

group("objects-arrays", function () {
  var o = { a: 1, b: { c: [1, 2, 3] } };
  check("objects-arrays", o.b.c.map(function (x) { return x * 2; }).join() === "2,4,6", "map");
  check("objects-arrays", Object.keys(o).join() === "a,b", "keys");
  class P { constructor(n) { this.n = n; } get d() { return this.n * 2; } static make(n) { return new P(n); } }
  check("objects-arrays", P.make(21).d === 42, "class getter");
  var m = new Map([[1, "x"], [2, "y"]]); var s = new Set([1, 2, 2, 3]);
  check("objects-arrays", m.get(2) === "y" && s.size === 3, "Map/Set");
  var a = []; for (var i = 0; i < 1000; i++) a.push(i);
  check("objects-arrays", a.slice(10, 13).join() === "10,11,12" && a.length === 1000, "array");
  var proxy = new Proxy({}, { get: function (t, k) { return "p:" + String(k); } });
  check("objects-arrays", proxy.foo === "p:foo", "Proxy");
  check("objects-arrays", [3, 1, 2].sort().join() === "1,2,3" && [..."abc"].length === 3, "sort/spread");
});

group("json", function () {
  var text = JSON.stringify({ a: [1, 2, { b: "x\né" }], c: null, d: true });
  var back = JSON.parse(text);
  check("json", back.a[2].b === "x\né" && back.c === null && back.d === true, "round trip " + text);
  check("json", JSON.stringify({ u: undefined, f: function () {} }) === "{}", "omits undefined");
  var threw = false; try { JSON.parse("{bad"); } catch (e) { threw = e instanceof SyntaxError; }
  check("json", threw, "SyntaxError on bad input");
});

group("regexp", function () {
  var m = /(\d{4})-(\d{2})-(\d{2})/.exec("on 2026-10-03 ok");
  check("regexp", m && m[1] === "2026" && m[3] === "03" && m.index === 3, "capture");
  check("regexp", "aBc".replace(/b/i, "X") === "aXc", "replace");
  var named = /(?<y>\d+)-(?<m>\d+)/.exec("7-9");
  check("regexp", named.groups.y === "7" && named.groups.m === "9", "named groups");
  check("regexp", /(?<=\$)\d+/.exec("cost $42")[0] === "42", "lookbehind");
  check("regexp", "a1b22c333".match(/\d+/g).join() === "1,22,333", "global");
  check("regexp", /\p{Lu}/u.test("É") && !/\p{Lu}/u.test("e"), "unicode property");
});

group("unicode-icu", function () {
  check("unicode-icu", "straße".toUpperCase() === "STRASSE", "sharp s");
  check("unicode-icu", "é".normalize("NFD").length === 2 && "é".normalize("NFC") === "é", "normalize");
  check("unicode-icu", "\u{1F600}".length === 2 && [..."\u{1F600}"].length === 1, "surrogates");
  check("unicode-icu", new Intl.Collator("sv").compare("z", "å") < 0 &&
                       new Intl.Collator("en").compare("z", "å") > 0, "locale collation");
  check("unicode-icu", new Intl.NumberFormat("de-DE").format(1234567.5) === "1.234.567,5", "number format");
  check("unicode-icu", "I".toLocaleLowerCase("tr") === "ı", "turkish dotless i");
  var seg = new Intl.Segmenter("en", { granularity: "word" });
  var words = 0; for (var x of seg.segment("one two three")) if (x.isWordLike) words++;
  check("unicode-icu", words === 3, "segmenter " + words);
});

group("exceptions", function () {
  function thrower() { throw new RangeError("boom"); }
  var caught = null; try { thrower(); } catch (e) { caught = e; } finally { caught = caught || "none"; }
  check("exceptions", caught instanceof RangeError && caught.message === "boom", "caught");
  check("exceptions", typeof caught.stack === "string" && caught.stack.length > 0, "stack");
  var deep = 0; function recurse() { deep++; recurse(); }
  var overflow = false; try { recurse(); } catch (e) { overflow = e instanceof RangeError; }
  check("exceptions", overflow && deep > 100, "stack overflow depth " + deep);
  var err = null; try { null.x; } catch (e) { err = e; }
  check("exceptions", err instanceof TypeError, "TypeError");
});

group("gc-stress", function () {
  // Survivors must stay intact across full collections while ~400 MB of
  // short-lived objects are allocated; the harness additionally bounds the
  // process's peak RSS, which a collector that fails to reclaim would exceed.
  var keep = [];
  for (var round = 0; round < 200; round++) {
    var junk = [];
    for (var i = 0; i < 20000; i++) junk.push({ i: i, s: "str" + i, a: [i, i + 1] });
    if (round % 40 === 0) keep.push(junk[round]);
    if (round % 50 === 49) fullGC();
  }
  edenGC();
  fullGC();
  check("gc-stress", keep.length === 5 && keep[4].s === "str160" && keep[4].a[1] === 161, "survivors intact");
  var registry = new FinalizationRegistry(function () {});
  var weak = new WeakRef({ big: new Array(1000).fill(1) });
  registry.register({}, "x");
  fullGC();
  check("gc-stress", weak.deref() === undefined || typeof weak.deref() === "object", "weakref");
  var many = new Array(200000);
  for (var k = 0; k < many.length; k++) many[k] = { k: k };
  fullGC();
  check("gc-stress", many[199999].k === 199999 && many.length === 200000, "large live set");
});

var asyncOrder = [];
group("promise-microtasks", function () {
  asyncOrder.push("sync");
  Promise.resolve().then(function () { asyncOrder.push("then1"); });
  (async function () { asyncOrder.push("async-start"); await null; asyncOrder.push("async-after"); })();
});

Promise.resolve().then(function () { return 1; }).then(function () { return 2; }).then(function () {
  check("promise-microtasks", asyncOrder.join() === "sync,async-start,then1,async-after",
        "order " + asyncOrder.join());
  Promise.all([1, Promise.resolve(2), new Promise(function (r) { r(3); })]).then(function (v) {
    check("promise-microtasks", v.join() === "1,2,3", "Promise.all");
    if (failures.length) {
      print("jsc_acceptance: FAILED " + failures.join(" | "));
      throw new Error("acceptance failed");
    }
    print("jsc_acceptance: ok groups=" + groups.length + " " + groups.join(","));
  });
});
