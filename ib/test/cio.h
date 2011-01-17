/* control IO */

#ifndef CIO_H
#define CIO_H

int cio_init();

void cio_cleanup();

xmlDocPtr cio_get_input(char *filename);

void cio_free_input(xmlDocPtr doc);

#endif /* CIO_H */
