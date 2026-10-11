/* libxml2 parse/XPath/serialise round trip for the libxml2 port (porting/recipes/libxml2.json). */
#include <stdio.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

int main(void) {
  static const char document[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<crt version=\"1\"><item id=\"a\">one</item><item id=\"b\">t\xC3\xB6\xC3\xB6</item></crt>";
  xmlDocPtr doc = xmlReadMemory(document, (int)strlen(document), "memory.xml", NULL, 0);
  xmlXPathContextPtr context;
  xmlXPathObjectPtr result;
  xmlChar* text;
  xmlChar* dumped = NULL;
  int dumped_size = 0;

  if (doc == NULL) {
    fprintf(stderr, "libxml2_parse_test: parse failed\n");
    return 1;
  }
  if (strcmp((const char*)xmlDocGetRootElement(doc)->name, "crt") != 0) {
    fprintf(stderr, "libxml2_parse_test: wrong root\n");
    return 1;
  }
  context = xmlXPathNewContext(doc);
  result = xmlXPathEvalExpression((const xmlChar*)"/crt/item[@id='b']", context);
  if (result == NULL || result->nodesetval == NULL || result->nodesetval->nodeNr != 1) {
    fprintf(stderr, "libxml2_parse_test: XPath found no node\n");
    return 1;
  }
  text = xmlNodeGetContent(result->nodesetval->nodeTab[0]);
  if (text == NULL || strcmp((const char*)text, "t\xC3\xB6\xC3\xB6") != 0) {
    fprintf(stderr, "libxml2_parse_test: wrong content\n");
    return 1;
  }
  xmlFree(text);
  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlDocDumpMemory(doc, &dumped, &dumped_size);
  if (dumped == NULL || strstr((const char*)dumped, "<item id=\"a\">one</item>") == NULL) {
    fprintf(stderr, "libxml2_parse_test: serialisation lost an element\n");
    return 1;
  }
  xmlFree(dumped);
  xmlFreeDoc(doc);
  xmlCleanupParser();
  printf("libxml2_parse_test: ok (libxml2 %s)\n", xmlParserVersion);
  return 0;
}
