#!/usr/bin/env bash

set -evx

ARCH=$(uname -m)

env | sort

if [[ "${ARCH}" == "x86_64" ]]; then
    cp $CI_PROJECT_DIR/ci/el8.repo.internal.geedge.net.repo /etc/yum.repos.d
    yum install -y libmaatframe-devel libMESA_prof_load-devel
    yum install -y libuuid-devel librdkafka-devel
else
    yum install -y libmaatframe-devel libMESA_prof_load-devel
    yum install -y libuuid-devel librdkafka-devel
fi

pip3 install msgpack