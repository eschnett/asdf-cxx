#! /usr/bin/env python

from __future__ import print_function
import numpy as np

from asdf import *

print("asdf-demo-python: Create a simple ASDF file from Python")

array0d = ndarray.create_int64(np.array([42], np.int64),
                               block_format_t_inline_array,
                               compression_t_none,
                               0,
                               np.array([]),
                               np.array([]))
ent0 = entry.create_from_ndarray("alpha", array0d, "")

array1d = ndarray.create_int64(np.array([1, 2, 3], np.int64),
                               block_format_t_block,
                               compression_t_none,
                               0,
                               np.array([]),
                               np.array([3]))
ent1 = entry.create_from_ndarray("beta", array1d, "")

array2d = ndarray.create_float64(np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0],
                                          np.float64),
                                 block_format_t_inline_array,
                                 compression_t_none, 0,
                                 np.array([]),
                                 np.array([2, 3]))
ent2 = entry.create_from_ndarray("gamma", array2d, "")

array3d = ndarray.create_complex128(np.array([1, -2, 3j, -4j, 5 + 1j, 6 - 1j],
                                             np.complex128),
                                    block_format_t_block,
                                    compression_t_bzip2,
                                    9,
                                    np.array([]),
                                    np.array([1, 2, 3]))
ent3 = entry.create_from_ndarray("delta", array3d, "")

array8d = ndarray.create_bool(np.array([True], np.int),
                              block_format_t_block,
                              compression_t_zlib,
                              9,
                              np.array([]),
                              np.array([1, 1, 1, 1, 1, 1, 1, 1]))
ent8 = entry.create_from_ndarray("epsilon", array8d, "")

seq = sequence.create([ent0, ent1, ent2])
ents = entry.create_from_sequence("zeta", seq, "")

ref = reference.create_from_path("", ["group", "1"])
entr = entry.create_from_reference("eta", ref, "")

grp = group.create({"alpha": ent0,
                    "beta": ent1,
                    "gamma": ent2,
                    "delta": ent3,
                    "epsilon": ent8,
                    "zeta": ents,
                    "eta": entr})

project = asdf.create_from_group({}, grp)
project.write("demo-python.asdf")

print("Done.")
