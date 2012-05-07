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

/* These hash functions are at least 3 times faster than those based
 * on val % p, where p is the largest prime smaller than the desired
 * number of hash bins.
 *
 * The worst cases for these hash function seem to be when the hashed
 * values differ by increments that are powers of two. For 4 <= n <= 16
 * this one stacks at most 8 values in a single bin when there are empty
 * bins, except for two cases when it will stack up to 16 values into
 * one bin when some bins are empty.
 */

/* hashes a 32-bit value to an n-bit value, where n<=32
 */
#define hash32(val,n)							\
({									\
	uint32_t __h = val;						\
	__h ^= __h>>6 ^ __h>>11 ^__h>>15 ^__h>>21 ^ __h>>26;		\
	__h = (0xffffffffu>>(32-n)) & __h;				\
	__h;								\
})

/* hashes a 64-bit value to an n-bit value, where n<=32
 */
#define hash64(val,n) (hash32(val^(uint64_t)val>>32, n))

/* Returns the truncated base 2 logarithm of its argument.
 */
static inline size_t tlog2(uint64_t n)
{
	size_t order = 0;
	while ((n = n >> 1)) order++;
	return order;
}
