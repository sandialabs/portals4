/*
 * ptl_rt.c
 */

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "portals4.h"
#include "portals4_runtime.h"

int shmemtest_rank;
int shmemtest_map_size;

static int debug = 0;

struct message {
	uint32_t nid;
	uint32_t pid;
	uint32_t rank;
};

ptl_process_t *get_desired_mapping(ptl_handle_ni_t ni)
{
	struct message msg;
	int i, j;
	MPI_Status status;
	ptl_process_t my_id;
	ptl_process_t *desired_map_ptr = NULL;

	if (PtlGetId(ni, &my_id)) {
		printf("PtlGetId failed\n");
		goto done;
	}

	desired_map_ptr = calloc(shmemtest_map_size, sizeof(ptl_process_t));
	if (!desired_map_ptr) {
		printf("calloc desired map failed\n");
		goto done;
	}

	desired_map_ptr[shmemtest_rank] = my_id;

	for (i=0; i<shmemtest_map_size; i++) {
		if (i == shmemtest_rank) {
			/* Send my info. */
			for (j=0; j<shmemtest_map_size; j++) {
				if (j == shmemtest_rank)
					continue;
				msg.nid = my_id.phys.nid;
				msg.pid = my_id.phys.pid;
				msg.rank = shmemtest_rank;
				MPI_Send(&msg, sizeof(msg), MPI_CHAR, j, 0, MPI_COMM_WORLD);
				if (debug)
					printf("sent desired mapping[%d].nid = %x, pid = %x\n", i, msg.nid, msg.pid);
			}
		} else {
			/* Get their info. */
			MPI_Recv(&msg, sizeof(msg), MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);

			assert(i == msg.rank);
			
			desired_map_ptr[i].phys.nid = msg.nid;
			desired_map_ptr[i].phys.pid = msg.pid;
			if (debug)
				printf("received desired mapping[%d].nid = %x, pid = %x\n", i, msg.nid, msg.pid);
		}
	}

 done:
	return desired_map_ptr;
}

static int is_mpi_initialized = 0;

void init_mpi(int *argc, char ***argv)
{
	if (!is_mpi_initialized) {
		int v;

		is_mpi_initialized = 1;
		MPI_Init(argc, argv);

		MPI_Comm_rank(MPI_COMM_WORLD, &v);
		shmemtest_rank = v;

		MPI_Comm_size(MPI_COMM_WORLD, &v);
		shmemtest_map_size = v;
	}
}

void fini_mpi(void)
{
	if (is_mpi_initialized) {
		is_mpi_initialized = 0;
		MPI_Finalize();
	}
}

