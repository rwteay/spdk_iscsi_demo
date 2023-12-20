#
# Copyright(c) 2020-2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from ctypes import c_int
from random import shuffle, choice
from time import sleep
import pytest

from pyocf.types.cache import Cache, CacheMode
from pyocf.types.core import Core
from pyocf.types.volume import RamVolume
from pyocf.types.volume_core import CoreVolume
from pyocf.types.data import Data
from pyocf.types.io import IoDir
from pyocf.utils import Size
from pyocf.types.shared import OcfCompletion, SeqCutOffPolicy


class Stream:
    def __init__(self, last, length, direction):
        self.last = last
        self.length = length
        self.direction = direction

    def __repr__(self):
        return f"{self.last} {self.length} {self.direction}"


def _io(vol, queue, addr, size, direction, context):
    comp = OcfCompletion([("error", c_int)], context=context)
    data = Data(size)

    io = vol.new_io(queue, addr, size, direction, 0, 0)
    io.set_data(data)
    io.callback = comp.callback
    io.submit()

    return comp


def io_to_streams(vol, queue, streams, io_size):
    completions = []
    for stream in streams:
        completions.append(
            _io(vol, queue, stream.last, io_size, stream.direction, context=(io_size, stream))
        )

    for c in completions:
        c.wait()
        io_size, stream = c.context

        stream.last += io_size
        stream.length += io_size

        assert not c.results["error"], "No IO should fail"


def test_seq_cutoff_max_streams(pyocf_ctx):
    """
    Test number of sequential streams tracked by OCF.

    MAX_STREAMS is the maximal amount of streams which OCF is able to track.

    1. Issue MAX_STREAMS requests (write or reads) to cache, 1 sector shorter than
        seq cutoff threshold
    2. Issue MAX_STREAMS-1 requests continuing the streams from 1. to surpass the threshold and
        check if cutoff was triggered (requests used PT engine)
    3. Issue single request to stream not used in 1. or 2. and check if it's been handled by cache
    4. Issue single request to stream least recently used in 1. and 2. and check if it's been
        handled by cache. It should no longer be tracked by OCF, because of request in step 3. which
        overflowed the OCF handling structure)
    """
    MAX_STREAMS = 128
    TEST_STREAMS = MAX_STREAMS + 1  # Number of streams used by test - one more than OCF can track
    core_size = Size.from_MiB(200)
    threshold = Size.from_KiB(4)

    streams = [
        Stream(
            last=Size((stream_no * int(core_size) // TEST_STREAMS), sector_aligned=True),
            length=Size(0),
            direction=choice(list(IoDir)),
        )
        for stream_no in range(TEST_STREAMS)
    ]  # Generate MAX_STREAMS + 1 non-overlapping streams

    # Remove one stream - this is the one we are going to use to overflow OCF tracking structure
    # in step 3
    non_active_stream = choice(streams)
    streams.remove(non_active_stream)

    cache = Cache.start_on_device(RamVolume(Size.from_MiB(200)), cache_mode=CacheMode.WT)
    core = Core.using_device(RamVolume(core_size), seq_cutoff_promotion_count=1)

    cache.add_core(core)
    vol = CoreVolume(core)
    queue = cache.get_default_queue()

    cache.set_seq_cut_off_policy(SeqCutOffPolicy.ALWAYS)
    cache.set_seq_cut_off_threshold(threshold)

    # STEP 1
    vol.open()
    shuffle(streams)
    io_size = threshold - Size.from_sector(1)
    io_to_streams(vol, queue, streams, io_size)

    stats = cache.get_stats()
    assert (
        stats["req"]["serviced"]["value"] == stats["req"]["total"]["value"] == len(streams)
    ), "All request should be serviced - no cutoff"

    old_serviced = len(streams)

    # STEP 2
    lru_stream = streams[0]
    streams.remove(lru_stream)

    shuffle(streams)
    io_to_streams(vol, queue, streams, Size.from_sector(1))

    stats = cache.get_stats()
    assert (
        stats["req"]["serviced"]["value"] == old_serviced
    ), "Serviced requests stat should not increase - cutoff engaged for all"
    assert stats["req"]["wr_pt"]["value"] + stats["req"]["rd_pt"]["value"] == len(
        streams
    ), "All streams should be handled in PT - cutoff engaged for all streams"

    # STEP 3
    io_to_streams(vol, queue, [non_active_stream], Size.from_sector(1))

    stats = cache.get_stats()
    assert (
        stats["req"]["serviced"]["value"] == old_serviced + 1
    ), "This request should be serviced by cache - no cutoff for inactive stream"

    # STEP 4
    io_to_streams(vol, queue, [lru_stream], Size.from_sector(1))

    vol.close()
    stats = cache.get_stats()

    assert (
        stats["req"]["serviced"]["value"] == old_serviced + 2
    ), "This request should be serviced by cache - lru_stream should be no longer tracked"
