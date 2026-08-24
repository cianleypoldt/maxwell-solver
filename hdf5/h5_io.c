#include "h5_io.h"
#include "base.h"
#include "field.h"
#include <H5Fpublic.h>
#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5public.h>
#include <stdio.h>
#include <stdlib.h>

//
//   meta/
//      Nx,Ny,Nz
//      dSx, dSy, dSz
//

struct hdf5_file {
    hid_t hid;
};

hdf5_file *open_hdf5(char *name) {
    hdf5_file *f = malloc(sizeof(hdf5_file));

    FILE *ftest = fopen(name, "rb");
    if (ftest) {
        fclose(ftest);
        f->hid = H5Fopen(name, H5F_ACC_RDWR, H5P_DEFAULT);
        if (f->hid < 0) goto error;
    } else {
        f->hid = H5Fcreate(name, H5F_ACC_EXCL, H5P_DEFAULT, H5P_DEFAULT);
        if (f->hid < 0) goto error;
        fprintf(stderr, "Created file \"%s\"\n", name);
    }
    return f;

error:
    printf("hdf5_open: Failed to open \"%s\"", name);
    fflush(stdout);
    H5Fclose(f->hid);
    free(f);
    return NULL;
}

void close_hdf5(hdf5_file *f) {
    H5Fclose(f->hid);
    free(f);
}

static const char *component_name(enum component c) {
    switch (c) {
        case EX:
            return "Ex";
        case EY:
            return "Ey";
        case EZ:
            return "Ez";
        case HX:
            return "Hx";
        case HY:
            return "Hy";
        case HZ:
            return "Hz";
        case EPS:
            return "Eps";
        case MU:
            return "Mu";
        case SIGMA:
            return "Sigma";
        default:
            return NULL;
    }
}

int hdf5_store_component(hdf5_file *f, const em_field *field, enum component c, size_t time_step) {
    const char *name = component_name(c);
    float *data = get_em_field_component(field, c);

    hid_t dset;
    htri_t exists = H5Lexists(f->hid, name, H5P_DEFAULT);
    if (!exists) {
        // Create new dataspace, dimension 0 indexed as time step
        hid_t space = H5Screate_simple(
            4,
            (hsize_t[4]){0, field->Nx, field->Ny, field->Nz},
            (hsize_t[4]){H5S_UNLIMITED, field->Nx, field->Ny, field->Nz}
        );
        if (space < 0) goto error;

        hid_t plist = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_chunk(plist, 4, (hsize_t[4]){1, field->Nx, field->Ny, field->Nz});

        H5Pset_alloc_time(plist, H5D_ALLOC_TIME_INCR);  // onlly allocate chunks that are written

        dset = H5Dcreate2(f->hid, name, H5T_NATIVE_FLOAT, space, H5P_DEFAULT, plist, H5P_DEFAULT);

        H5Pclose(plist);
        H5Sclose(space);

        if (dset < 0) goto error;

        fprintf(stderr, "Created dataset \"%s\"\n", name);

    } else {
        dset = H5Dopen2(f->hid, name, H5P_DEFAULT);
        if (dset < 0) {
            fprintf(stderr, "hdf5_store_component: failed to open dataset \"%s\"\n", name);
            goto error;
        }
    }

    // extend time dimension if outgrown
    hid_t cur_space = H5Dget_space(dset);
    hsize_t cur_dims[4], cur_maxdims[4];
    H5Sget_simple_extent_dims(cur_space, cur_dims, cur_maxdims);
    H5Sclose(cur_space);

    if ((hsize_t)time_step >= cur_dims[0]) {
        hsize_t new_dims[4] = {(hsize_t)time_step + 1, cur_dims[1], cur_dims[2], cur_dims[3]};
        if (H5Dset_extent(dset, new_dims) < 0) {
            fprintf(stderr, "hdf5_store_component: failed to extend dataset \"%s\"\n", name);
            goto write_error;
        }
    }

    hid_t filespace = H5Dget_space(dset);
    hsize_t offset[4] = {(hsize_t)time_step, 0, 0, 0};
    hsize_t count[4] = {1, field->Nx, field->Ny, field->Nz};

    if (H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL) < 0) {
        H5Sclose(filespace);
        goto write_error;
    }

    hid_t memspace = H5Screate_simple(4, count, NULL);
    herr_t status = H5Dwrite(dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, data);

    H5Sclose(memspace);
    H5Sclose(filespace);

    if (!(status < 0)) {
        H5Dclose(dset);
        return 1;
    }

write_error:
    H5Dclose(dset);
error:
    fprintf(stderr, "hdf5_store_component: Failed to write to dataset \"%s\" at timestep %zu\n", name, time_step);
    return -1;
}

int hdf5_delete_file(char *name) {
    if (remove(name) != 0) {
        printf("hdf5_delete_file: Unable to delete file \"%s\"", name);
        return -1;
    }
    fprintf(stderr, "Deleted file \"%s\"\n", name);

    return 1;
}

int hdf5_load_component(hdf5_file *f, const em_field *field, enum component c, size_t time_step) {
    const char *name = component_name(c);
    float *data = get_em_field_component(field, c);

    htri_t exists = H5Lexists(f->hid, name, H5P_DEFAULT);
    if (exists <= 0) {
        fprintf(stderr, "hdf5_load_component: dataset \"%s\" does not exist\n", name);
        goto error;
    }

    hid_t dset = H5Dopen2(f->hid, name, H5P_DEFAULT);
    if (dset < 0) goto error;

    hid_t filespace = H5Dget_space(dset);
    hsize_t dims[4], maxdims[4];
    H5Sget_simple_extent_dims(filespace, dims, maxdims);

    /* bounds check: dims/Nx/Ny/Nz must match, and time_step must be within extent */
    if (dims[1] != field->Nx || dims[2] != field->Ny || dims[3] != field->Nz) {
        fprintf(stderr, "hdf5_load_component: shape mismatch for \"%s\" "
                        "(file: %llu x %llu x %llu, field: %zu x %zu x %zu)\n",
                name,
                (unsigned long long)dims[1],
                (unsigned long long)dims[2],
                (unsigned long long)dims[3],
                field->Nx,
                field->Ny,
                field->Nz);
        H5Sclose(filespace);
        H5Dclose(dset);
        goto error;
    }

    if ((hsize_t)time_step >= dims[0]) {
        fprintf(stderr, "hdf5_load_component: timestep %zu out of range (dataset has %llu steps)\n", time_step, (unsigned long long)dims[0]);
        H5Sclose(filespace);
        H5Dclose(dset);
        goto error;
    }

    hsize_t offset[4] = {(hsize_t)time_step, 0, 0, 0};
    hsize_t count[4] = {1, field->Nx, field->Ny, field->Nz};

    if (H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL) < 0) {
        H5Sclose(filespace);
        H5Dclose(dset);
        goto error;
    }

    hid_t memspace = H5Screate_simple(4, count, NULL);

    herr_t status = H5Dread(dset, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, data);

    H5Sclose(memspace);
    H5Sclose(filespace);
    H5Dclose(dset);

    if (status < 0) goto error;

    return 0;

error:
    fprintf(stderr, "hdf5_load_component: Failed to load component \"%s\" at timestep %zu\n", name, time_step);
    return -1;
}

int hdf5_delete_dataset() {
}

int hdf5_load_from_dataset() {
}
