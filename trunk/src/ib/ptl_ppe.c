/**
 * @file ptl_ppe.c
 *
 * Helper code for ppe integration
 */

#include "ptl_loc.h"
#include "ptl_ppe.h"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

static char *socket_name = NULL;

char*
ptl_ppe_socket_name(void)
{
    uid_t uid;
    struct passwd* pw;

    if (NULL != socket_name) {
        return socket_name;
    }

    uid = getuid();
    pw = getpwuid(uid);
    if (NULL != pw && NULL != pw->pw_name) {
        asprintf(&socket_name, "/tmp/portals-ppe-%s", pw->pw_name);
    } else {
        asprintf(&socket_name, "/tmp/portals-ppe-%d", (int) uid);
    }
    
    return socket_name;
}
