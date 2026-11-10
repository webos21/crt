// Keep TLS in a DSO so executable-local relaxation cannot mask the import policy.
static thread_local int slot = 7;
extern "C" int crt_shared_tls(int replacement) {
  int previous = slot;
  slot = replacement;
  return previous;
}
extern "C" void crt_shared_throw() { throw 73; }
