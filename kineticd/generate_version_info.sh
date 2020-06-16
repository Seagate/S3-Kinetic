#!/bin/sh
echo "Generating src/version_info.h"
cd `dirname $0`

BUILD_DATE=`date`

if [ -z "$(git status --porcelain)" ]; then
    GIT_HASH=$(git rev-parse --short HEAD)
else
    GIT_HASH=$(git rev-parse --short HEAD)-dirty
fi

cat > src/version_info.h <<TPL
#ifndef KINETIC_VERSION_INFO_H_
#define KINETIC_VERSION_INFO_H_
#define BUILD_DATE ("$BUILD_DATE")
#define CURRENT_SEMANTIC_VERSION ("09.02.10")
#define GIT_HASH ("$GIT_HASH")
#define CURRENT_PROTOCOL_VERSION ("$1")
#define KINETIC_PROTO_GIT_HASH ("$2")
#endif  // KINETIC_VERSION_INFO_H_
TPL
