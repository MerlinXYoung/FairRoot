 ################################################################################
 #    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
 #                                                                              #
 #              This software is distributed under the terms of the             # 
 #         GNU Lesser General Public Licence version 3 (LGPL) version 3,        #  
 #                  copied verbatim in the file "LICENSE"                       #
 ################################################################################
# Create a library called "libPixel" which includes the source files given in
# the array .
# The extension is already found.  Any number of sources could be listed here.

set(INCLUDE_DIRECTORIES
  ${BASE_INCLUDE_DIRECTORIES}
  ${CMAKE_SOURCE_DIR}/fairmq
  ${CMAKE_SOURCE_DIR}/fairmq/options
  ${CMAKE_SOURCE_DIR}/base/MQ/policies/Serialization
  ${CMAKE_SOURCE_DIR}/examples/MQ/9-PixelDetector/src
  ${CMAKE_SOURCE_DIR}/examples/MQ/9-PixelDetector/src/devices
  ${CMAKE_SOURCE_DIR}/examples/common/mcstack
)


Set(SYSTEM_INCLUDE_DIRECTORIES
    ${SYSTEM_INCLUDE_DIRECTORIES}
    ${ZeroMQ_INCLUDE_DIR}
)

include_directories( ${INCLUDE_DIRECTORIES})
Include_Directories(SYSTEM ${SYSTEM_INCLUDE_DIRECTORIES})

set(LINK_DIRECTORIES
${ROOT_LIBRARY_DIR}
${Boost_LIBRARY_DIRS}
) 

link_directories( ${LINK_DIRECTORIES})

Set(NO_DICT_SRCS
devices/FairMQEx9Sampler.cxx
devices/FairMQEx9SamplerBin.cxx
devices/PixelFindHitsTask.cxx
devices/FairMQEx9Merger.cxx
devices/FairMQEx9FileSink.cxx
devices/FairMQEx9FileSinkBin.cxx
)

Set(NO_DICT_HEADERS
devices/FairMQEx9Sampler.h
devices/FairMQEx9SamplerBin.h
devices/PixelFindHitsTask.h
devices/FairMQEx9Merger.h
devices/FairMQEx9FileSink.h
devices/FairMQEx9FileSinkBin.h
devices/FairMQEx9TaskProcessor.h
devices/FairMQEx9TaskProcessor.tpl
devices/FairMQEx9TaskProcessorBin.h
devices/FairMQEx9TaskProcessorBin.tpl
)

set(SRCS
Pixel.cxx
PixelContFact.cxx
PixelDigitize.cxx
PixelDigiPar.cxx
PixelDigiSource.cxx
PixelDigiBinSource.cxx
PixelDigiWriteToFile.cxx
PixelDigiWriteToBinFile.cxx
PixelFindHits.cxx
PixelFindTracks.cxx
PixelFitTracks.cxx
PixelGeo.cxx
PixelGeoPar.cxx

PixelEventHeader.cxx
PixelPoint.cxx
PixelDigi.cxx
PixelHit.cxx
PixelTrack.cxx
)

Set(HEADERS 
Pixel.h
PixelContFact.h
PixelDigitize.h
PixelDigiPar.h
PixelDigiSource.h
PixelDigiBinSource.h
PixelDigiWriteToFile.h
PixelDigiWriteToBinFile.h
PixelFindHits.h
PixelFindTracks.h
PixelFitTracks.h
PixelGeo.h
PixelGeoPar.h

PixelEventHeader.h
PixelPoint.h
PixelDigi.h
PixelHit.h
PixelTrack.h

PixelPayload.h
)

Set(LINKDEF PixelLinkDef.h)
Set(LIBRARY_NAME Pixel)
Set(DEPENDENCIES
    Base 
    GeoBase
    ParBase
    MCStack
    FairMQ 
    BaseMQ 
    Gen
    Passive
    ${Boost_THREAD_LIBRARY} 
    ${Boost_SYSTEM_LIBRARY} 
    ${Boost_SERIALIZATION_LIBRARY} 
   ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

GENERATE_LIBRARY()

Install(FILES ${NO_DICT_HEADERS} DESTINATION include)
