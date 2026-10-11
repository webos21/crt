/* Put ahead of the CRT SDK's include directory for the curl-openssl port on macOS only
 * (porting/recipes/curl-openssl.json, target_overrides.macos.include_dirs).
 *
 * curl's configure macro CURL_DARWIN_SYSTEMCONFIGURATION decides "this is macOS, link the
 * CoreFoundation, CoreServices and SystemConfiguration frameworks" by compiling a program that
 * includes <TargetConditionals.h> and requires TARGET_OS_MAC. The CRT SDK ships a TargetConditionals.h
 * that answers "yes, macOS" for consumers that only need the conditionals, so configure linked the
 * frameworks into libcurl (proxy discovery through the host's SystemConfiguration), which the Host
 * ABI firewall does not allow: libcurl must reach the network through the CRT libc alone.
 *
 * This header makes that one probe fail closed. Nothing else in a CRT-built libcurl includes
 * TargetConditionals.h (curl guards it with __APPLE__, which the CRT wrapper leaves undefined),
 * so a stray include becomes a loud build error instead of a silent framework dependency.
 * Removal: a curl option that disables the framework link without disabling IPv6, or an upstream
 * check keyed on the target ABI instead of the host triple. */
#error "CRT: the curl port must not select Apple frameworks (Host ABI firewall)"
