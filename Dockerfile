FROM debian:12 AS base

# disable requests for user input of the following code
ENV DEBIAN_FRONTEND=noninteractive

# add testing packages (for linux-cpupower)
RUN echo "deb http://deb.debian.org/debian/ testing non-free contrib main" \
	>> /etc/apt/sources.list
# update apt-get packet lists, only strictly necessary for cpu-power,
# which uses the testing package repos
RUN apt-get update

## install debian packages
# used in installation
RUN apt-get install -y apt-utils
# newer gcc and g++
RUN apt-get install -y gcc-12 g++-12
# cmake for compiling, git to get commit and diff, gdb not actively used
RUN apt-get install -y cmake git gdb
# used when debugging docker problems
RUN apt-get install -y vim tmux
# NUMA configuration
RUN apt-get install -y numactl libnuma-dev
# no idea where we use these
RUN apt-get install -y uuid uuid-dev
# for turning on DSA (see DSA/benchmarks/turn_on_dsa.sh, called from other sites)
RUN apt-get install -y accel-config
# to change scheduler to performance
RUN apt-get install -y linux-cpupower
# install modprobe, used by cpupower as installed by linux-cpupower and by dsa-in-odd.sh
RUN apt-get install -y kmod
# for plotting and SIMDOpewrators generation
RUN apt-get install -y python3-seaborn python3-jinja2
RUN apt-get install -y python3-networkx python3-wget

RUN apt-get install -y openssh-client

# delete apt-get packet lists
RUN rm -rf /var/lib/apt-get/lists/*

# create shortcuts/links to the executables
RUN rm -f /usr/bin/g++ /usr/bin/gcc
RUN ln --symbolic /usr/bin/g++-12 /usr/bin/g++
RUN ln --symbolic /usr/bin/gcc-12 /usr/bin/gcc

# create working directory to bind volumes
RUN mkdir /home/user
WORKDIR /home/user


# Allow building the image with a host UID/GID so a matching user exists in
# /etc/passwd. This prevents errors when the container is run with
# --user host_uid:host_gid and tools (git, python packages, etc.) try to
# resolve the numeric UID to a username.
ARG HOST_UID=1000
ARG HOST_GID=1000
RUN set -eux; \
	groupadd -g "${HOST_GID}" user || true; \
	useradd -m -u "${HOST_UID}" -g "${HOST_GID}" -s /bin/bash user || true; \
	chown -R "${HOST_UID}:${HOST_GID}" /home/user || true

# clone, build and install DML, not necessary if you don't use it
# deactivated because it can't be cached by docker because of the ADD above.
# compile the DML outside then the ADD will include them
# see https://stackoverflow.com/questions/39376786/docker-and-symlinks
#RUN git submodule update --init --recursive
#WORKDIR ./code/thirdParty/DML
#RUN mkdir build
#RUN mkdir dml_install
#WORKDIR ./build
#RUN cmake -DDML_BUILD_EXAMPLES=OFF -DDML_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=../dml_install ..
#RUN cmake --build . --target install

