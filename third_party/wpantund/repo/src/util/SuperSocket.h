/*
 *
 * Copyright (c) 2016 Nest Labs, Inc.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *    Description:
 *      This file declares the SuperSocket class, which provides an
 *      easy way to create various types of sockets using a convenient
 *      path syntax.
 *
 */

#ifndef __wpantund__SuperSocket__
#define __wpantund__SuperSocket__

#include "UnixSocket.h"

namespace nl {
class SuperSocket : public UnixSocket {
protected:
	SuperSocket(const std::string& path);

public:
	virtual ~SuperSocket();

	static boost::shared_ptr<SocketWrapper> create(const std::string& path);

	virtual void reset();

	virtual int hibernate(void);

protected:
	std::string mPath;
}; // class SuperSocket

}; // namespace nl

#endif /* defined(__wpantund__SuperSocket__) */
