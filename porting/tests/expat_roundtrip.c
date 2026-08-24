#include <stdio.h>
#include <string.h>

#include <expat.h>

static const char kDocument[] =
    "<?xml version=\"1.0\"?>\n"
    "<crt-expat greeting=\"hello\">\n"
    "  <item id=\"1\">alpha</item>\n"
    "  <item id=\"2\">beta</item>\n"
    "</crt-expat>\n";

static int root_seen = 0;
static int item_count = 0;
static char greeting_attr[32];
static char item_text[2][32];
static int item_text_len[2];
static int depth = 0;

static void XMLCALL start_element(void *user_data, const XML_Char *name, const XML_Char **atts) {
  (void)user_data;
  if (strcmp(name, "crt-expat") == 0) {
    root_seen = 1;
    for (int i = 0; atts[i] != NULL; i += 2) {
      if (strcmp(atts[i], "greeting") == 0) {
        snprintf(greeting_attr, sizeof(greeting_attr), "%s", atts[i + 1]);
      }
    }
  } else if (strcmp(name, "item") == 0 && item_count < 2) {
    item_text_len[item_count] = 0;
    item_text[item_count][0] = '\0';
  }
  depth++;
}

static void XMLCALL end_element(void *user_data, const XML_Char *name) {
  (void)user_data;
  depth--;
  if (strcmp(name, "item") == 0 && item_count < 2) {
    item_count++;
  }
}

static void XMLCALL char_data(void *user_data, const XML_Char *s, int len) {
  (void)user_data;
  if (depth == 2 && item_count < 2) {
    int *cur_len = &item_text_len[item_count];
    char *buf = item_text[item_count];
    int space = (int)sizeof(item_text[0]) - 1 - *cur_len;
    int copy_len = len < space ? len : space;
    if (copy_len > 0) {
      memcpy(buf + *cur_len, s, (size_t)copy_len);
      *cur_len += copy_len;
      buf[*cur_len] = '\0';
    }
  }
}

int main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    printf("expat_roundtrip_test: XML_ParserCreate failed\n");
    return 1;
  }

  XML_SetElementHandler(parser, start_element, end_element);
  XML_SetCharacterDataHandler(parser, char_data);

  enum XML_Status status = XML_Parse(parser, kDocument, (int)strlen(kDocument), 1);
  if (status != XML_STATUS_OK) {
    printf("expat_roundtrip_test: parse failed: %s (line %lu)\n",
           XML_ErrorString(XML_GetErrorCode(parser)),
           (unsigned long)XML_GetCurrentLineNumber(parser));
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);

  if (!root_seen) {
    printf("expat_roundtrip_test: root element not seen\n");
    return 1;
  }
  if (strcmp(greeting_attr, "hello") != 0) {
    printf("expat_roundtrip_test: attribute mismatch (got '%s')\n", greeting_attr);
    return 1;
  }
  if (item_count != 2) {
    printf("expat_roundtrip_test: item count mismatch (got %d)\n", item_count);
    return 1;
  }
  if (strcmp(item_text[0], "alpha") != 0 || strcmp(item_text[1], "beta") != 0) {
    printf("expat_roundtrip_test: item text mismatch (got '%s', '%s')\n", item_text[0], item_text[1]);
    return 1;
  }

  XML_Expat_Version version = XML_ExpatVersionInfo();
  printf("expat_roundtrip_test: ok items=%d version=%d.%d.%d\n", item_count,
         version.major, version.minor, version.micro);
  return 0;
}
