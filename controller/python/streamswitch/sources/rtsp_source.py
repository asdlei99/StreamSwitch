"""
streamswitch.sources.rtsp_source
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream factory of the default RTSP source type

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..stream_mngr import register_source_type, SourceProcessStream
from ..exceptions import ExecutableNotFoundError
from ..utils import find_executable
from ..process_mngr import kill_all

RTSP_SOURCE_PROGRAM_NAME = "stsw_rtsp_source"
RTSP_SOURCE_TYPE_NAME = "rtsp_source"

class RtspSourceStream(SourceProcessStream):
    _executable = RTSP_SOURCE_PROGRAM_NAME


