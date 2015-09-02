"""
streamswitch.control.sources.rtsp_source
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream factory of the default RTSP source type

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..stream_mngr import register_source_type, SourceProcessStream
from ...utils.exceptions import ExecutableNotFoundError
from ...utils.utils import find_executable
from ...utils.process_mngr import kill_all

RTSP_SOURCE_PROGRAM_NAME = "stsw_rtsp_source"


class RtspSourceStream(SourceProcessStream):
    _executable = RTSP_SOURCE_PROGRAM_NAME


def register_rtsp_source_type(type_name="rtsp"):
    if find_executable(RTSP_SOURCE_PROGRAM_NAME) is None:
        raise ExecutableNotFoundError(RTSP_SOURCE_PROGRAM_NAME)
    register_source_type(type_name, RtspSourceStream)
    try:
        kill_all(RTSP_SOURCE_PROGRAM_NAME)
    except Exception:
        pass


if __name__ == "__main__":
    register_rtsp_source_type()