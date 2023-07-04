#!/usr/bin/env bash
# $1: image name
# $2:$3 host port:container port

# --privileged is required so that gdb works properly.
docker run --privileged --name workflow_dev -it --rm -v $(greadlink -f ..):/workflow -p $2:$3 $1
