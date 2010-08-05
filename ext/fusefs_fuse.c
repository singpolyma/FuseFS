/* fusefs_fuse.c */

/* This is rewriting most of the things that occur
 * in fuse_main up through fuse_loop */

#define FUSE_USE_VERSION 22
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
/* #include "fuse_i.h"
#include "fuse_compat.h"
#include "fuse_kernel.h"
#include "fuse_kernel_compat5.h" */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <signal.h>

struct fuse * fuse_instance = NULL;
static unsigned int fusefd;
static char *mounted_at = NULL;

static int set_one_signal_handler(int signal, void (*handler)(int));

int fusefs_fd() {
  return fusefd;
}

int
fusefs_unmount() {
  if (fuse_instance)
    fuse_destroy(fuse_instance);
  fuse_instance = NULL;
  if (mounted_at)
    fuse_unmount(mounted_at);
  free(mounted_at);
  fusefd = -1;
}

static void
fusefs_ehandler() {
  if (fuse_instance != NULL) {
    fusefs_unmount();
  }
}

int
fusefs_setup(char *mountpoint, const struct fuse_operations *op, char *opts) {
  char fuse_new_opts[1024];
  char fuse_mount_opts[1024];
  char nopts[1024];

  char *cur;
  char *ptr;

  fuse_new_opts[0] = '\0';
  fuse_mount_opts[0] = '\0';

  for (cur=opts;cur;cur = ptr) {
    ptr = strchr(cur,',');
    if (ptr) *(ptr++) = '\0';
    if (fuse_is_lib_option(cur)) {
      if (fuse_new_opts[0]) {
        strcpy(nopts,fuse_new_opts);
        snprintf(fuse_new_opts,1024,"%s,%s",nopts,cur);
      } else {
        snprintf(fuse_new_opts,1024,"%s",cur);
      }
    } else {
      if (fuse_mount_opts[0]) {
        strcpy(nopts,fuse_mount_opts);
        snprintf(fuse_mount_opts,1024,"%s,%s",nopts,cur);
      } else {
        snprintf(fuse_mount_opts,1024,"%s",cur);
      }
    }
  }

  fusefd = -1;
  if (fuse_instance != NULL) {
    return 0;
  }
  if (mounted_at != NULL) {
    return 0;
  }

  /* First, mount us */
  fusefd = fuse_mount(mountpoint, fuse_mount_opts[0] ? fuse_mount_opts : NULL);
  if (fusefd == -1) return 0;

  fuse_instance = fuse_new(fusefd, fuse_new_opts[0] ? fuse_new_opts : NULL, op, sizeof(*op));
  if (fuse_instance == NULL)
    goto err_unmount;

  /* Set signal handlers */
  if (set_one_signal_handler(SIGHUP, fusefs_ehandler) == -1 ||
      set_one_signal_handler(SIGINT, fusefs_ehandler) == -1 ||
      set_one_signal_handler(SIGTERM, fusefs_ehandler) == -1 ||
      set_one_signal_handler(SIGPIPE, SIG_IGN) == -1)
    return 0;

  atexit(fusefs_ehandler);

  /* We've initialized it! */
  mounted_at = strdup(mountpoint);
  return 1;
err_destroy:
  fuse_destroy(fuse_instance);
err_unmount:
  fuse_unmount(mountpoint);
  return 0;
}

int
fusefs_uid() {
  struct fuse_context *context = fuse_get_context();
  if (context) return context->uid;
  return -1;
}

int
fusefs_gid() {
  struct fuse_context *context = fuse_get_context();
  if (context) return context->gid;
  return -1;
}

int
fusefs_process() {
  /* This gets exactly 1 command out of fuse fd. */
  /* Ideally, this is triggered after a select() returns */
  if (fuse_instance != NULL) {
    struct fuse_cmd *cmd;

    if (fuse_exited(fuse_instance))
      return 0;

    cmd = fuse_read_cmd(fuse_instance);
    if (cmd == NULL)
      return 1;

    fuse_process_cmd(fuse_instance, cmd);
  }
  return 1;
}


static int set_one_signal_handler(int signal, void (*handler)(int))
{
    struct sigaction sa;
    struct sigaction old_sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = handler;
    sigemptyset(&(sa.sa_mask));
    sa.sa_flags = 0;

    if (sigaction(signal, NULL, &old_sa) == -1) {
        perror("FUSE: cannot get old signal handler");
        return -1;
    }

    if (old_sa.sa_handler == SIG_DFL &&
        sigaction(signal, &sa, NULL) == -1) {
        perror("Cannot set signal handler");
        return -1;
    }
    return 0;
}
