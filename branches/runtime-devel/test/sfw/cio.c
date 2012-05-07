/* control IO */

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "ptl_test.h"
#include "cio.h"

/*
 * first pass - trivial implementation just reads one file
 */

void cio_init(void)
{
//	LIBXML_TEST_VERSION
}

void cio_cleanup(void)
{
	xmlCleanupParser();
	xmlMemoryDump();
}

xmlDocPtr cio_get_input(char *filename)
{
	xmlDocPtr doc;

	doc = xmlReadFile(filename, NULL, 0);
	if (!doc)
		printf("unable to read and parse %s\n", filename);

	return doc;
}

void cio_free_input(xmlDocPtr doc)
{
	xmlFreeDoc(doc);
}
