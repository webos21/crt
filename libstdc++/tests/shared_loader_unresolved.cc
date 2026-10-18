extern "C" int crt_missing_loader_import_negative_control();
extern "C" int crt_call_missing_loader_import() {
  return crt_missing_loader_import_negative_control();
}
