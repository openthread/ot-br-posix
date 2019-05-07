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
            
            ev.path[0].disabled = true;
            
            httpRequest.then(function successCallback(response) {
                if (response.data.error == 0) {
                    $scope.showAlert(event, 'Commission', 'success');
                } else {
                    $scope.showAlert(event, 'Commission', 'failed');
                }
                ev.path[0].disabled = false;
            });
        };
    };
})();
