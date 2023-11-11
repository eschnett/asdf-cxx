#! /usr/bin/env python

from __future__ import print_function
import numpy as np

from asdf import *



def write_external():
    print("Writing external file...")

    # The actual dataset
    alpha = ndarray.create_int64(
        np.array([1, 2, 3], np.int64), block_format_t_inline_array,
        compression_t_none, 0, np.array([]), [3])
    # A local reference
    beta = reference.create_from_path("", ["group", "alpha", "data"])

    grp = group.create(
        {"alpha": entry.create_from_ndarray("alpha", alpha, ""),
         "beta": entry.create_from_reference("beta", beta, "")})

    project = asdf.create_from_group({}, grp)
    project.write("external.asdf")



def write_metadata():
    print("Writing metadata file...")

    # A remote reference
    gamma = reference.create_from_path("external.asdf",
                                       ["group", "alpha", "data"])
    # A local reference
    delta = reference.create_from_path("", ["group", "gamma", "reference"])
    # A remote reference to a local reference
    epsilon = reference.create_from_path("external.asdf",
                                         ["group", "beta", "reference"])

    grp = group.create(
        {"gamma": entry.create_from_reference("gamma", gamma, ""),
         "delta": entry.create_from_reference("delta", delta, ""),
         "epsilon": entry.create_from_reference("epsilon", epsilon, "")})

    project = asdf.create_from_group({}, grp)
    project.write("metadata.asdf")



def read_metadata():
    print("Reading metadata file...")

    project = asdf.read("metadata.asdf")
    grp = project.get_group()

    gamma = grp.get_entries()["gamma"].get_reference()
    print("gamma: <" + gamma.get_target() + ">")
    rs_node = gamma.resolve()
    arr = ndarray.read(rs_node)
    print("gamma': [ndarray] " + str(arr.get_data_vector_int64()))

    delta = grp.get_entries()["delta"].get_reference()
    print("delta: <" + delta.get_target() + ">")
    rs_node = delta.resolve()
    ref = reference.create_from_reader_state_node(rs_node)
    print("delta': [reference] " + ref.get_target())
    rs_node1 = ref.resolve()
    arr = ndarray.read(rs_node1)
    print("delta'': [ndarray] " + str(arr.get_data_vector_int64()))
  
    epsilon = grp.get_entries()["epsilon"].get_reference()
    print("epsilon: <" + epsilon.get_target() + ">")
    rs_node = epsilon.resolve()
    ref = reference.create_from_reader_state_node(rs_node)
    print("epsilon': [reference] " + ref.get_target())
    rs_node1 = ref.resolve()
    arr = ndarray.read(rs_node1)
    print("epsilon'': [ndarray] " + str(arr.get_data_vector_int64()))



print("asdf-demo-external-python:")
print("    Create a simple ASDF file with external references from Python")

write_external()
write_metadata()
read_metadata()

print("Done.")
