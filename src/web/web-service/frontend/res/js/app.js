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
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS'
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
            passphrase: '123456',
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
        $scope.showPanels = function(index) {
            $scope.headerTitle = $scope.menu[index].title;
            for (var i = 0; i < 6; i++) {
                $scope.menu[i].show = false;
            }
            $scope.menu[index].show = true;
            if (index == 1) {
                $scope.isLoading = true;
                $http.get('/available_network').then(function(response) {
                    $scope.isLoading = false;
                    if (response.data.error == 0) {
                        $scope.networksInfo = response.data.result;
                    } else {
                        $scope.showScanAlert(event);
                    }
                });
            }
            if (index == 3) {
                $http.get('/get_properties').then(function(response) {
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
                $scope.dataInit();
                $scope.showTopology();
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
                networkKey: '00112233445566778899aabbccddeeff',
                prefix: 'fd11:22::',
                defaultRoute: true,
            };

            $scope.showAlert = function(ev, result) {
                $mdDialog.show(
                    $mdDialog.alert()
                    .parent(angular.element(document.querySelector('#popupContainer')))
                    .clickOutsideToClose(true)
                    .title('Information')
                    .textContent('Join operation is ' + result)
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
                    networkKey: $scope.thread.networkKey,
                    prefix: $scope.thread.prefix,
                    defaultRoute: $scope.thread.defaultRoute,
                    index: index,
                };
                var httpRequest = $http({
                    method: 'POST',
                    url: '/join_network',
                    data: data,
                });

                data = {
                    extPanId: networkInfo.xp,
                    networkName: networkInfo.nn,
                };
                httpRequest.then(function successCallback(response) {
                    $scope.res = response.data.result;
                    if (response.data.result == 'successful') {
                        $mdDialog.hide();
                    }
                    $scope.isDisplay = false;
                    $scope.showAlert(event, response.data.result);
                });
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
                    url: '/form_network',
                    data: data,
                });

                data = {
                    extPanId: $scope.thread.extPanId,
                    networkName: $scope.thread.networkName,
                };
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
                    url: '/add_prefix',
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
                    url: '/delete_prefix',
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
                url: '/commission',
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

        $scope.restServerPort = '81';
        $scope.ipaddr = location.hostname + ':' + $scope.restServerPort;

        //basic information line
        $scope.networksInfo = 'Unknown';
        //num of router calculated by diagnostic
        $scope.numOfRouter = 'Unknown';

        //diagnostic information for detailed display
        $scope.nodeDetailInfo = 'Unknown';
        //for response of Diagnostic
        $scope.networksDiagInfo = '';
        $scope.graphisReady = false;
        $scope.detailList = {
            'Ext Address': { 'title': false, 'content': true },
            'Rloc16': { 'title': false, 'content': true },
            'Mode': { 'title': false, 'content': false },
            'Connectivity': { 'title': false, 'content': false },
            'Route': { 'title': false, 'content': false },
            'Leader Data': { 'title': false, 'content': false },
            'Network Data': { 'title': false, 'content': true },
            'IP6 Address List': { 'title': false, 'content': true },
            'MAC Counters': { 'title': false, 'content': false },
            'Child Table': { 'title': false, 'content': false },
            'Channel Pages': { 'title': false, 'content': false }
        };
        $scope.graphInfo = {
            'nodes': [],
            'links': []
        }

        $scope.dataInit = function() {

            $http.get('http://' + $scope.ipAddr + '/node').then(function(response) {

                $scope.networksInfo = response.data;

            });
        }
        $scope.isObject = function(v) {
            return v.constructor === Object;
        }
        $scope.isArray = function(v) {
            return !!v && v.constructor === Array;
        }

        $scope.clickList = function(key) {
            $scope.detailList[key]['content'] = !$scope.detailList[key]['content']
        }

        $scope.nodeChoose = function() {
            const hide = e => e.style.display = 'none';
            const show = e => e.style.display = ''
            document.querySelectorAll('#Leader').forEach(e => e.style.display ? show(e) : hide(e))
        }
        $scope.showTopology = function() {
            //record index of all node including child,leader and router
            var nodeMap = {}
            var count, src, dist, rloc, leaderid, routeid, child, rlocOfParent, rlocOfChild;

            $scope.graphisReady = false;
            $scope.graphInfo = {
                'nodes': [],
                'links': []
            };
            $http.get('http://' + $scope.ipAddr + '/diagnostics').then(function(response) {


                $scope.networksDiagInfo = response.data;

                count = 0;
                for (var x of $scope.networksDiagInfo) {
                    if ('Child Table' in x) {
                        rloc = parseInt(x['Rloc16'], 16).toString(16);
                        nodeMap[rloc] = count;

                        routeid = '0x' + (parseInt(x['Rloc16'], 16) >> 10).toString(16);
                        x['RouteId'] = routeid;
                        leaderid = (parseInt(x['Leader Data']['LeaderRouterId'], 16) << 10).toString(16);
                        if (leaderid == rloc) {
                            x['Role'] = 'Leader';
                        } else {
                            x['Role'] = 'Router';
                        }

                        $scope.graphInfo.nodes.push(x);
                        if (x['Rloc16'] === $scope.networksInfo.rloc16) {
                            $scope.nodeDetailInfo = x
                        }
                        count = count + 1;
                    }
                }
                // num of Router is based on the diagnostic information
                $scope.numOfRouter = count;

                // index for a second loop
                src = 0;
                // construct links 
                for (var y of $scope.networksDiagInfo) {
                    if ('Child Table' in y) {
                        // link bewtwen routers
                        for (var z of y['Route']['RouteData']) {
                            rloc = (parseInt(z['RouteId'], 16) << 10).toString(16);
                            if (rloc in nodeMap) {
                                dist = nodeMap[rloc];
                                if (src < dist) {
                                    $scope.graphInfo.links.push({
                                        'source': src,
                                        'target': dist,
                                        'weight': 1,
                                        'type': 0,
                                        'linkInfo': {
                                            'inQuality': z['LinkQualityIn'],
                                            'outQuality': z['LinkQualityOut']
                                        }
                                    });
                                }
                            }
                        }

                        //link between router and child 
                        for (var n of y['Child Table']) {
                            child = {};
                            rlocOfParent = parseInt(y['Rloc16'], 16).toString(16);
                            rlocOfChild = (parseInt(y['Rloc16'], 16) + parseInt(n['ChildId'], 16)).toString(16);

                            src = nodeMap[rlocOfParent];

                            child['Rloc16'] = '0x' + rlocOfChild;
                            routeid = '0x' + (parseInt(y['Rloc16'], 16) >> 10).toString(16);
                            child['RouteId'] = routeid;

                            nodeMap[rlocOfChild] = count;
                            child['Role'] = 'Child'
                            $scope.graphInfo.nodes.push(child);
                            $scope.graphInfo.links.push({
                                'source': src,
                                'target': count,
                                'weight': 1,
                                'type': 1,
                                'linkInfo': {
                                    'Timeout': n['Timeout'],
                                    'Mode': n['Mode']
                                }

                            });

                            count = count + 1;
                        }
                    }
                    src = src + 1;
                }
                // construct graph

                $scope.drawGraph();
            })

            // need timeout handler
        }

        // update what information to show when click differrent nodes
        $scope.updateDetailLabel = function() {
            for (var x in $scope.detailList) {
                $scope.detailList[x]['title'] = false;
            }
            for (var y in $scope.nodeDetailInfo) {

                if (y in $scope.detailList) {
                    $scope.detailList[y]['title'] = true;
                }

            }
        }

        //function that draw a svg for topology
        $scope.drawGraph = function() {
            var svg, tooltip, force, filt, feMerge;
            var scale, len;

            //erase former graph
            document.getElementById('topograph').innerHTML = '';
            scale = $scope.graphInfo.nodes.length;
            len = 125 * Math.sqrt(scale);

            //topology graph
            svg = d3.select('.d3graph').append('svg')
                .attr('preserveAspectRatio', 'xMidYMid meet')
                .attr('viewBox', '0, 0, ' + len.toString(10) + ', ' + (len / (3 / 2)).toString(10)) //class to make it responsive

            //tooltip style  for each node
            tooltip = d3.select('body')
                .append('div')
                .attr('class', 'tooltip')
                .style('position', 'absolute')
                .style('z-index', '10')
                .style('visibility', 'hidden')
                .text('a simple tooltip');

            force = d3.layout.force()
                .gravity(.05)

            //line length
            .distance(100)
                .charge(-100)
                .size([len, len / (3 / 2)]);

            //blur for node
            filt = svg.append('defs')
                .append('filter')
                .attr({ id: 'f1', x: 0, y: 0, width: '150%', height: '150%' });
            filt
                .append('feOffset')
                .attr({ result: 'offOut', 'in': 'sourceAlpha', dx: 1, dy: 1 });
            filt
                .append('feGaussianBlur')
                .attr({ result: 'blurOut', 'in': 'offOut', stdDeviation: 1 });
            feMerge = filt.append('feMerge');
            feMerge
                .append('feMergeNode')
                .attr('in', 'offsetBlur');
            feMerge
                .append('feMergeNode')
                .attr('in', 'SourceGraphic');


            d3.json('/static/data1.json', function(json) {
                var pentagonPoints;
                json = $scope.graphInfo;
                force
                    .nodes(json.nodes)
                    .links(json.links)
                    .start();


                var link = svg.selectAll('.link')
                    .data(json.links)
                    .enter().append('line')
                    .attr('class', 'link')
                    .style('stroke', '#18c3f7')
                    // dash line for link between child and parent
                    .style('stroke-dasharray', function(d) {
                        if ('Timeout' in d.linkInfo) return '5 5';
                        else return '0 0'
                    })
                    // line width representing link quality
                    .style('stroke-width', function(d) {
                        if ('inQuality' in d.linkInfo)
                            return Math.sqrt(d.linkInfo.inQuality);
                        else return Math.sqrt(0.5)
                    })

                // effect on mouseover
                .on('mouseover', function(d) {
                        return tooltip.style('visibility', 'visible')
                            .text(d.linkInfo);
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
                    .attr('class', function(d) {
                        return d.Role;
                    })
                    .call(force.drag)
                    .on('mouseover', function(d) {
                        return tooltip.style('visibility', 'visible')
                            .text('RLOC16: ' + d.Rloc16 + ' RouteId:  ' + d.RouteId);
                    })
                    .on('mousemove', function() {
                        return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                            .style('left', (d3.event.pageX + 10) + 'px');
                    })
                    .on('mouseout', function() {
                        return tooltip.style('visibility', 'hidden');
                    });

                d3.selectAll('.Child').append('circle')
                    .attr('r', '10')
                    .attr('fill', '#18c3f7')
                    .attr('class', function(d) {
                        return d.Rloc16;
                    })
                    .on('mouseover', function(d) {
                        return tooltip.style('visibility', 'visible')
                            .text('RLOC16: ' + d.Rloc16 + ' RouteId:  ' + d.RouteId);
                    })
                    .on('mousemove', function() {
                        return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                            .style('left', (d3.event.pageX + 10) + 'px');
                    })
                    .on('mouseout', function() {
                        return tooltip.style('visibility', 'hidden');
                    });


                d3.selectAll('.Leader').append('polygon')
                    .attr('points', '-7.5,10.44 7.5,10.44 12.12,-3.936 0,-12.63 -12.12,-3.936')
                    .attr('fill', '#000000')
                    .style('filter', 'url(#f1)')
                    .attr('class', function(d) {
                        return 'Stroke';
                    })
                    .on('mouseover', function(d) {

                        d3.select(this).attr('transform', 'scale(1.25)');
                        return tooltip.style('visibility', 'visible')
                            .text('RLOC16: ' + d.Rloc16 + ' RouteId:  ' + d.RouteId);

                    })
                    .on('mousemove', function() {
                        return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                            .style('left', (d3.event.pageX + 10) + 'px');
                    })
                    .on('mouseout', function() {
                        d3.select(this).attr('transform', 'scale(0.8)');
                        return tooltip.style('visibility', 'hidden');

                    })
                    .on('click', function(d) {
                        d3.selectAll('.Stroke').style('stroke', 'none')

                        d3.selectAll('.Stroke').style('stroke', 'none')


                        d3.select(this).style('stroke', 'yellow')
                            .style('stroke-width', '1px');


                        $scope.$apply(function() {
                            $scope.nodeDetailInfo = d;
                            $scope.updateDetailLabel();
                        });
                    });

                d3.selectAll('.Router').append('polygon')
                    .attr('points', '-7.5,10.44 7.5,10.44 12.12,-3.936 0,-12.63 -12.12,-3.936')
                    .attr('fill', '#024f9d')
                    .style('filter', 'url(#f1)')
                    .attr('class', function(d) {
                        return 'Stroke';
                    })
                    .on('mouseover', function(d) {

                        d3.select(this).attr('transform', 'scale(1.1)');
                        return tooltip.style('visibility', 'visible')
                            .text('RLOC16: ' + d.Rloc16 + ' RouteId:  ' + d.RouteId);


                    })
                    .on('mousemove', function() {
                        return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                            .style('left', (d3.event.pageX + 10) + 'px');
                    })
                    .on('mouseout', function() {
                        d3.select(this).attr('transform', 'scale(0.8)');
                        return tooltip.style('visibility', 'hidden');

                    })
                    .on('click', function(d) {
                        d3.selectAll('.Stroke').style('stroke', 'none')

                        d3.select(this).style('stroke', 'yellow')
                            .style('stroke-width', '1px');



                        $scope.$apply(function() {
                            $scope.nodeDetailInfo = d;
                            $scope.updateDetailLabel();
                        });
                    });

                force.on('tick', function() {
                    link.attr('x1', function(d) { return d.source.x; })
                        .attr('y1', function(d) { return d.source.y; })
                        .attr('x2', function(d) { return d.target.x; })
                        .attr('y2', function(d) { return d.target.y; });
                    node.attr('transform', function(d) {
                        return 'translate(' + d.x + ',' + d.y + ')';
                    });
                });
            });

            $scope.updateDetailLabel();
            $scope.graphisReady = true;



        }
    };
})();
