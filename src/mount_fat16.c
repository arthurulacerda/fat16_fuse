#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "sector.h"
#include "log.h"

//------------------------------------------------------------------------------

void *fat16_init(struct fuse_conn_info *conn)
{
  log_msg("Chamando init\n");

  // Your code here 

  return NULL;
}

void fat16_destroy(void *data)
{
  log_msg("Chamando destroy\n");
  
  // Your code here
}

//------------------------------------------------------------------------------

struct fuse_operations fat16_oper = {
  .init       = fat16_init,
  .destroy    = fat16_destroy
};


//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  int ret;
  
  log_open();

  ret = fuse_main(argc, argv, &fat16_oper, NULL);

  log_msg("ret: %d\n", ret);

  return ret;
}
