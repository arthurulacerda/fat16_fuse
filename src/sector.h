#ifndef SECTOR_H
#define SECTOR_H

#include <stdio.h>

/* Read the sector 'secnum' from the image to the buffer */
void sector_read(FILE *fd, unsigned int secnum, void *buffer);

#endif
