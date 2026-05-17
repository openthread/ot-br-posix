'use strict';
'require view';
'require rpc';
'require ui';
'require openthread/errors as otbrErrors';

var callGetNetworkkey = rpc.declare({
	object: 'luci.openthread',
	method: 'get_networkkey'
});

var callAttach = rpc.declare({
	object: 'luci.openthread',
	method: 'attach',
	params: [ 'panid', 'channel', 'networkkey' ]
});

function getQuery(name) {
	var p = new URLSearchParams(location.search);
	return p.get(name) || '';
}

return view.extend({
	load: function() {
		return callGetNetworkkey();
	},

	render: function(data) {
		var nk = (data && data.networkkey) || '';
		var panid   = getQuery('PanId');
		var channel = getQuery('Channel');
		var name    = getQuery('NetworkName');

		var keyInput = E('input', {
			'type': 'text',
			'name': 'networkkey',
			'value': nk,
			'style': 'width:50%'
		});

		function submit() {
			var key = keyInput.value;
			if (key.length != 32) {
				ui.addNotification(null, E('p', _('Network key length must be 32 hex characters')), 'danger');
				return;
			}
			return callAttach(panid, parseInt(channel), key).then(function(r) {
				otbrErrors.notify(r);
				if (!r || !r.error)
					location.href = L.url('admin/network/thread');
			});
		}

		return E([], [
			E('h2', {}, _('Join Network: %s').format(name)),
			E('div', { 'style': 'width:80%;margin-left:20%' }, [
				E('span', {}, E('strong', {}, _('Network Key '))),
				E('span', { 'style': 'margin-left:3%' }),
				keyInput,
				E('div', { 'style': 'margin-left:13%;margin-top:1%' },
					E('span', {}, _('Please input your network key to ensure you join the right network.')))
			]),
			E('br'), E('br'), E('br'),
			E('div', { 'class': 'cbi-page-actions right' }, [
				E('button', {
					'class': 'cbi-button cbi-button-neutral',
					'style': 'float:left',
					'click': function() { location.href = L.url('admin/network/thread_scan'); }
				}, _('Back to scan result')),
				E('button', {
					'class': 'cbi-button cbi-button-add important',
					'click': ui.createHandlerFn({}, submit)
				}, _('Attach'))
			])
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null,
});
