/* control IO */

#ifndef CIO_H
#define CIO_H

void cio_init(void);

void cio_cleanup(void);

xmlDocPtr cio_get_input(char *filename);

void cio_free_input(xmlDocPtr doc);

#endif /* CIO_H */
