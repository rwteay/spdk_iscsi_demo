#
# Copyright(c) 2019-2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from ctypes import c_int
from random import randrange

import pytest

from pyocf.types.cache import Cache, Core
from pyocf.types.data import Data
from pyocf.types.io import IoDir
from pyocf.types.queue import Queue
from pyocf.types.shared import OcfCompletion
from pyocf.types.volume import Volume, RamVolume
from pyocf.types.volume_core import CoreVolume
from pyocf.utils import Size


@pytest.mark.security
def test_neg_write_too_long_data(pyocf_ctx, c_uint16_randomize):
    """
        Check if writing data larger than exported object size is properly blocked
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(1))
    data = Data(int(Size.from_KiB(c_uint16_randomize)))
    completion = io_operation(vol, queue, data, IoDir.WRITE)

    if c_uint16_randomize > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


@pytest.mark.security
def test_neg_read_too_long_data(pyocf_ctx, c_uint16_randomize):
    """
        Check if reading data larger than exported object size is properly blocked
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(1))
    data = Data(int(Size.from_KiB(c_uint16_randomize)))
    completion = io_operation(vol, queue, data, IoDir.READ)

    if c_uint16_randomize > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


@pytest.mark.security
def test_neg_write_too_far(pyocf_ctx, c_uint16_randomize):
    """
        Check if writing data which would normally fit on exported object is
        blocked when offset is set so that data goes over exported device end
    """

    limited_size = c_uint16_randomize % (int(Size.from_KiB(4)) + 1)
    vol, queue = prepare_cache_and_core(Size.from_MiB(4))
    data = Data(int(Size.from_KiB(limited_size)))
    completion = io_operation(vol, queue, data, IoDir.WRITE, int(Size.from_MiB(3)))

    if limited_size > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


@pytest.mark.security
def test_neg_read_too_far(pyocf_ctx, c_uint16_randomize):
    """
        Check if reading data which would normally fit on exported object is
        blocked when offset is set so that data is read beyond exported device end
    """

    limited_size = c_uint16_randomize % (int(Size.from_KiB(4)) + 1)
    vol, queue = prepare_cache_and_core(Size.from_MiB(4))
    data = Data(int(Size.from_KiB(limited_size)))
    completion = io_operation(vol, queue, data, IoDir.READ, offset=(Size.from_MiB(3)))

    if limited_size > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


@pytest.mark.security
def test_neg_write_offset_outside_of_device(pyocf_ctx, c_int_sector_randomize):
    """
        Check that write operations are blocked when
        IO offset is located outside of device range
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(2))
    data = Data(int(Size.from_KiB(1)))
    completion = io_operation(vol, queue, data, IoDir.WRITE, offset=c_int_sector_randomize)

    if 0 <= c_int_sector_randomize <= int(Size.from_MiB(2)) - int(Size.from_KiB(1)):
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


@pytest.mark.security
def test_neg_read_offset_outside_of_device(pyocf_ctx, c_int_sector_randomize):
    """
        Check that read operations are blocked when
        IO offset is located outside of device range
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(2))
    data = Data(int(Size.from_KiB(1)))
    completion = io_operation(vol, queue, data, IoDir.READ, offset=c_int_sector_randomize)

    if 0 <= c_int_sector_randomize <= int(Size.from_MiB(2)) - int(Size.from_KiB(1)):
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


@pytest.mark.security
def test_neg_offset_unaligned(pyocf_ctx, c_int_randomize):
    """
        Check that write operations are blocked when
        IO offset is not aligned
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(2))
    vol = vol.get_front_volume()
    data = Data(int(Size.from_KiB(1)))
    if c_int_randomize % 512 != 0:
        with pytest.raises(Exception):
            vol.new_io(queue, c_int_randomize, data.size, IoDir.WRITE, 0, 0)


@pytest.mark.security
def test_neg_size_unaligned(pyocf_ctx, c_uint16_randomize):
    """
        Check that write operations are blocked when
        IO size is not aligned
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(2))
    data = Data(int(Size.from_B(c_uint16_randomize)))
    if c_uint16_randomize % 512 != 0:
        with pytest.raises(Exception):
            vol.new_io(queue, 0, data.size, IoDir.WRITE, 0, 0)


@pytest.mark.security
def test_neg_io_class(pyocf_ctx, c_int_randomize):
    """
        Check that IO operations are blocked when IO class
        number is not in allowed values {0, ..., 32}
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(2))
    data = Data(int(Size.from_MiB(1)))
    completion = io_operation(vol, queue, data, randrange(0, 2), io_class=c_int_randomize)

    if 0 <= c_int_randomize <= 32:
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


@pytest.mark.security
def test_neg_io_direction(pyocf_ctx, c_int_randomize):
    """
        Check that IO operations are not executed for unknown IO direction,
        that is when IO direction value is not in allowed values {0, 1}
    """

    vol, queue = prepare_cache_and_core(Size.from_MiB(2))
    data = Data(int(Size.from_MiB(1)))
    completion = io_operation(vol, queue, data, c_int_randomize)

    if c_int_randomize in [0, 1]:
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


def prepare_cache_and_core(core_size: Size, cache_size: Size = Size.from_MiB(50)):
    cache_device = RamVolume(cache_size)
    core_device = RamVolume(core_size)

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)
    vol = CoreVolume(core)
    queue = cache.get_default_queue()

    return vol, queue


def io_operation(
    vol: Volume, queue: Queue, data: Data, io_direction: int, offset: int = 0, io_class: int = 0,
):
    vol.open()
    io = vol.new_io(queue, offset, data.size, io_direction, io_class, 0)
    io.set_data(data)

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()
    vol.close()
    return completion
