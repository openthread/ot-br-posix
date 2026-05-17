'use strict';
'require view';
'require rpc';
'require ui';

var callScan = rpc.declare({
	object: 'luci.openthread',
	method: 'scan'
});

function signalScale(rssi) {
	var r = Number(rssi || 0);
	if (r <= -100) return 0;
	if (r >=  -50) return 100;
	return Math.floor(2 * (100 + r));
}

function signalIcon(net) {
	var base = L.resource('icons');
	if (net.NetworkName == null) return base + '/signal-none.svg';
	var s = signalScale(net.Rssi);
	if (s < 15) return base + '/signal-000.svg';
	if (s < 35) return base + '/signal-000-025.svg';
	if (s < 55) return base + '/signal-025-050.svg';
	if (s < 75) return base + '/signal-050-075.svg';
	return base + '/signal-075-100.svg';
}

return view.extend({
	load: function() {
		return callScan();
	},

	render: function(data) {
		var nets = (data && data.scan_list) || [];

		var rows = [E('tr', { 'class': 'tr table-titles' }, [
			E('th', { 'class': 'th col-1 center' }, _('RSSI')),
			E('th', { 'class': 'th col-1 center' }, _('Channel')),
			E('th', { 'class': 'th col-2 center' }, _('PAN Id')),
			E('th', { 'class': 'th col-1 left' },   _('Lqi')),
			E('th', { 'class': 'th cbi-section-actions' }, ' '),
		])];

		if (!nets.length) {
			rows.push(E('tr', { 'class': 'tr placeholder' },
				E('td', { 'class': 'td', 'colspan': 5 },
					E('em', {}, _('No networks found')))));
		} else {
			nets.forEach(function(net, i) {
				var scale = signalScale(net.Rssi);
				rows.push(E('tr', { 'class': 'tr cbi-rowstyle-' + (1 + (i % 2)) }, [
					E('td', { 'class': 'td col-1 center' },
						E('abbr', { 'title': _('Signal: %s dB / Quality: %s').format(net.Rssi, net.Lqi) }, [
							E('img', { 'src': signalIcon(net) }), E('br'),
							E('small', {}, scale + '%')
						])),
					E('td', { 'class': 'td col-1 center' }, String(net.Channel)),
					E('td', { 'class': 'td col-2 center' }, String(net.PanId)),
					E('td', { 'class': 'td col-1 left' },   String(net.Lqi)),
					E('td', { 'class': 'td cbi-section-actions' },
						E('button', {
							'class': 'cbi-button cbi-button-action important',
							'click': ui.createHandlerFn({}, function() {
								var qs = '?PanId=' + encodeURIComponent(net.PanId)
								       + '&Channel=' + encodeURIComponent(net.Channel)
								       + '&NetworkName=' + encodeURIComponent(net.NetworkName || '');
								location.href = L.url('admin/network/thread_join') + qs;
							})
						}, _('Join Network')))
				]));
			});
		}

		return E([], [
			E('h2', {}, _('Join Network: Thread Scan')),
			E('div', { 'class': 'cbi-map' },
				E('div', { 'class': 'cbi-section' },
					E('table', { 'class': 'table' }, rows))),
			E('div', { 'class': 'cbi-page-actions right' }, [
				E('button', {
					'class': 'cbi-button cbi-button-neutral',
					'style': 'float:left',
					'click': function() { location.href = L.url('admin/network/thread'); }
				}, _('Back to overview')),
				E('button', {
					'class': 'cbi-button cbi-button-action',
					'click': function() { location.reload(); }
				}, _('Repeat scan'))
			])
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null,
});
