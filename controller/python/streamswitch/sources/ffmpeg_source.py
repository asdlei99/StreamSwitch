"""
streamswitch.sources.ffmpeg_source
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream factory of the ffmpeg source type

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..stream_mngr import register_source_type, SourceProcessStream
from ..exceptions import ExecutableNotFoundError
from ..utils import find_executable
from ..process_mngr import kill_all

FFMPEG_SOURCE_PROGRAM_NAME = "ffmpeg_demuxer_source"
FFMPEG_SOURCE_TYPE_NAME = "ffmpeg_source"

class FFmpegSourceStream(SourceProcessStream):
    _executable = FFMPEG_SOURCE_PROGRAM_NAME


