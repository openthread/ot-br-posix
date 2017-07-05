#!/bin/sh
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

PREV_PATH="`pwd`"

die() {
	echo " *** ERROR: " $*
	exit 1
}

set -x

prep_ssh_key() {
	openssl aes-256-cbc -K "${encrypted_8f78c13496e8_key}" -iv "${encrypted_8f78c13496e8_iv}" -in .travis/deploy.prv.enc -out .travis/deploy.prv -d &&
	chmod 600 .travis/deploy.prv &&
	ssh-add .travis/deploy.prv &&
	git config --global user.name "Travis CI" &&
	git config --global user.email "noreply@travis-ci.com"
}

AUTOCONF_BRANCH="autoconf/$TRAVIS_BRANCH"


if [ $TRAVIS_REPO_SLUG = "openthread/wpantund" ] \
&& [ $TRAVIS_BRANCH = "master" ]                 \
&& [ $TRAVIS_OS_NAME = "linux" ]                 \
&& [ $TRAVIS_PULL_REQUEST = "false" ]            \
&& [ $BUILD_MAKEPATH = "build" ]                 \
&& [ $BUILD_MAKEARGS = "distcheck" ]             \
&& prep_ssh_key                                  #
then
	git fetch --unshallow origin || git fetch origin || die

	git fetch origin scripts:scripts ${AUTOCONF_BRANCH}:${AUTOCONF_BRANCH} ${TRAVIS_BRANCH}:${TRAVIS_BRANCH} || die

	PREVREV="`git rev-parse ${AUTOCONF_BRANCH}`"

	echo "Checking for update to '${AUTOCONF_BRANCH}'..."

	git checkout scripts || die

	NO_PUSH=1 ./update-autoconf-master ${TRAVIS_BRANCH} || die

	CHANGED_LINES=`git diff "${PREVREV}".."${AUTOCONF_BRANCH}" | grep '^[-+]' | grep -v '^[-+]SOURCE_VERSION=' | wc -l`

	if [ "$CHANGED_LINES" -gt "2" ]
	then
		echo "Branch '${AUTOCONF_BRANCH}' is OUT OF DATE."

		git checkout "${TRAVIS_BRANCH}" || die

		git push git@github.com:${TRAVIS_REPO_SLUG}.git ${AUTOCONF_BRANCH}:${AUTOCONF_BRANCH} || die
	else
		echo "Branch '${AUTOCONF_BRANCH}' is still up-to-date."
	fi
else
	echo "Skipping update of '${AUTOCONF_BRANCH}'."
fi

exit 0
