# Intersect the reviewed public API with this configuration's definitions.
# FFmpeg-OFF builds legitimately omit codecs/extractors/muxers. Never export
# private crtmedia_* helpers merely because they have the same prefix.
execute_process(COMMAND "${NM}" --defined-only --extern-only "${ARCHIVE}"
  RESULT_VARIABLE status OUTPUT_VARIABLE definitions ERROR_VARIABLE diagnostic)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "cannot inspect crtmedia archive: ${diagnostic}")
endif()
file(STRINGS "${PUBLIC_SYMBOLS}" public_symbols)
set(exports "EXPORTS\n")
foreach(symbol IN LISTS public_symbols)
  if(definitions MATCHES "[ \t]${symbol}(\r?\n|$)")
    string(APPEND exports "${symbol}\n")
  endif()
endforeach()
file(WRITE "${OUTPUT}" "${exports}")
