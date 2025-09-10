/*
 *    Copyright (c) 2017, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

(function() {
    angular
        .module('StarterApp', ['ngMaterial', 'ngMessages'])
        .controller('AppCtrl', AppCtrl)
        .service('sharedProperties', function() {
            var index = 0;
            var networkInfo;

            return {
                getIndex: function() {
                    return index;
                },
                setIndex: function(value) {
                    index = value;
                },
                getNetworkInfo: function() {
                    return networkInfo;
                },
                setNetworkInfo: function(value) {
                    networkInfo = value
                },
            };
        });

    function AppCtrl($scope, $http, $mdDialog, $interval, sharedProperties) {
        $scope.menu = [{
                title: 'Home',
                icon: 'home',
                show: true,
            },
            {
                title: 'Join',
                icon: 'add_circle_outline',
                show: false,
            },
            {
                title: 'Form',
                icon: 'open_in_new',
                show: false,
            },
            {
                title: 'Status',
                icon: 'info_outline',
                show: false,
            },
            {
                title: 'Settings',
                icon: 'settings',
                show: false,
            },
            {
                title: 'Commission',
                icon: 'add_circle_outline',
                show: false,
            },
            {
                title: 'Topology',
                icon: 'add_circle_outline',
                show: false,
            },

        ];

        $scope.thread = {
            networkName: 'OpenThreadDemo',
            extPanId: '1111111122222222',
            panId: '0x1234',
            passphrase: 'j01Nme',
            networkKey: '00112233445566778899aabbccddeeff',
            channel: 15,
            prefix: 'fd11:22::',
            defaultRoute: true,
        };

        $scope.setting = {
            prefix: 'fd11:22::',
            defaultRoute: true,
        };

        $scope.headerTitle = 'Home';
        $scope.status = [];

        $scope.isLoading = false;

        $scope.showScanAlert = function(ev) {
            $mdDialog.show(
                $mdDialog.alert()
                .parent(angular.element(document.querySelector('#popupContainer')))
                .clickOutsideToClose(true)
                .title('Information')
                .textContent('There is no available Thread network currently, please \
                             wait a moment and retry it.')
                .ariaLabel('Alert Dialog Demo')
                .ok('Okay')
            );
        };
        $scope.showPanels = async function(index) {
            $scope.headerTitle = $scope.menu[index].title;
            for (var i = 0; i < 7; i++) {
                $scope.menu[i].show = false;
            }
            $scope.menu[index].show = true;
            if (index == 1) {
                $scope.isLoading = true;
                $http.get('available_network').then(function(response) {
                    $scope.isLoading = false;
                    if (response.data.error == 0) {
                        $scope.networksInfo = response.data.result;
                    } else {
                        $scope.showScanAlert(event);
                    }
                });
            }
            if (index == 3) {
                $http.get('get_properties').then(function(response) {
                    console.log(response);
                    if (response.data.error == 0) {
                        var statusJson = response.data.result;
                        $scope.status = [];
                        for (var i = 0; i < Object.keys(statusJson).length; i++) {
                            $scope.status.push({
                                name: Object.keys(statusJson)[i],
                                value: statusJson[Object.keys(statusJson)[i]],
                                icon: 'res/img/icon-info.png',
                            });
                        }
                    }
                });
            }
            if (index == 6) {
                await $scope.showTopology();
                $scope.dataInit();
            }
        };

        $scope.showJoinDialog = function(ev, index, item) {
            sharedProperties.setIndex(index);
            sharedProperties.setNetworkInfo(item);
            $scope.index = index;
            $mdDialog.show({
                controller: DialogController,
                templateUrl: 'join.dialog.html',
                parent: angular.element(document.body),
                targetEvent: ev,
                clickOutsideToClose: true,
                fullscreen: $scope.customFullscreen,
            });
        };

        function DialogController($scope, $mdDialog, $http, $interval, sharedProperties) {
            var index = sharedProperties.getIndex();
            $scope.isDisplay = false;
            $scope.thread = {
                networkKey: '00112233445566778899aabbccddeeff',
                prefix: 'fd11:22::',
                defaultRoute: true,
            };

            $scope.showAlert = function(ev, message) {
                $mdDialog.show(
                    $mdDialog.alert()
                    .parent(angular.element(document.querySelector('#popupContainer')))
                    .clickOutsideToClose(true)
                    .title('Information')
                    .textContent(message)
                    .ariaLabel('Alert Dialog Demo')
                    .ok('Okay')
                    .targetEvent(ev)
                );
            };

            $scope.showQRAlert = function(ev, message) {
                $mdDialog.show(
                    $mdDialog.alert()
                    .parent(angular.element(document.querySelector('#popupContainer')))
                    .clickOutsideToClose(true)
                    .title('Information')
                    .textContent(message)
                    .ariaLabel('Alert Dialog Demo')
                    .ok('Okay')
                    .targetEvent(ev)
                    .multiple(true)
                );
            };

            $scope.showQRCode = function(ev, image) {
              $mdDialog.show({
                targetEvent: ev,
                parent: angular.element(document.querySelector('#popupContainer')),
                  template:
                    '<md-dialog>' +
                    '  <md-dialog-content>' +
                    '  <h6 style="margin: 10px 10px 10px 10px; "><b>Open your OT Commissioner Android App to scan the Connect QR Code</b></h6>' +
                    '  <div layout="row">' +
                    '  <img ng-src="' + image + '" alt="qr code" style="display: block; margin-left: auto; margin-right: auto; width:40%"></img>' +
                    '  </div>' +
                    '  <md-dialog-actions>' +
                    '    <md-button ng-click="closeDialog()" class="md-primary">' +
                    '      Close' +
                    '    </md-button>' +
                    '  </md-dialog-actions>' +
                    '</md-dialog>',
                   controller: function DialogController($scope, $mdDialog) {
                    $scope.closeDialog = function() {
                    $mdDialog.hide();
                    }
                },
                    multiple: true
             });
            };

            $scope.join = function(valid) {
                if (!valid)
                {
                    return;
                }

                if ($scope.thread.defaultRoute == null) {
                    $scope.thread.defaultRoute = false;
                };
                $scope.isDisplay = true;
                var data = {
                    credentialType: $scope.thread.credentialType,
                    networkKey: $scope.thread.networkKey,
                    pskd: $scope.thread.pskd,
                    prefix: $scope.thread.prefix,
                    defaultRoute: $scope.thread.defaultRoute,
                    index: index,
                };
                var httpRequest = $http({
                    method: 'POST',
                    url: 'join_network',
                    data: data,
                });

                httpRequest.then(function successCallback(response) {
                    $scope.res = response.data.result;
                    if (response.data.result == 'successful') {
                        $mdDialog.hide();
                    }
                    $scope.isDisplay = false;
                    $scope.showAlert(event, "Join operation is " + response.data.result + ". " + response.data.message);
                });
            };

            $scope.cancel = function() {
                $mdDialog.cancel();
            };

            $scope.qrcode = function() {
                $scope.isLoading = false;
                $http.get('get_qrcode').then(function(response) {
                    console.log(response);
                    $scope.res = response.data.result;
                    if (response.data.result == 'successful') {
                        var image = "http://api.qrserver.com/v1/create-qr-code/?color=000000&amp;bgcolor=FFFFFF&amp;data=v%3D1%26%26eui%3D" + response.data.eui64 +"%26%26cc%3D" + $scope.thread.pskd +"&amp;qzone=1&amp;margin=0&amp;size=400x400&amp;ecc=L";
                        $scope.showQRCode(event, image);
                    } else {
                        $scope.showQRAlert(event, "sorry, can not generate the QR code.");
                    }   
                    $scope.isDisplay = true;       
                    
                });
            };
        };


        $scope.showConfirm = function(ev, valid) {
            if (!valid)
            {
                return;
            }

            var confirm = $mdDialog.confirm()
                .title('Are you sure you want to Form the Thread Network?')
                .textContent('')
                .targetEvent(ev)
                .ok('Okay')
                .cancel('Cancel');

            $mdDialog.show(confirm).then(function() {
                if ($scope.thread.defaultRoute == null) {
                    $scope.thread.defaultRoute = false;
                };
                var data = {
                    networkKey: $scope.thread.networkKey,
                    prefix: $scope.thread.prefix,
                    defaultRoute: $scope.thread.defaultRoute,
                    extPanId: $scope.thread.extPanId,
                    panId: $scope.thread.panId,
                    passphrase: $scope.thread.passphrase,
                    channel: $scope.thread.channel,
                    networkName: $scope.thread.networkName,
                };
                $scope.isForming = true;
                var httpRequest = $http({
                    method: 'POST',
                    url: 'form_network',
                    data: data,
                });

                httpRequest.then(function successCallback(response) {
                    $scope.res = response.data.result;
                    if (response.data.result == 'successful') {
                        $mdDialog.hide();
                    }
                    $scope.isForming = false;
                    $scope.showAlert(event, 'FORM', response.data.result);
                });
            }, function() {
                $mdDialog.cancel();
            });
        };

        $scope.showAlert = function(ev, operation, result) {
            $mdDialog.show(
                $mdDialog.alert()
                .parent(angular.element(document.querySelector('#popupContainer')))
                .clickOutsideToClose(true)
                .title('Information')
                .textContent(operation + ' operation is ' + result)
                .ariaLabel('Alert Dialog Demo')
                .ok('Okay')
                .targetEvent(ev)
            );
        };

        $scope.showAddConfirm = function(ev) {
            var confirm = $mdDialog.confirm()
                .title('Are you sure you want to Add this On-Mesh Prefix?')
                .textContent('')
                .targetEvent(ev)
                .ok('Okay')
                .cancel('Cancel');

            $mdDialog.show(confirm).then(function() {
                if ($scope.setting.defaultRoute == null) {
                    $scope.setting.defaultRoute = false;
                };
                var data = {
                    prefix: $scope.setting.prefix,
                    defaultRoute: $scope.setting.defaultRoute,
                };
                var httpRequest = $http({
                    method: 'POST',
                    url: 'add_prefix',
                    data: data,
                });

                httpRequest.then(function successCallback(response) {
                    $scope.showAlert(event, 'Add', response.data.result);
                });
            }, function() {
                $mdDialog.cancel();
            });
        };

        $scope.showDeleteConfirm = function(ev) {
            var confirm = $mdDialog.confirm()
                .title('Are you sure you want to Delete this On-Mesh Prefix?')
                .textContent('')
                .targetEvent(ev)
                .ok('Okay')
                .cancel('Cancel');

            $mdDialog.show(confirm).then(function() {
                var data = {
                    prefix: $scope.setting.prefix,
                };
                var httpRequest = $http({
                    method: 'POST',
                    url: 'delete_prefix',
                    data: data,
                });

                httpRequest.then(function successCallback(response) {
                    $scope.showAlert(event, 'Delete', response.data.result);
                });
            }, function() {
                $mdDialog.cancel();
            });
        };

        $scope.startCommission = function(ev) {
            var data = {
                pskd: $scope.commission.pskd,
                passphrase: $scope.commission.passphrase,
            };
            var httpRequest = $http({
                method: 'POST',
                url: 'commission',
                data: data,
            });
            
            ev.target.disabled = true;
            
            httpRequest.then(function successCallback(response) {
                if (response.data.error == 0) {
                    $scope.showAlert(event, 'Commission', 'success');
                } else {
                    $scope.showAlert(event, 'Commission', 'failed');
                }
                ev.target.disabled = false;
            });
        };

        $scope.restServerPort = '8081';
        $scope.ipAddr = window.location.hostname + ':' + $scope.restServerPort;

        // Basic information line
        $scope.basicInfo = {
            'networkName' : 'Unknown',
            'leaderData'  :{'leaderRouterId' : 'Unknown'}
        }
        // Num of router calculated by diagnostic
        $scope.NumOfRouter = 'Unknown';

        // Diagnostic information for detailed display
        $scope.nodeDetailInfo = 'Unknown';
        // For response of Diagnostic
        $scope.networksDiagInfo = '';
        $scope.deviceList = '';
        $scope.graphisReady = false;
        $scope.detailList = {
            'extAddress': { 'title': true, 'content': true },
            'rloc16': { 'title': true, 'content': true },
            'ipv6Addresses': { 'title': false, 'content': false },
            'routerNeighbor': { 'title': true, 'content': false },
            'route': { 'title': true, 'content': false },
            'leaderData': { 'title': false, 'content': false },
            'networkData': { 'title': false, 'content': true },
            'macCounters': { 'title': false, 'content': false },
            'childTable': { 'title': true, 'content': false },
            'channelPages': { 'title': false, 'content': false },
            'mode': { 'title': false, 'content': false },
            'timeout': { 'title': false, 'content': false },
            'connectivity': { 'title': false, 'content': false },
            'batteryLevel': { 'title': false, 'content': false },
            'supplyVoltage': { 'title': false, 'content': false },
            'maxChildTimeout': { 'title': false, 'content': false },
            'lDevIdSubject': { 'title': false, 'content': false },
            'iDevIdCert': { 'title': false, 'content': false },
            'eui64': { 'title': false, 'content': false },
            'version': { 'title': false, 'content': false },
            'vendorName': { 'title': false, 'content': false },
            'vendorModel': { 'title': false, 'content': false },
            'vendorSwVersion': { 'title': false, 'content': false },
            'threadStackVersion': { 'title': false, 'content': false },
            'children': { 'title': false, 'content': false },
            'childIpv6Addresses': { 'title': false, 'content': false },
            'mleCounters': { 'title': false, 'content': false }
        };
        
        $scope.graphInfo = {
            'nodes': [],
            'links': []
        }

        $scope.dataInit = async function(maxRetries = 10) {
            let attempts = 0;
            let response;
        
            while (attempts < maxRetries) {
                try {
                    attempts++;
                    response = await $http.get('http://' + $scope.ipAddr + '/api/node',{
                        headers: {
                            'Accept': 'application/json'
                        }
                    });
                    break; 
                } catch (error) {
                    if (error.status !== 400) {
                        // Log the error
                        console.warn(`Attempt ${attempts} failed with a ${error.status} error. Retrying...`);
                    }
                }
            }
            if (attempts === maxRetries) {
                console.error(`Failed to fetch devices after ${maxRetries} attempts.`);
            }

            $scope.basicInfo = response.data;
            console.log($scope.basicInfo.networkName);
            console.log($scope.basicInfo.leaderData.leaderRouterId);
            $scope.basicInfo.rloc16 = $scope.intToHexString($scope.basicInfo.rloc16, 4);
            $scope.basicInfo.leaderData.leaderRouterId = '0x' + $scope.intToHexString($scope.basicInfo.leaderData.leaderRouterId, 2);
            $scope.$apply();
        }
        $scope.isObject = function(obj) {
            return obj.constructor === Object;
        }
        $scope.isArray = function(arr) {
            return !!arr && arr.constructor === Array;
        }

        $scope.clickList = function(key) {
            $scope.detailList[key]['content'] = !$scope.detailList[key]['content']
        }

        $scope.intToHexString = function(num, len){
            var value;
            value  = num.toString(16);
            
            while( value.length < len ){
                value = '0' + value;
            }
            return value;
        }

        // Body for POST request to /actions endpoint with default TLVs
        $scope.RequestBodyDefault = {
            "data": [
                {
                "type": "getNetworkDiagnosticTask",
                "attributes": {
                    "destination": null,
                    "types": [],
                    "timeout": 60
                    }
                }
            ]
        }

        // Body for POST request to /actions endpoint
        $scope.RequestBody = {}

        // Returns request body for given destination
        $scope.createRequestBody = function(destination) {
            var Body = angular.copy($scope.RequestBody);
            Body.data[0].attributes.destination = destination;
            //console.log(Body);
            return Body;
        }

        // Add TLV to request body
        $scope.updateRequestBody = function() {
            $scope.RequestBody = angular.copy($scope.RequestBodyDefault);

            // Iterate over each key in detailList to check which TLVs should be included in the request body
            Object.keys($scope.detailList).forEach(function(key) {
                if ($scope.detailList[key].title === true) {
                    $scope.RequestBody.data[0].attributes.types.push(key);
                }
            });
        }

        // Sleep for given time
        $scope.sleep = async function(ms) {
            return new Promise(resolve => setTimeout(resolve, ms));
        }

        // GET request to check action status
        $scope.getActionStatus = async function(action_id) {

            const response = await $http.get('http://' + $scope.ipAddr + '/api/actions/' + action_id, {
                headers: {
                    'Accept': 'application/vnd.api+json'
                }
            });

            let status = response.data.data.attributes.status;

            return status;
        }

        // POST request to retrieve list of devices
        $scope.fetchDevices = async function (maxRetries = 3) {
            let attempts = 0;
            let devicesResponse;
        
            while (attempts < maxRetries) {
                try {
                    attempts++;
                    devicesResponse = await $http.post('http://' + $scope.ipAddr + '/api/devices', {}, {
                        headers: {
                            'Accept': 'application/json'
                        }
                    });
                    break; 
                } catch (error) {
                    if (error.status !== 400) {
                        // Log the error
                        console.warn(`Attempt ${attempts} failed with a ${error.status} error. Retrying...`);
                    }
                }
            }
        
            if (attempts === maxRetries) {
                console.error(`Failed to fetch devices after ${maxRetries} attempts.`);
            }
        
            return devicesResponse; // Return the successful response
        }

        // POST request to cause action that fetches device diagnostic, then poll action status until completed
        $scope.fetchDeviceDiagnostic = async function(id) {
            let stopped = false;

            do {
                // Fetch device diagnostics
                const postResponse = await $http.post('http://' + $scope.ipAddr + '/api/actions', $scope.createRequestBody(id), {
                    headers: {
                        'Content-Type': 'application/vnd.api+json',
                        'Accept': 'application/json'
                    }
                });

                let action_id = postResponse.data.data[0].id; 
                console.log("action_id:", action_id);

                // Polling of action status
                let time = 0.0;
                stopped = false;
                while (true) {
                    const status = await $scope.getActionStatus(action_id);
                    if (status === "completed") {
                        console.log("Completed action in %fs (id: %s)", time, id);
                        break; // Exit the loop if the action is completed
                    }
                    if (status === "stopped") {
                        console.log("Oops... Action stopped (id: %s)", id);
                        stopped = false; //true
                        break; // Break out to retry the POST request
                    }
                    await $scope.sleep(500);
                    time += 0.5;
                }
            } while (stopped);  
        }

        $scope.fetchDiagnosticsForDevices = async function(devices) {
            // Create an array of promises for each device diagnostic fetch
            let promises = devices.map(async (device) => {
                let id = device.extAddress; // Assume each device has a unique 'id'
                if (device.role !== "child") { // Ignore child diagnostics
                    await $scope.fetchDeviceDiagnostic(id); // Fetch diagnostic for each device
                }
            });
        
            // Wait for all promises to resolve (all diagnostics to be fetched)
            await Promise.all(promises);
        };

        // Deduplicate node entries based on "extAddress" and only retain newest entries
        $scope.dedupNetworksDiagEntries = function() {
            const networksDiagInfoDedup = Object.values($scope.networksDiagInfo.reduce((acc, entry) => {
                const extAddress = entry.extAddress;
                const createdDate = entry.created;
            
                // If no entry exists for this extAddress, or the current entry is newer, update the accumulator
                if (!acc[extAddress] || new Date(createdDate) > new Date(acc[extAddress].created)) {
                    acc[extAddress] = entry;
                }
            
                return acc;
            }, {}));

            return networksDiagInfoDedup;
        }

        // Remove children node entries based on "rloc16"
        $scope.removeChildren = function() {
            
            const filteredEntries = $scope.networksDiagInfo.filter(entry => {
                return entry.rloc16.endsWith('00'); // Keep entries where rloc16 ends with '00'
            });
        
            return filteredEntries;
        };

        // Behaviour upon pressing show Topology button
        $scope.showTopology = async function() {
            console.log("show topology...");  // Debugging log message

            $scope.graphisReady = false;
            $scope.graphInfo = {
                'nodes': [],
                'links': []
            };
            
            // Consider TLV checkboxes
            $scope.updateRequestBody();

            try {
                // Fetch the list of devices
                devicesResponse = await $scope.fetchDevices();
        
                const devices = devicesResponse.data;
                console.log("devices:", devices);
                
                // Delete diagnostics entries
                const deleteResponse = await $http.delete('http://' + $scope.ipAddr + '/api/diagnostics');
                console.log("deleted diagnostics");

                // Fetch the list of devices
                const getResponse1 = await $http.get('http://' + $scope.ipAddr + '/api/diagnostics', {
                    headers: {
                        'Accept': 'application/json'
                    }
                });
                $scope.networksDiagInfo = getResponse1.data;

                // SANITY CHECK: Make sure response is empty after deletion
                console.log('networksDiagInfo:', $scope.networksDiagInfo);

                // Loop over each device and fetch diagnostics
                await $scope.fetchDiagnosticsForDevices(devices);

                // Fetch the list of device diagnostics
                const getResponse = await $http.get('http://' + $scope.ipAddr + '/api/diagnostics', {
                    headers: {
                        'Accept': 'application/json'
                    }
                });
                $scope.networksDiagInfo = getResponse.data;

                // Once all diagnostics have been fetched
                console.log('networksDiagInfo:', $scope.networksDiagInfo);

                // Remove duplicate entries
                $scope.networksDiagInfo = $scope.dedupNetworksDiagEntries();
                $scope.networksDiagInfo = $scope.removeChildren();
                console.log('networksDiagInfo after dedup:', $scope.networksDiagInfo);
        
                // Build Topology based on networksDiagInfo and render Graph
                $scope.buildTopology();
                $scope.drawGraph();

                console.log("Graph rendered");
        
            } catch (error) {
                console.error('Error fetching device diagnostics:', error);
            }
        };

        // Build Topology by populating $scope.graphInfo based on $scope.networksDiagInfo
        $scope.buildTopology = function() {
            var nodeMap = {};
            var count, src, dist, rloc, child, rlocOfParent, rlocOfChild, diagOfNode, linkNode, childInfo;

            // Ensure consistent routerID and rloc16 across all nodes
            for (diagOfNode of $scope.networksDiagInfo){
                diagOfNode['routerId'] = '0x' + $scope.intToHexString(diagOfNode['routerId'],2);
                if ('leaderData' in diagOfNode) {
                    diagOfNode['leaderData']['leaderRouterId'] = '0x' + $scope.intToHexString(diagOfNode['leaderData']['leaderRouterId'],2);
                }

                if ('route' in diagOfNode) { // Router is connected to network
                    for (linkNode of diagOfNode['route']['routeData']){
                        linkNode['routeId'] = '0x' + $scope.intToHexString(linkNode['routeId'], 2);
                    }
                }
            }
            
            // Count nodes
            count = 0;
            // Populate nodes list
            for (diagOfNode of $scope.networksDiagInfo) {
                if ('childTable' in diagOfNode) {

                    rloc = diagOfNode['rloc16'];
                    nodeMap[rloc] = count;
                    
                    if (diagOfNode['isLeader'] == true) {
                        diagOfNode['role'] = 'Leader';
                    } else {
                        diagOfNode['role'] = 'Router';
                    }

                    $scope.graphInfo.nodes.push(diagOfNode); 
                    
                    if (diagOfNode['rloc16'] === $scope.basicInfo.rloc16) {
                        $scope.nodeDetailInfo = diagOfNode
                    }
                    count = count + 1;
                }
            }
            // Num of Router is based on the diagnostic information
            $scope.NumOfRouter = count;
            
            // Index for a second loop
            src = 0;
            // Construct links 
            for (diagOfNode of $scope.networksDiagInfo) {
                if ('childTable' in diagOfNode) {
                    // Link between routers
                    for (linkNode of diagOfNode['route']['routeData']) {
                        rloc = (parseInt(linkNode['routeId'], 16) << 10).toString(16).padStart(4, '0');
                        if (rloc in nodeMap) {
                            dist = nodeMap[rloc];
                            if (src < dist) {
                                $scope.graphInfo.links.push({
                                    'source': src,
                                    'target': dist,
                                    'weight': 1,
                                    'type': 0,
                                    'linkInfo': {
                                        'inQuality': linkNode['linkQualityIn'],
                                        'outQuality': linkNode['linkQualityOut']
                                    }
                                });
                            }
                        }
                    }

                    // Link between router and child 
                    for (childInfo of diagOfNode['childTable']) {
                        child = {};
                        rlocOfParent = diagOfNode['rloc16']
                        rlocOfChild = (parseInt(diagOfNode['rloc16'], 16) + childInfo['childId']).toString(16);
                        rlocOfChild = rlocOfChild.padStart(4, '0'); // Padding to ensure 4-character hex string

                        src = nodeMap[rlocOfParent];
                        
                        child['rloc16'] = rlocOfChild;
                        child['routerId'] = diagOfNode['routerId'];
                        nodeMap[rlocOfChild] = count;
                        child['role'] = 'Child';
                        $scope.graphInfo.nodes.push(child);
                        $scope.graphInfo.links.push({
                            'source': src,
                            'target': count,
                            'weight': 1,
                            'type': 1,
                            'linkInfo': {
                                'Timeout': childInfo['timeout'],
                                'Mode': childInfo['mode']
                            }

                        });

                        count = count + 1;
                    }
                }
                src = src + 1;
            }

            console.log("graphInfo:", $scope.graphInfo);

        }
        

        $scope.updateDetailLabel = function() {
            for (var detailInfoKey in $scope.detailList) {
                $scope.detailList[detailInfoKey]['title'] = false;
            }
            for (var diagInfoKey in $scope.nodeDetailInfo) {
                if (diagInfoKey in $scope.detailList) {
                    $scope.detailList[diagInfoKey]['title'] = true;
                }
            }
        }

        
        // Initialize the slider model
        $scope.thresholdFrameErrorRate = 20;

        // Draw SVG
        $scope.drawGraph = function() {
            var json, svg, tooltip, force;
            var scale, len;
            
            console.log("D3: updating SVG");

            document.getElementById('topograph').innerHTML = '';
            scale = $scope.graphInfo.nodes.length;
            len = 50 * Math.sqrt(scale) + 200;

            // Create the zoom behavior
            var zoom = d3.behavior.zoom()
                .scaleExtent([0.5, 3]) 
                .on('zoom', zoomed);

            function zoomed() {
                svg.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
            }
        
            // Legend
            svgLegend = d3.select('.d3graph').append('svg')
                .attr('preserveAspectRatio', 'xMidYMid meet')
                .attr('viewBox', '0, 0, ' + len.toString(10) + ',' + (len/7).toString(10))

                    
            const scale_legend = len/250;

            svgLegend.append('circle')
                .attr('cx',len-20 * scale_legend)
                .attr('cy',10 * scale_legend).attr('r', 3 * scale_legend)
                .style('fill', "#7e77f8")
                .style('stroke', '#484e46')
                .style('stroke-width', '0.4px');
            
            svgLegend.append('circle')
                .attr("cx",len-20 * scale_legend)
                .attr('cy',20 * scale_legend)
                .attr('r', 3 * scale_legend)
                .style('fill', '#03e2dd')
                .style('stroke', '#484e46')
                .style('stroke-width', '0.4px');
            
            svgLegend.append('circle')
                .attr('cx',len-20 * scale_legend)
                .attr('cy',30 * scale_legend)
                .attr('r', 3 * scale_legend)
                .style('fill', '#aad4b0')
                .style('stroke', '#484e46')
                .style('stroke-width', '0.4px')
                .style('stroke-dasharray','2 1');
        
            svgLegend.append('circle')
                .attr('cx',len-50 * scale_legend)
                .attr('cy',10 * scale_legend).attr('r', 3 * scale_legend)
                .style('fill', '#ffffff')
                .style('stroke', '#f39191')
                .style('stroke-width', '0.4px');
            
            svgLegend.append('text')
                .attr('x', len-15 * scale_legend)
                .attr('y', 10 * scale_legend)
                .text('Leader')
                .style('font-size', (4 * scale_legend).toString(10) + 'px')
                .attr('alignment-baseline','middle');
            
            svgLegend.append('text')
                .attr('x', len-15 * scale_legend)
                .attr('y',20 * scale_legend )
                .text('Router')
                .style('font-size', (4 * scale_legend).toString(10) + 'px')
                .attr('alignment-baseline','middle');
            
            svgLegend.append('text')
                .attr('x', len-15 * scale_legend)
                .attr('y',30 * scale_legend )
                .text('Child')
                .style('font-size', (4 * scale_legend).toString(10) + 'px')
                .attr('alignment-baseline','middle');
            
            svgLegend.append('text')
                .attr('x', len-45 * scale_legend)
                .attr('y',10 * scale_legend)
                .text('Selected')
                .style('font-size', (4 * scale_legend).toString(10) + 'px')
                .attr('alignment-baseline','middle');


            // Topology graph
            svg = d3.select('.d3graph').append('svg')
                .attr('preserveAspectRatio', 'xMidYMid meet')
                .attr('viewBox', '0, 0, ' + len.toString(10) + ', ' + len.toString(10))
                .call(zoom) // Attach the zoom behavior
                .append('g')  // Group for zoomable content

            // Tooltip style  for each node
            tooltip = d3.select('body')
                .append('div')
                .attr('class', 'tooltip')
                .style('position', 'absolute')
                .style('z-index', '10')
                .style('visibility', 'hidden')
                .text('a simple tooltip');

            force = d3.layout.force()
                .distance(function(link) {
                    return link.type ? 50 : (link.linkInfo.inQuality ? 100 / link.linkInfo.inQuality + 50 : 250);
                })
                .charge(-50)
                .linkStrength(0.5)
                .size([len, len]);

            json = $scope.graphInfo;
           
            force
                .nodes(json.nodes)
                .links(json.links)
                .start();
            
            // Define color scale for errorRate
            var colorScale = d3.scale.linear()
            .domain([0, 0.5, 0.75, 1]) // Define points for each color stop
            .range(['green', 'yellow', 'orange', 'red']); // Green for low error rate, red for high        

            var link = svg.selectAll('.link')
                .data(json.links)
                .enter().append('line')
                .attr('class', 'link')
                // Color-grade based on errorRate
                .style('stroke', function(item) {
                    const target = item.source.routerNeighbor.find(neighbor => neighbor.rloc16 === item.target.rloc16);
                    //console.log(target);
                    if (target && target["frameErrorRate"] >= $scope.thresholdFrameErrorRate/100) {
                        return colorScale(target.frameErrorRate);
                    } else {
                        return '#908484'; // Default color
                    }
                })
                // Dash line for link between child and parent
                .style('stroke-dasharray', function(item) {
                    if ('Timeout' in item.linkInfo) return '4 4';
                    else return '0 0'
                })
                // Line width representing link quality
                .style('stroke-width', function(item) {
                    if ('inQuality' in item.linkInfo)
                        return Math.sqrt(item.linkInfo.inQuality);
                    else return 1;
                })
                // Effect of mouseover on a link
                .on('mouseover', function(item) {
                    return tooltip.style('visibility', 'visible')
                        .text(JSON.stringify(item.linkInfo));
                })
                .on('mousemove', function() {
                    return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                        .style('left', (d3.event.pageX + 10) + 'px');
                })
                .on('mouseout', function() {
                    return tooltip.style('visibility', 'hidden');
                });


            var node = svg.selectAll('.node')
                .data(json.nodes)
                .enter().append('g')
                .attr('class', function(item) {
                        return item.role;
                })
                .call(force.drag)
                // Tooltip effect of mouseover on a node 
                .on('mouseover', function(item) {
                    return tooltip.style('visibility', 'visible')
                                  .text(item.rloc16);
                })
                .on('mousemove', function() {
                    return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                                  .style('left', (d3.event.pageX + 10) + 'px');
                })
                .on('mouseout', function() {
                    return tooltip.style('visibility', 'hidden');
                });

            d3.selectAll('.Child')
                .append('circle')
                .attr('r', '6')
                .attr('fill', '#aad4b0')
                .style('stroke', '#484e46')
                .style('stroke-dasharray','2 1')
                .style('stroke-width', '0.5px')
                .attr('class', function(item) {
                    return item.rloc16;
                })
                .on('mouseover', function(item) {
                    return tooltip.style('visibility', 'visible')
                                  .text(item.rloc16);
                })
                .on('mousemove', function() {
                    return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                                  .style('left', (d3.event.pageX + 10) + 'px');
                })
                .on('mouseout', function() {
                    return tooltip.style('visibility', 'hidden');
                });


            d3.selectAll('.Leader')
                .append('circle')
                .attr('r', '8')
                .attr('fill', '#7e77f8')
                .style('stroke', '#484e46')
                .style('stroke-width', '1px')
                .attr('class', function(item) {
                    return 'Stroke';
                })
                // Effect that node will become bigger when mouseover
                .on('mouseover', function(item) {
                    d3.select(this)
                        .transition()
                        .attr('r','9');
                    return tooltip.style('visibility', 'visible')
                                  .text(item.rloc16);
                })
                .on('mousemove', function() {
                    return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                                  .style('left', (d3.event.pageX + 10) + 'px');
                })
                .on('mouseout', function() {
                    d3.select(this).transition().attr('r','8');
                        return tooltip.style('visibility', 'hidden');
                })
                // Effect that node will have a yellow edge when clicked
                .on('click', function(item) {
                    d3.selectAll('.Stroke')
                        .style('stroke', '#484e46')
                        .style('stroke-width', '1px');
                    d3.select(this)
                        .style('stroke', '#f39191')
                        .style('stroke-width', '1px');
                    $scope.$apply(function() {
                        $scope.nodeDetailInfo = item;
                    });
                });
            d3.selectAll('.Router')
                .append('circle')
                .attr('r', '8')
                .style('stroke', '#484e46')
                .style('stroke-width', '1px')
                .attr('fill', '#03e2dd')
                .attr('class','Stroke')
                .on('mouseover', function(item) {
                    d3.select(this)
                        .transition()
                        .attr('r','8');
                    return tooltip.style('visibility', 'visible')
                                  .text(item.rloc16);
                })
                .on('mousemove', function() {
                    return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                                  .style('left', (d3.event.pageX + 10) + 'px');
                })
                .on('mouseout', function() {
                    d3.select(this)
                        .transition()
                        .attr('r','7');
                    return tooltip.style('visibility', 'hidden');
                })
                // The same effect as Leader
                .on('click', function(item) {
                    d3.selectAll('.Stroke')
                        .style('stroke', '#484e46')
                        .style('stroke-width', '1px');
                    d3.select(this)
                        .style('stroke', '#f39191')
                        .style('stroke-width', '1px');
                    $scope.$apply(function() {
                        $scope.nodeDetailInfo = item;
                    });
                });

            force.on('tick', function() {
                link.attr('x1', function(item) { return item.source.x; })
                    .attr('y1', function(item) { return item.source.y; })
                    .attr('x2', function(item) { return item.target.x; })
                    .attr('y2', function(item) { return item.target.y; });
                node.attr('transform', function(item) {
                    return 'translate(' + item.x + ',' + item.y + ')';
                });
            });
            
            $scope.graphisReady = true;
            //$scope.$apply(); // Force reload of SVG element

        }
    };
})();
