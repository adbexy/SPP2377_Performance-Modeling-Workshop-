#!/bin/bash

set -euo pipefail #exit on [e]rror and [u]nknown variabless, x: print commands, exit on -o pipefail

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

TAG="vmalloc_${USER}"
CONTAINER="${TAG}_cont"
HOST_UID=$(id -u)
HOST_GID=$(id -g)


# the host has to load this module for cpupower to work
# and it has to provide the directory /lib/modules/...
# as at least a readonly bind mount.
sudo modprobe msr
# Detect if the script was started with the sequence: "activates --build"
# Also treat a lone "--build" arg as intent to build the image.

# Only stop/remove the container if it already exists. This avoids errors
# when there's no container with that name.
if sudo docker ps -a --format '{{.Names}}' | grep -wq "${CONTAINER}"; then
	echo "Stopping and removing any existing container $CONTAINER"
	# If it's currently running, stop it first
	if sudo docker ps --format '{{.Names}}' | grep -wq "${CONTAINER}"; then
		sudo docker stop "${CONTAINER}"
	else
		echo "Container $CONTAINER exists but is not running; skipping stop"
	fi
	sudo docker rm "${CONTAINER}"
else
	echo "No existing container named $CONTAINER; nothing to stop/remove"
fi

DO_BUILD=0
args=("$@")
arg_count=${#args[@]}
for ((i=0;i<arg_count;i++)); do
	if [ "${args[i]}" = "--build" ]; then
		DO_BUILD=1
		break
	fi
done

if [ "$DO_BUILD" -eq 1 ]; then
	echo "Building docker image $TAG"
	sudo docker build --tag "$TAG" \
		--build-arg HOST_UID="$HOST_UID" \
		--build-arg HOST_GID="$HOST_GID" \
		.
fi

echo "Starting docker container $CONTAINER from image $TAG"
sudo docker run \
	-it \
	--name $CONTAINER \
	--privileged \
	--cap-add=sys_nice \
	--user "$HOST_UID:$HOST_GID" \
  	-v "$SCRIPT_DIR":/home/user:rw \
	--mount type=bind,src=/lib/modules,target=/lib/modules,readonly \
	$TAG

