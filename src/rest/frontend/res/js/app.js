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

        $scope.RoleInt2String = 
        {
            0 : 'Disabled', 
            1 : 'Detached',
            2 : 'Child' ,
            3 : 'Router',
            4 : 'Leader'   

        }

        
        $scope.thread = {
            networkName: 'OpenThreadDemo',
            extPanId: '1111111122222222',
            panId: '0x1234',
            passphrase: '123456',
            masterKey: '00112233445566778899aabbccddeeff',
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

        $scope.showScanAlert = function(ev,response) {
            var alertMessage;
            if (response.status == 200)
            {
                alertMessage = 'No available thread network now'
            }
            else 
            {   
                if ('ErrorDescription' in response.data)
                {
                    alertMessage = response.data.ErrorDescription;
                }
                else if ('ErorMessage' in response.data)
                {
                    alertMessage= response.data.ErorMessage;
                }
                else
                {
                    alertMessage = 'Undefined response';
                }
            }
           
            $mdDialog.show(
                $mdDialog.alert()
                .parent(angular.element(document.querySelector('#popupContainer')))
                .clickOutsideToClose(true)
                .title('Information')
                .textContent(alertMessage)
                .ariaLabel('Alert Dialog Demo')
                .ok('Okay')
            );
        };
        $scope.showStatusAlert = function(ev,response) {
            var alertMessage;
            if ('ErrorDescription' in response.data)
            {
                alertMessage = response.data.ErrorDescription;
            }
            else if ('ErrorMessage' in response.data)
            {
                alertMessage=  response.data.ErrorMessage ;
            }
            else
            {
                alertMessage = 'Undefined response';
            }
            $mdDialog.show(
                $mdDialog.alert()
                .parent(angular.element(document.querySelector('#popupContainer')))
                .clickOutsideToClose(true)
                .title('Information')
                .textContent(alertMessage)
                .ariaLabel('Alert Dialog Demo')
                .ok('Okay')
            );
        };

        $scope.showPanels = function(index) {
            $scope.headerTitle = $scope.menu[index].title;
            for (var i = 0; i < 7; i++) {
                $scope.menu[i].show = false;
            }
            $scope.menu[index].show = true;
            if (index == 1) {
                $scope.isLoading = true;
                $http.get('/v1/networks').then(
                    function successCallback(response) 
                    {
                        $scope.isLoading = false;
                        if (response.status == 200  && response.data.length > 0)
                        {
                            $scope.networksInfo = response.data;
                        }
                        else
                        {    
                            $scope.showScanAlert(event,response);
                        }
                    },
                    function errorCallback(response)
                    {
                        $scope.isLoading = false;
                        $scope.showScanAlert(event,response);
                    
                    }
                );
            }

            if (index == 3) {
                $http.get('/v1/node').then(
                    function successCallback(response) 
                    {
                        if (response.status == 200){
                            var NodeInfo = response.data;
                            $scope.status = [];
                            for (var i = 0; i < Object.keys(NodeInfo).length; i++) {
                                if (Object.keys(NodeInfo)[i] == 'Network:NodeType' || Object.keys(NodeInfo)[i]  == 'NCP:State' )
                                {
                                    NodeInfo[Object.keys(NodeInfo)[i]] = $scope.RoleInt2String[NodeInfo[Object.keys(NodeInfo)[i]]];
                                }
                                $scope.status.push
                                ({
                                    name: Object.keys(NodeInfo)[i],
                                    value: NodeInfo[Object.keys(NodeInfo)[i]],
                                    icon: 'res/img/icon-info.png',
                                });
                            }
                        
                        }
                        else {
                            $scope.showStatusAlert(event,response);
                        }
                    },
                    function errorCallback(response){
                        
                        $scope.showStatusAlert(event,response);
                    
                    }

                );
            }
            if (index == 6) {
                $scope.dataInit();
                $scope.showTopology(event);
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
            var networkInfo = sharedProperties.getNetworkInfo();
            $scope.isDisplay = false;
            $scope.thread = {
                masterKey: '00112233445566778899aabbccddeeff',
                prefix: 'fd11:22::',
                defaultRoute: true,
            };

            $scope.showAlert = function(ev, response) {

                var alertMessage;
                if (response.status == 200)
                {
                    alertMessage = 'Successful'
                }
                else 
                {   
                    if ('ErrorDescription' in response.data)
                    {
                        alertMessage = response.data.ErrorDescription;
                    }
                    else if ('ErorMessage' in response.data)
                    {
                        alertMessage= response.data.ErorMessage;
                    }
                    else
                    {
                        alertMessage = 'Failed bacause of Undefined response';
                    }
                }
                
                $mdDialog.show(
                    $mdDialog.alert()
                    .parent(angular.element(document.querySelector('#popupContainer')))
                    .clickOutsideToClose(true)
                    .title('Information')
                    .textContent('Join operation is ' + alertMessage)
                    .ariaLabel('Alert Dialog Demo')
                    .ok('Okay')
                    .targetEvent(ev)
                );
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
                    masterKey: $scope.thread.masterKey,
                    prefix: $scope.thread.prefix,
                    defaultRoute: $scope.thread.defaultRoute,
                    index: index,
                    extPanId : networkInfo.ExtPanId,
                    networkName : networkInfo.NetworkName,
                    channel: networkInfo.Channel,
                    panId : '0x' + networkInfo.PanId.toString(16)
                };
                var httpRequest = $http({
                    method: 'PUT',
                    url: '/v1/networks/current',
                    data: data,
                });
                httpRequest.then(
                    function successCallback(response) {
                    
                        if (response.status == 200) {
                            $mdDialog.hide();
                        }
                        $scope.isDisplay = false;
                        $scope.showAlert(event, response);
                    },
                    function errorCallback(response){
                        $scope.isDisplay = false;
                        $scope.showAlert(event, response);
                    }
                );
            };

            $scope.cancel = function() {
                $mdDialog.cancel();
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
                    masterKey: $scope.thread.masterKey,
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
                    url: '/v1/networks',
                    data: data,
                });

                httpRequest.then(
                    function successCallback(response) {
                        $scope.res = response.data.result;
                        if (response.status == 200) {
                            $mdDialog.hide();
                        }
                        $scope.isForming = false;
                        $scope.showAlert(event, 'FORM', response);
                    },
                    function errorCallback(response){
                        $scope.isForming = false;
                        $scope.showAlert(event, 'FORM', response);
                    }


                );
            }, function() {
                $mdDialog.cancel();
            });
        };

        $scope.showAlert = function(ev, operation, response) {
            var alertMessage;
            if (response.status == 200)
            {
                alertMessage = 'Successful';
            }
            else 
            {
                if ('ErrorDescription' in response.data)
                {
                    alertMessage = response.data.ErrorDescription;
                }
                else if ('ErorMessage' in response.data)
                {
                    alertMessage= response.data.ErorMessage;
                }
                else
                {
                    alertMessage = 'Failed bacause of Undefined response';
                }
            }

            $mdDialog.show(
                $mdDialog.alert()
                .parent(angular.element(document.querySelector('#popupContainer')))
                .clickOutsideToClose(true)
                .title('Information')
                .textContent(operation + ' operation is ' + alertMessage)
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
                    url: '/v1/networks/current/prefix',
                    data: data,
                });

                httpRequest.then(function successCallback(response) {
                   
                    $scope.showAlert(event, 'Add', response);
                }
                , function errorCallback(response){
                    
                    $scope.showAlert(event, 'Add', response);
                    
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
                    method: 'DELETE',
                    url: '/v1/networks/current/prefix',
                    data: data,
                });

                httpRequest.then(function successCallback(response) {
                    $scope.showAlert(event, 'Delete', response);
                }, function errorCallback(response){
                    $scope.showAlert(event, 'Delete', response);
                    
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
                url: '/v1/networks/current/commission',
                data: data,
            });
            
            ev.target.disabled = true;
            
            httpRequest.then(function successCallback(response) {
                $scope.showAlert(event, 'Commission', response);
                ev.target.disabled = false;
            
            }, function errorCallback(response){
                $scope.showAlert(event, 'Commission', response);
                
            });
        };

        // Basic information line
        $scope.basicInfo = {
            'NetworkName' : 'Unknown',
            'LeaderData'  :{'LeaderRouterId' : 'Unknown'}
        }
        // Num of router calculated by diagnostic
        $scope.NumOfRouter = 'Unknown';

        // Diagnostic information for detailed display
        $scope.nodeDetailInfo = 'Unknown';
        // For response of Diagnostic
        $scope.networksDiagInfo = '';
        $scope.topologyIsReady = false;
        $scope.topologyIsLoading = false;
        
        $scope.detailList = {
            'ExtAddress': { 'title': false, 'content': true },
            'Rloc16': { 'title': false, 'content': true },
            'Mode': { 'title': false, 'content': false },
            'Connectivity': { 'title': false, 'content': false },
            'Route': { 'title': false, 'content': false },
            'LeaderData': { 'title': false, 'content': false },
            'NetworkData': { 'title': false, 'content': true },
            'IP6Address List': { 'title': false, 'content': true },
            'MACCounters': { 'title': false, 'content': false },
            'ChildTable': { 'title': false, 'content': false },
            'ChannelPages': { 'title': false, 'content': false }
        };
        $scope.graphInfo = {
            'nodes': [],
            'links': []
        }

        $scope.dataInit = function(ev) {
            $http.get('/v1/node/network-name').then(function(response)
            {
                $scope.basicInfo.NetworkName = response.data;
            });
            $http.get('/v1/node/rloc16').then(function(response)
            {
                $scope.basicInfo.Rloc16 = response.data;
                $scope.basicInfo.Rloc16 = $scope.intToHexString($scope.basicInfo.Rloc16,4);
            });
            $http.get('/v1/node/leader-data').then(function(response)
            {
                $scope.basicInfo.LeaderData = response.data;
                $scope.basicInfo.LeaderData.LeaderRouterId = '0x' + $scope.intToHexString($scope.basicInfo.LeaderData.LeaderRouterId<<10,4);
            });
            
           
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

        $scope.showTopologyAlert = function(ev,response) {
            var alertMessage;
            if (response.status == 200)
            {
                alertMessage = 'Empty diagnostic information returned, please check and try again later'
            }
            else 
            {   
                if ('ErrorDescription' in response.data)
                {
                    alertMessage = response.data.ErrorDescription;
                }
                else if ('ErorMessage' in response.data)
                {
                    alertMessage= response.data.ErorMessage;
                }
                else
                {
                    alertMessage = 'Undefined response';
                }
            }
           
            $mdDialog.show(
                $mdDialog.alert()
                .parent(angular.element(document.querySelector('#popupContainer')))
                .clickOutsideToClose(true)
                .title('Information')
                .textContent(alertMessage)
                .ariaLabel('Alert Dialog Demo')
                .ok('Okay')
            );
        };

        $scope.showTopology = function(event) {

            // Is loading
            $scope.topologyIsLoading = true;
            $scope.topologyIsReady = false;
            $scope.graphInfo = {
                'nodes': [],
                'links': []
            };
            $http.get('/v1/diagnostics').then(function successCallback(response) {
                if (response.status == 200 && response.data.length > 0)
                {
                    $scope.topology(response);
                    // Loads successfully
                    $scope.topologyIsLoading = false;
                    $scope.topologyIsReady = true;
                }
                else{
                    // Not show anything
                    $scope.topologyIsReady = false;
                    $scope.topologyIsLoading = false;
                    $scope.showTopologyAlert(event,response);
                }
            },
            function errorCallback(response){
                $scope.showTopologyAlert(event,response);
                // Not show anything
                $scope.topologyIsReady = false;
                $scope.topologyIsLoading = false;
            }

            
            )
        }

        $scope.topology = function(response){
            var nodeMap = {};
            var count, src, dist, rloc, child, rlocOfParent, rlocOfChild, diagOfNode, linkNode, childInfo;
            $scope.networksDiagInfo = response.data;
                
            for (diagOfNode of $scope.networksDiagInfo){
                    
                diagOfNode['RouteId'] = '0x' + $scope.intToHexString(diagOfNode['Rloc16'] >> 10,2);
                    
                diagOfNode['Rloc16'] = '0x' + $scope.intToHexString(diagOfNode['Rloc16'],4);
                    
                diagOfNode['LeaderData']['LeaderRouterId'] = '0x' +  $scope.intToHexString(diagOfNode['LeaderData']['LeaderRouterId'],2);
 
                for (linkNode of diagOfNode['Route']['RouteData']){
                    linkNode['RouteId'] = '0x' + $scope.intToHexString(linkNode['RouteId'],2);
                }
                    
            }
                
            count = 0;
                
            for (diagOfNode of $scope.networksDiagInfo) {
                if ('ChildTable' in diagOfNode) {
                        
                    rloc = parseInt(diagOfNode['Rloc16'],16).toString(16);
                    nodeMap[rloc] = count;
                        
                    if (diagOfNode['RouteId'] == diagOfNode['LeaderData']['LeaderRouterId']) {
                        diagOfNode['Role'] = 'Leader';
                    }
                    else {
                        diagOfNode['Role'] = 'Router';
                    }
 
                    $scope.graphInfo.nodes.push(diagOfNode);
                        
                    if (diagOfNode['Rloc16'] === $scope.basicInfo.rloc16) {
                        $scope.nodeDetailInfo = diagOfNode
                    }
                    count = count + 1;
                }
            }
 
            $scope.NumOfRouter = count;
                
            // Index for a second loop
            src = 0;
 
            // Construct links
            for (diagOfNode of $scope.networksDiagInfo) {
                if ('ChildTable' in diagOfNode) {
                    // Link bewtwen routers
                    for (linkNode of diagOfNode['Route']['RouteData']) {
                        rloc = ( parseInt(linkNode['RouteId'],16) << 10).toString(16);
                        if (rloc in nodeMap) {
                            dist = nodeMap[rloc];
                            if (src < dist) {
                                $scope.graphInfo.links.push({
                                    'source': src,
                                    'target': dist,
                                    'linkInfo': {
                                        'inQuality': linkNode['LinkQualityIn'],
                                        'outQuality': linkNode['LinkQualityOut']
                                    }
                                });
                            }
                        }
                    }

                    // Link between router and child
                    for (childInfo of diagOfNode['ChildTable']) {
                        child = {};
                        rlocOfParent = parseInt(diagOfNode['Rloc16'],16).toString(16);
                        rlocOfChild = (parseInt(diagOfNode['Rloc16'],16) + childInfo['ChildId']).toString(16);

                        src = nodeMap[rlocOfParent];
                            
                        child['Rloc16'] = '0x' +  $scope.intToHexString(parseInt(diagOfNode['Rloc16'],16) + childInfo['ChildId'],4);
                        child['RouteId'] = diagOfNode['RouteId'];
                        nodeMap[rlocOfChild] = count;
                        child['Role'] = 'Child';
                        $scope.graphInfo.nodes.push(child);
                        $scope.graphInfo.links.push({
                            'source': src,
                            'target': count,
                            'linkInfo': {
                                'Timeout': childInfo['Timeout'],
                                'Mode': childInfo['Mode']
                            }
                        });
                        count = count + 1;
                    }
                }
                src = src + 1;
            }
            $scope.drawGraph();
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
 
        $scope.drawGraph = function() {
            var json, svg, tooltip, force;
            var scale, len;

            document.getElementById('topograph').innerHTML = '';
            scale = $scope.graphInfo.nodes.length;
            len = 125 * Math.sqrt(scale);

            // Topology graph
            svg = d3.select('.d3graph')
                    .append('svg')
                    .attr('preserveAspectRatio', 'xMidYMid meet')
                    .attr('viewBox', '0, 0, ' + len.toString(10) + ', ' + (len / (3 / 2)).toString(10));
            
            // Legend
            svg.append('circle')
                .attr('cx',len-20)
                .attr('cy',10).attr('r', 3)
                .style('fill', '#7e77f8')
                .style('stroke', '#484e46')
                .style('stroke-width', '0.4px');
            
            svg.append('circle')
                .attr('cx',len-20)
                .attr('cy',20)
                .attr('r', 3)
                .style('fill', '#03e2dd')
                .style('stroke', '#484e46')
                .style('stroke-width', '0.4px');
            
            svg.append('circle')
                .attr('cx',len-20)
                .attr('cy',30)
                .attr('r', 3)
                .style('fill', '#aad4b0')
                .style('stroke', '#484e46')
                .style('stroke-width', '0.4px')
                .style('stroke-dasharray','2 1');
           
            svg.append('circle')
                .attr('cx',len-50)
                .attr('cy',10).attr('r', 3)
                .style('fill', '#ffffff')
                .style('stroke', '#f39191')
                .style('stroke-width', '0.4px');
            
            svg.append('text')
                .attr('x', len-15)
                .attr('y', 10)
                .text('Leader')
                .style('font-size', '4px')
                .attr('alignment-baseline','middle');
            
            svg.append('text')
                .attr('x', len-15)
                .attr('y',20 )
                .text('Router')
                .style('font-size', '4px')
                .attr('alignment-baseline','middle');
            
            svg.append('text')
                .attr('x', len-15)
                .attr('y',30 )
                .text('Child')
                .style('font-size', '4px')
                .attr('alignment-baseline','middle');
            svg.append('text')
                .attr('x', len-45)
                .attr('y',10 )
                .text('Selected')
                .style('font-size', '4px')
                .attr('alignment-baseline','middle');

            // Tooltip style for each node
            tooltip = d3.select('body')
                .append('div')
                .attr('class', 'tooltip')
                .style('position', 'absolute')
                .style('z-index', '10')
                .style('visibility', 'hidden')
                .text('a simple tooltip');

            force = d3.layout.force()
                .distance(40)
                .size([len, len / (3 / 2)]);

            
            json = $scope.graphInfo;
 
            force
                .nodes(json.nodes)
                .links(json.links)
                .start();


            var link = svg.selectAll('.link')
                .data(json.links)
                .enter().append('line')
                .attr('class', 'link')
                .style('stroke', '#908484')
                // Dash line for link between child and parent
                .style('stroke-dasharray', function(item) {
                    if ('Timeout' in item.linkInfo) return '4 4';
                    else return '0 0'
                })
                // Line width representing link quality
                .style('stroke-width', function(item) {
                    if ('inQuality' in item.linkInfo)
                        return Math.sqrt(item.linkInfo.inQuality/2);
                    else return Math.sqrt(0.5)
                })
                // Tooltip effect of mouseover on a line
                .on('mouseover', function(item) {
                    return tooltip.style('visibility', 'visible')
                        .text(item.linkInfo);
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
                    return item.Role;
                })
                .call(force.drag)
                // Tooltip effect of mouseover on a node
                .on('mouseover', function(item) {
                    return tooltip.style('visibility', 'visible')
                                  .text(item.Rloc16 );
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
                    return item.Rloc16;
                })
                .on('mouseover', function(item) {
                    return tooltip.style('visibility', 'visible')
                                  .text(item.Rloc16 );
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
                .attr('class', 'Stroke')
                // Effect that node will become bigger when mouseover
                .on('mouseover', function(item) {
                    d3.select(this)
                        .transition()
                        .attr('r','9');
                    return tooltip.style('visibility', 'visible')
                                  .text(item.Rloc16);
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
                        $scope.updateDetailLabel();
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
                                  .text(item.Rloc16);
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
                        $scope.updateDetailLabel();
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
            

            $scope.updateDetailLabel();
            

        }
    };
})();
