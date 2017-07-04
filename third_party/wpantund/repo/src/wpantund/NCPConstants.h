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
 */

#ifndef wpantund_NCPConstants_h
#define wpantund_NCPConstants_h

#define NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT    5  // seconds
#define NCP_DEFAULT_COMMAND_SEND_TIMEOUT        5  // seconds
#define NCP_TICKLE_TIMEOUT                      60 // seconds
#define NCP_DEEP_SLEEP_TICKLE_TIMEOUT           (60*70) // Seventy minutes
#define NCP_FORM_TIMEOUT                        60 // seconds
#define NCP_JOIN_TIMEOUT                        30 // seconds

#define NCP_NETWORK_KEY_SIZE                    16

#define BUSY_DEBOUNCE_TIME_IN_MS                200
#define MAX_INSOMNIA_TIME_IN_MS                 (MSEC_PER_SEC * 60 * 3)

#define NCP_DEBUG_LINE_LENGTH_MAX               400

#endif
