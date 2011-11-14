/*
 * This Cplant(TM) source code is part of the Portals3 Reference
 * Implementation.
 *
 * This Cplant(TM) source code is the property of Sandia National
 * Laboratories.
 *
 * This Cplant(TM) source code is copyrighted by Sandia National
 * Laboratories.
 *
 * The redistribution of this Cplant(TM) source code is subject to the
 * terms of version 2 of the GNU General Public License.
 * (See COPYING, or http://www.gnu.org/licenses/lgpl.html.)
 *
 * Cplant(TM) Copyright 1998-2006 Sandia Corporation. 
 *
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the US Government.
 * Export of this program may require a license from the United States
 * Government.
 */


/* Portals3 is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License,
 * as published by the Free Software Foundation.
 *
 * Portals3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Portals3; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Questions or comments about this library should be sent to:
 *
 * Jim Schutt
 * Sandia National Laboratories, New Mexico
 * P.O. Box 5800
 * Albuquerque, NM 87185-0806
 *
 * jaschut@sandia.gov
 *
 */


#ifndef __LIBTCP_NAL_H__
#define __LIBTCP_NAL_H__

extern nal_create_t p3tcp_create_nal;
extern nal_stop_t p3tcp_stop_nal;
extern nal_destroy_t p3tcp_destroy_nal;
extern pid_ranges_t p3tcp_pid_ranges;

/* This is the maximum number of TCP/IP-capable interfaces that the
 * TCP NAL can simultaneously operate.
 */
#define MAX_P3TCP_IF 4
#define MAX_P3TCP_IFNAME 16

struct p3tcp_data;
typedef struct p3tcp_data p3tcp_data_t;
extern void p3tcp_chan_poll(p3tcp_data_t *d, int poll_cnt);

extern p3tcp_data_t *p3tcp_data(ptl_interface_t type);
extern int p3tcp_init_private(ptl_interface_t type, const lib_ni_t *ni);
extern void p3tcp_libnal_shutdown(p3tcp_data_t *d);

#ifdef PTL_KERNEL_BLD
struct p3tcp_kthread;
void __p3tcp_save_progress_thread(struct p3tcp_data *nal_data,
				  struct p3tcp_kthread *thread);
#endif

#endif /* __LIBTCP_NAL_H__ */
