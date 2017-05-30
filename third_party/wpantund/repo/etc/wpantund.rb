# Homebrew recipe for wpantund.

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

require 'formula'

class Wpantund < Formula
  homepage 'https://github.com/openthread/wpantund'
  head 'https://github.com/openthread/wpantund.git', :using => :git, :branch => 'master'
  url 'https://github.com/openthread/wpantund.git', :using => :git, :tag => 'full/0.07.01'
  version '0.07.01'

  devel do
    url 'https://github.com/openthread/wpantund.git', :using => :git, :tag => 'full/latest-unstable'
    version 'latest-unstable'
  end

  depends_on 'pkg-config' => :build
  depends_on 'd-bus'
  depends_on 'boost'

  depends_on 'autoconf' => :build
  depends_on 'automake' => :build
  depends_on 'libtool' => :build
  depends_on 'autoconf-archive' => :build

  def install
    system "[ -x configure ] || PATH=\"#{HOMEBREW_PREFIX}/bin:$PATH\" ./bootstrap.sh"

    system "./configure",
      "--disable-dependency-tracking",
      "--without-connman",
      "--enable-all-restricted-plugins",
      "--prefix=#{prefix}"

    system "make check"
    system "make install"
  end

  def test
    system "wpanctl"
  end
end
