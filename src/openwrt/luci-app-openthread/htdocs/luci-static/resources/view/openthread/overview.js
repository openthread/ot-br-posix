'use strict';
'require view';
'require dom';
'require poll';
'require rpc';
'require ui';
'require openthread/errors as otbrErrors';

var callStateSummary = rpc.declare({
	object: 'luci.openthread',
	method: 'state_summary'
});

var callNeighborsSummary = rpc.declare({
	object: 'luci.openthread',
	method: 'neighbors_summary'
});

var callThreadStop = rpc.declare({
	object: 'luci.openthread',
	method: 'thread_stop'
});

function signalScale(linkQualityIn) {
	var lq = Number(linkQualityIn);
	if (lq == 0) return 0;
	if (lq == 1) return 30;
	if (lq == 2) return 60;
	return 100;
}

function signalIcon(scale) {
	var base = L.resource('icons');
	if (scale == 0)  return base + '/signal-000.svg';
	if (scale == 30) return base + '/signal-025-050.svg';
	if (scale == 50) return base + '/signal-050-075.svg';
	return base + '/signal-075-100.svg';
}

function renderSignal(bss) {
	var scale = signalScale(bss.LinkQualityIn);
	return E('abbr', { 'title': _('LinkQualityIn: %s').format(bss.LinkQualityIn) }, [
		E('img', { 'src': signalIcon(scale) }),
		E('br'),
		E('small', {}, ' ' + scale + '%')
	]);
}

function navigate(url) {
	location.href = L.url(url);
}

function actionButton(label, cls, onclick, title) {
	return E('button', {
		'class': cls,
		'title': title || label,
		'click': ui.createHandlerFn({}, onclick)
	}, label);
}

function renderStatusRow(st) {
	if (st.state && st.state != 'disabled') {
		var actions = E('div', {}, [
			actionButton(_('Disable'), 'cbi-button cbi-button-neutral',
				function() {
					return callThreadStop().then(function(r) {
						otbrErrors.notify(r);
						return location.reload();
					});
				}, _('Disable this network')),
			actionButton(_('Edit'), 'cbi-button cbi-button-action important',
				function() { navigate('admin/network/thread_setting'); },
				_('Edit this network')),
			actionButton(_('View'), 'cbi-button cbi-button-add',
				function() { navigate('admin/network/thread_view'); },
				_('View this network')),
		]);
		return [
			E('strong', {}, _('Network Name:') + ' '), st.networkname || '',
			' | ',
			E('strong', {}, _('State:') + ' '), st.state || '',
			E('br'),
			E('strong', {}, _('PAN ID:') + ' '), String(st.panid ?? ''),
			'   |   ',
			E('strong', {}, _('Channel:') + ' '), String(st.channel ?? ''),
			actions
		];
	}
	return [
		E('strong', {}, _('Not connected to any Thread network')),
		E('br'),
		_('Please join or create a Thread network.')
	];
}

function renderNeighborTable(st) {
	var titlesNeighbor = [_('Signal'), _('Role'), _('RLOC16'), _('Age'), _('Avg RSSI'), _('Last RSSI')];
	var titlesParent   = [_('Signal'), _('Role'), _('RLOC16'), _('Age'), _('LinkQualityIn'), _('ExtAddress')];
	var isChild = (st.state == 'child');
	var titles = isChild ? titlesParent : titlesNeighbor;

	var rows = [E('tr', { 'class': 'tr table-titles' },
		titles.map(function(t) { return E('th', { 'class': 'th center' }, t); }))];

	if (!st.neighbor || !st.neighbor.length) {
		rows.push(E('tr', { 'class': 'tr placeholder' },
			E('td', { 'class': 'td', 'colspan': titles.length },
				E('em', {}, _('No information available')))));
	} else {
		st.neighbor.forEach(function(bss) {
			var cells = isChild
				? [renderSignal(bss), bss.Role, bss.Rloc16, bss.Age, bss.LinkQualityIn, bss.ExtAddress]
				: [renderSignal(bss), bss.Role, bss.Rloc16, bss.Age, bss.AvgRssi, bss.LastRssi];
			rows.push(E('tr', { 'class': 'tr' },
				cells.map(function(c) { return E('td', { 'class': 'td center' }, c); })));
		});
	}

	return E('table', { 'class': 'table', 'id': 'neighbors' }, rows);
}

return view.extend({
	load: function() {
		return Promise.all([callStateSummary(), callNeighborsSummary()]);
	},

	render: function(data) {
		var st = data[0] || {};
		var neigh = data[1] || {};

		var statusCell = E('div', { 'id': 'thread-status' }, renderStatusRow(st));
		var neighborsBox = E('div', { 'id': 'thread-neighbors' }, renderNeighborTable(neigh));

		var view = E([], [
			E('h2', {}, _('Thread Overview')),
			E('div', { 'class': 'cbi-section-node' }, [
				E('div', { 'class': 'table' }, [
					E('div', { 'class': 'tr cbi-rowstyle-2' }, [
						E('div', { 'class': 'td col-1 center middle' },
							E('span', { 'class': 'ifacebadge' }, [
								E('img', { 'src': L.resource('icons/wifi.svg') }),
								' ',
								st.interfacename || ''
							])),
						E('div', { 'class': 'td col-7 left middle' },
							E('big', {}, E('strong', {}, _('Generic MAC 802.15.4 Thread')))),
						E('div', { 'class': 'td middle cbi-section-actions' }, E('div', {}, [
							actionButton(_('Scan'), 'cbi-button cbi-button-action important',
								function() { navigate('admin/network/thread_scan'); },
								_('Scan and join network')),
							actionButton(_('Create'), 'cbi-button cbi-button-add',
								function() { navigate('admin/network/thread_setting'); },
								_('Create new network'))
						]))
					]),
					E('div', { 'class': 'tr cbi-rowstyle-1' }, [
						E('div', { 'class': 'td col-1 center middle' }),
						E('div', { 'class': 'td col-7 left middle' }, statusCell),
						E('div', { 'class': 'td middle cbi-section-actions' })
					])
				])
			]),
			E('br'),
			E('h2', {}, _('Neighbors')),
			E('br'),
			neighborsBox
		]);

		poll.add(L.bind(function() {
			return Promise.all([callStateSummary(), callNeighborsSummary()]).then(function(d) {
				dom.content(statusCell, renderStatusRow(d[0] || {}));
				dom.content(neighborsBox, renderNeighborTable(d[1] || {}));
			});
		}, this), 2);

		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null,
});
