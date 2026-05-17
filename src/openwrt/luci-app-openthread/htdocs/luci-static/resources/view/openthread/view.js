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

var callJoinerRemove = rpc.declare({
	object: 'luci.openthread',
	method: 'joiner_remove',
	params: [ 'eui64' ]
});

function transRole(r) {
	if (r == 'C') return _('Child');
	if (r == 'R') return _('Router');
	return _('Pending');
}

function renderLeaderTable(st) {
	var ld = (st && st.leaderdata) || {};
	return E('table', { 'class': 'table' }, [
		E('tr', { 'class': 'tr table-titles', 'style': 'background-color:#eee' }, [
			E('th', { 'class': 'th col-3 center' }, _('Leader Router Id')),
			E('th', { 'class': 'th col-3 center' }, _('Partition Id')),
			E('th', { 'class': 'th col-2 center' }, _('Weighting')),
			E('th', { 'class': 'th col-2 center' }, _('Data Version')),
			E('th', { 'class': 'th col-2 center' }, _('Stable Data Version')),
		]),
		E('tr', { 'class': 'tr cbi-rowstyle-2', 'style': 'border:solid 1px #ddd;border-top:hidden' }, [
			E('td', { 'class': 'td col-3 center' }, String(ld.LeaderRouterId ?? '')),
			E('td', { 'class': 'td col-3 center' }, String(ld.PartitionId ?? '')),
			E('td', { 'class': 'td col-2 center' }, String(ld.Weighting ?? '')),
			E('td', { 'class': 'td col-2 center' }, String(ld.DataVersion ?? '')),
			E('td', { 'class': 'td col-2 center' }, String(ld.StableDataVersion ?? '')),
		])
	]);
}

function removeJoinerHandler(eui64) {
	return function() {
		return callJoinerRemove(eui64).then(function(r) {
			otbrErrors.notify(r);
			return r;
		});
	};
}

function joinerActionsCell(j) {
	var eui64 = j.isAny ? '*' : j.eui64;
	return E('div', { 'class': 'th cbi-section-actions' },
		E('button', {
			'class': 'cbi-button cbi-button-reset',
			'click': ui.createHandlerFn({}, removeJoinerHandler(eui64))
		}, _('Remove')));
}

function renderNeighborsTable(neigh) {
	var isChild = (neigh.state == 'child');
	var titles = isChild
		? [_('RLOC16'), _('Role'), _('Age'), _('LinkQualityIn'), _('ExtAddress'), ' ']
		: [_('RLOC16'), _('Role'), _('Age'), _('Avg RSSI'), _('Last RSSI'), _('Mode'), _('Extended MAC'), ' '];

	var rows = [E('tr', { 'class': 'tr table-titles', 'style': 'background-color:#eee' },
		titles.map(function(t) { return E('th', { 'class': 'th center' }, t); }))];

	(neigh.neighbor || []).forEach(function(bss) {
		var cells = isChild
			? [bss.Rloc16, transRole(bss.Role), bss.Age, bss.LinkQualityIn, bss.ExtAddress]
			: [bss.Rloc16, transRole(bss.Role), bss.Age, bss.AvgRssi, bss.LastRssi, bss.Mode, bss.ExtAddress];
		rows.push(E('tr', { 'class': 'tr' }, [].concat(
			cells.map(function(c) { return E('td', { 'class': 'td center' }, String(c ?? '')); }),
			[E('td', { 'class': 'td center' })]
		)));
	});

	for (var i = 0; i < (neigh.joinernum || 0); i++) {
		var j = (neigh.joinerlist || [])[i] || {};
		var pendingCols = isChild ? 4 : 6;
		var cells = [];
		cells.push(E('td', { 'class': 'td center' }, _('Pending')));
		cells.push(E('td', { 'class': 'td center' }, _('New Joiner')));
		for (var k = 2; k < pendingCols; k++)
			cells.push(E('td', { 'class': 'td center' }, _('Pending')));
		cells.push(E('td', { 'class': 'td center' }, j.isAny ? '*' : (j.eui64 || '')));
		cells.push(joinerActionsCell(j));
		rows.push(E('tr', { 'class': 'tr' }, cells));
	}

	if (rows.length == 1) {
		rows.push(E('tr', { 'class': 'tr placeholder' },
			E('td', { 'class': 'td', 'colspan': titles.length },
				E('em', {}, _('No information available')))));
	}

	return E('table', { 'class': 'table', 'style': 'width:90%;margin-left:5%' }, rows);
}

return view.extend({
	load: function() {
		return Promise.all([callStateSummary(), callNeighborsSummary()]);
	},

	render: function(data) {
		var st = data[0] || {};
		var neigh = data[1] || {};

		var leaderBox    = E('div', { 'class': 'cbi-map', 'style': 'width:90%;margin-left:5%' },
			E('div', { 'class': 'cbi-section' }, renderLeaderTable(st)));
		var neighborsBox = E('div', {}, renderNeighborsTable(neigh));

		var v = E([], [
			E('h2', {}, _('Thread View: %s (wpan0)').format(st.networkname || '')),
			E('div', {}, _('This is the list of nodes in your Thread network.')),
			E('br'),
			E('h3', {}, _('Leader Situation of Network')), E('br'),
			leaderBox, E('br'),
			E('h3', {}, _('Neighbor Situation of Network')), E('br'),
			neighborsBox,
			E('div', { 'class': 'cbi-page-actions right', 'style': 'margin-top:10%' }, [
				E('button', {
					'class': 'cbi-button cbi-button-neutral',
					'click': function() { location.href = L.url('admin/network/thread'); }
				}, _('Back to overview')),
				E('button', {
					'class': 'cbi-button cbi-button-add',
					'click': function() { location.href = L.url('admin/network/thread_add'); }
				}, _('Add')),
			])
		]);

		poll.add(function() {
			return Promise.all([callStateSummary(), callNeighborsSummary()]).then(function(d) {
				dom.content(leaderBox, E('div', { 'class': 'cbi-section' }, renderLeaderTable(d[0] || {})));
				dom.content(neighborsBox, renderNeighborsTable(d[1] || {}));
			});
		}, 2);

		return v;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null,
});
