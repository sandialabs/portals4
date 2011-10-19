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

ptl_process_t *get_desired_mapping(ptl_handle_ni_t ni)
{
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

    MPI_Allgather(&my_id, sizeof(my_id), MPI_BYTE,
                  desired_map_ptr, sizeof(my_id), MPI_BYTE,
                  MPI_COMM_WORLD);

 done:
	return desired_map_ptr;
}

static int is_mpi_initialized = 0;

static void runtime_finalize(void)
{
	if (is_mpi_initialized) {
		is_mpi_initialized = 0;
		MPI_Finalize();
	}
}

void runtime_init(void)
{
	if (!is_mpi_initialized) {
		int v;

		is_mpi_initialized = 1;
		MPI_Init(NULL, NULL);

		MPI_Comm_rank(MPI_COMM_WORLD, &v);
		shmemtest_rank = v;

		MPI_Comm_size(MPI_COMM_WORLD, &v);
		shmemtest_map_size = v;

		atexit(runtime_finalize);
	}
}

