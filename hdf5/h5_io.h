#ifndef HDF5_FIELD_STORAGE_H
#define HDF5_FIELD_STORAGE_H

#include "base.h"

typedef struct hdf5_file hdf5_file;

hdf5_file *open_hdf5(char *name);
void close_hdf5(hdf5_file *f);
int hdf5_delete_file(char *name);

int hdf5_store_component(hdf5_file *f, const struct em_field *field, enum component c, size_t time_step);
int hdf5_load_component(hdf5_file *f, const struct em_field *field, enum component c, size_t time_step);

#endif
