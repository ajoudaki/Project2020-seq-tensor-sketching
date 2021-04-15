# Contains sketch base classes and helper methods

import random

import numpy as np
import numba as nb
from numba import njit
from numba.experimental import jitclass
from numba.typed import List

from sequence import *

# A SketchedSequence contains a sequence and its sketch.
# The sketch must be a 1D array of float32s.
@jitclass([('seq', Sequence_type), ('sketch', nb.float32[::1])])
class SketchedSequence:
    def __init__(self, seq: Sequence, sketch):
        self.seq = seq
        self.sketch = sketch


SketchedSequence_type = SketchedSequence.class_type.instance_type

# Compute the Euclidean distance between two sketched sequences.
@njit
def dist(ss1: np.ndarray, ss2: np.ndarray) -> np.float32:
    return np.linalg.norm(ss1.sketch - ss2.sketch)


# Return a sorted list of (dist, seq1, seq2).
@njit
def pairwise_dists(seqs: list[Sequence]) -> list[tuple[np.float32, Sequence, Sequence]]:
    d = []
    for s1, s2 in itertools.combinations(seqs, 2):
        d.append((dist(s1.sketch, s2.sketch), s1, s2))
    d.sort(key=lambda tup: tup[0])
    return d


sketchparams_spec = [('A', nb.int32), ('t', nb.int32), ('D', nb.int32), ('normalize', nb.bool_)]


@jitclass(sketchparams_spec)
class SketchParams:
    def __init__(self, A, t, D, normalize=True):
        self.A = A
        self.t = t
        self.D = D
        self.normalize = normalize


SketchParams_type = SketchParams.class_type.instance_type


# NOTE: Sketchers are not always jitted, since e.g. CUDA invocations do not support this.
class Sketcher:
    def __init__(self, params: SketchParams):
        self.A = params.A
        self.t = params.t
        self.D = params.D
        self.normalize = params.normalize

    # [Optional] sketch a single sequence for all t' <= t.
    def _full_sketch(self, seq: Sequence):
        pass

    # Sketch a single sequence.
    def sketch_one(self, seq: Sequence) -> SketchedSequence:
        pass

    # Sketch a list of sequences.
    def sketch(self, seqs: list[Sequence]) -> list[SketchedSequence]:
        pass