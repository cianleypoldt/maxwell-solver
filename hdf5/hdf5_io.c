#include "hdf5_io.h"
#include "hdf5.h"

//
//   meta/
//      Nx,Ny,Nz
//      dSx, dSy, dSz
//2
//
//
//

int create_file(const char *name) {
    hid_t file = H5Fcreate(
        name,
        H5F_ACC_TRUNC,
        H5P_DEFAULT,
        H5P_DEFAULT
    );

    if (file < 0) {
        fprintf(stderr, "Failed to create file\n");
        return 1;
    }

    hsize_t dims[1] = {5};

    hid_t dataspace = H5Screate_simple(
        1,
        dims,
        NULL
    );

    if (dataspace < 0) {
        H5Fclose(file);
        return 1;
    }
}

void open_file();

void close_file();
