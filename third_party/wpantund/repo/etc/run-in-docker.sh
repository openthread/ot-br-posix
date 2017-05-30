#!/bin/bash
#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e

path_to_dir ()
{
    f=$@;
    if [ -d "$f" ]; then
        base="";
        dir="$f";
    else
        base="/$(basename "$f")";
        dir=$(dirname "$f");
    fi;
    dir=$(cd "$dir" && /bin/pwd);
    echo "$dir$base"
}

usage ()
{
  echo "usage: "
  echo "$0 [-i] <command to exec inside docker>"
  echo "$0 -c|--clear"
  echo "$0 -a|--clear-all"
  echo -n
  echo "-c, --clear              delete all docker images including the current one"
  echo "--priv                   run container with privileges"
  echo "-p, --prune              delete all docker images except the current one"
  echo "-i, --interactive        run docker with -it"
}

calc_sha1 ()
{
	# Figure out which program to use to calculate
	# the SHA1 hash and execute it.
	if ( which shasum > /dev/null 2>&1 )
	then shasum "$1"
	elif ( which sha1sum > /dev/null 2>&1 )
	then sha1sum "$1"
	elif ( which openssl > /dev/null 2>&1 )
	then openssl dgst -sha1 < "$1"
	fi
}

SCRIPT_DIR="$(path_to_dir "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )")"
PROJECT_DIR="$(pwd)"
SRC_DIR="/src"

# 1. Calculate the SHA1 hash of the Dockerfile
DOCKER_SHA=$(calc_sha1 $SCRIPT_DIR/Dockerfile)
DOCKER_SHORT_SHA=${DOCKER_SHA:0:8}
DOCKER_IMAGE_NAME_BASE="nestlabs/wpantund-build"
DOCKER_IMAGE_NAME="$DOCKER_IMAGE_NAME_BASE:$DOCKER_SHORT_SHA"

prune_unused_dockers ()
{
  docker images | grep $DOCKER_IMAGE_NAME_BASE | awk 'BEGIN{OFS = ":"}{print $1,$2}' | grep -v $DOCKER_IMAGE_NAME | xargs -I {} docker rmi {}
  return 0
}

delete_current_docker ()
{
  docker rmi $DOCKER_IMAGE_NAME
}

delete_all_dockers ()
{
  prune_unused_dockers
  delete_current_docker
  rm -rf "$SCRIPT_DIR/hostinfo"
}

if [[ $# -eq 0 ]]
then
  echo "Give me something to do."
  usage
  exit 0
fi

OTHER_OPTS=""

# Check to see if we have any arguments to work on.
while [[ $# > 0 ]]
do
  key="$1"

  case $key in
    -h|--help)
      usage
     exit 0;
      ;;
    -c|--clear)
      delete_all_dockers
      exit $?
      ;;
    -p|--prune)
      prune_unused_dockers
      exit $?
      ;;
    --priv)
      OTHER_OPTS="${OTHER_OPTS} --privileged"
      ;;
    -i|--it)
      OTHER_OPTS="${OTHER_OPTS} -it"
      ;;
    *)
      if [ ${key:0:1} ==  '-' ]
      then
        echo "Unknown option $key"
        usage
        exit 1
      fi
      break
      ;;
  esac
  shift
done

# 2. Check to see if that docker image exists.
RESULT=$(docker images | awk 'BEGIN{OFS = ":"}{print $1,$2}' |grep -c $DOCKER_IMAGE_NAME) || true

if [ $RESULT -eq 1 ]; then
  echo "Docker image $DOCKER_IMAGE_NAME already exists"
else
  echo "Creating new docker image: $DOCKER_IMAGE_NAME"
  docker build -t $DOCKER_IMAGE_NAME $SCRIPT_DIR
fi

COMMAND="$@"
TMPFILE="$(mktemp ./tmp.XXXXXXXXXX)"
echo "$COMMAND" > "$TMPFILE"
chmod 0755 "$TMPFILE"
trap "rm -f '$TMPFILE'" EXIT
echo "Executing '$COMMAND' as $USERNAME"

CMD="docker run \
  -v=$PROJECT_DIR:$SRC_DIR \
  -w=$SRC_DIR \
  --hostname=`hostname`\
  --rm \
  $OTHER_OPTS \
  $DOCKER_IMAGE_NAME \
  bash --login -ci '$TMPFILE'"

$CMD
