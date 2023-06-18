#!/usr/bin/env bash
# $1: image name
docker run --name dev_workflow -it --rm -v $(greadlink -f ..):/workflow -p $2:$3 $1