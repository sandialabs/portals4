#include "ptl_test.h"

#include <assert.h>

struct message {
	uint32_t nid;
	uint32_t pid;
	uint32_t rank;
};

static int get_desired_mapping(struct node_info *info)
{
	struct message msg;
	int i, j;
	MPI_Status status;
	ptl_process_t my_id;
	int errs = 0;

	if (PtlGetId(info->ni_handle, &my_id)) {
		printf("PtlGetId failed\n");
		errs ++;
		goto done;
	}

	info->desired_map_ptr = calloc(info->map_size, sizeof(ptl_process_t));
	if (!info->desired_map_ptr) {
		printf("calloc desired map failed\n");
		errs ++;
		goto done;
	}

	info->desired_map_ptr[info->rank] = my_id;

	for (i=0; i<info->map_size; i++) {
		if (i == info->rank) {
			/* Send my info. */
			for (j=0; j<info->map_size; j++) {
				if (j == info->rank)
					continue;
				msg.nid = my_id.phys.nid;
				msg.pid = my_id.phys.pid;
				msg.rank = info->rank;
				MPI_Send(&msg, sizeof(msg), MPI_CHAR, j, 0, MPI_COMM_WORLD);
				//printf("FZ sent from rank %d to rank %d\n", i, j);
			}
		} else {
			/* Get their info. */
			MPI_Recv(&msg, sizeof(msg), MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);
			//printf("FZ recv from rank %d - %x:%x\n", i, msg.nid, msg.pid);

			assert(i == msg.rank);
			
			info->desired_map_ptr[i].phys.nid = msg.nid;
			info->desired_map_ptr[i].phys.pid = msg.pid;
		}
	}

 done:
	return errs;
}

int ompi_rt_init(struct node_info *info)
{
	int errs = 0;
	int v;

	MPI_Comm_rank(MPI_COMM_WORLD, &v);
	info->rank = v;

	MPI_Comm_size(MPI_COMM_WORLD, &v);
	info->map_size = v;

	if (info->ni_handle != PTL_INVALID_HANDLE) {
		if (!info->desired_map_ptr)
			errs += get_desired_mapping(info);
	}

	return errs;
}
